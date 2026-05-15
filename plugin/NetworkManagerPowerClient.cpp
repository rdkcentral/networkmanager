/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#ifdef ENABLE_POWERMANAGER

#include "NetworkManagerPowerClient.h"
#include "NetworkManagerLogger.h"
#include <com/com.h>

using namespace WPEFramework;
using namespace WPEFramework::Exchange;
using namespace WPEFramework::Plugin;

// ---------------------------------------------------------------------------
// NetworkManagerPowerClient
// ---------------------------------------------------------------------------

NetworkManagerPowerClient::NetworkManagerPowerClient(INetworkPowerCallback& callback)
    : mCallback(callback)
    , mPreChangeNotification(*this)
    , mChangedNotification(*this)
{
    NMLOG_INFO("NetworkManagerPowerClient: connecting to PowerManager");
    if (auto r = Open(RPC::CommunicationTimeOut, Connector(), "org.rdk.PowerManager"); r == Core::ERROR_NONE) {
        // Connected; Operational() will be called by the framework when the proxy is ready
    } else {
        NMLOG_ERROR("NetworkManagerPowerClient: failed to open link to PowerManager (error %u)", r);
    }
}

NetworkManagerPowerClient::~NetworkManagerPowerClient()
{
    NMLOG_INFO("NetworkManagerPowerClient: shutting down");
    // Stop the power-event thread first so any in-flight work completes
    // before we release the COM-RPC proxy.
    mStopThread = true;
    mQueueCv.notify_one();
    if (mPowerThread.joinable()) {
        mPowerThread.join();
    }
    Close(Core::infinite);
    unregisterEventsAndDeactivate();
}

bool NetworkManagerPowerClient::IsValid() const
{
    return mPowerManager != nullptr;
}

bool NetworkManagerPowerClient::getNetworkStandbyMode() const
{
    bool standbyMode = false;
    if (IsValid()) {
        if (auto r = mPowerManager->GetNetworkStandbyMode(standbyMode); r != Core::ERROR_NONE) {
            NMLOG_ERROR("NetworkManagerPowerClient: GetNetworkStandbyMode failed (%u)", r);
        }
    }
    return standbyMode;
}

void NetworkManagerPowerClient::sendPowerModePreChangeComplete(int transactionId)
{
    if (IsValid()) {
        NMLOG_INFO("NetworkManagerPowerClient: sending PowerModePreChangeComplete for transactionId=%d, mClientId=%u", transactionId, mClientId);
        mPowerManager->PowerModePreChangeComplete(mClientId, transactionId);
    }
}

void NetworkManagerPowerClient::sendDelayPowerModeChange(int transactionId, int seconds)
{
    if (IsValid()) {
        if (auto r = mPowerManager->DelayPowerModeChangeBy(mClientId, transactionId, seconds); r != Core::ERROR_NONE) {
            NMLOG_ERROR("NetworkManagerPowerClient: DelayPowerModeChangeBy failed (%u)", r);
        }
    }
}

void NetworkManagerPowerClient::Operational(bool upAndRunning)
{
    NMLOG_INFO("NetworkManagerPowerClient::Operational(%s)", upAndRunning ? "true" : "false");
    if (upAndRunning) {
        if (!IsValid()) {
            mPowerManager = Interface();
            registerEvents();
            // Start the dedicated power-event thread after registration so it
            // is ready to handle events as soon as they can arrive.
            mStopThread = false;
            mPowerThread = std::thread(&NetworkManagerPowerClient::powerThreadLoop, this);
        }
    } else {
        // Stop the power-event thread before unregistering so any in-flight
        // event that was already enqueued is drained with a fast ack.
        mStopThread = true;
        mQueueCv.notify_one();
        if (mPowerThread.joinable()) {
            mPowerThread.join();
        }
        unregisterEventsAndDeactivate();
    }
}

void NetworkManagerPowerClient::registerEvents()
{
    NMLOG_INFO("NetworkManagerPowerClient: registering events");
    if (!IsValid()) {
        NMLOG_ERROR("NetworkManagerPowerClient: not in valid state, skipping event registration");
        return;
    }
    if (auto r = mPowerManager->AddPowerModePreChangeClient("org.rdk.NetworkManager", mClientId); r != Core::ERROR_NONE) {
        NMLOG_ERROR("NetworkManagerPowerClient: AddPowerModePreChangeClient failed (%u)", r);
    } else {
        NMLOG_INFO("NetworkManagerPowerClient: registered as pre-change client, mClientId=%u", mClientId);
    }
    if (auto r = mPowerManager->Register(&mPreChangeNotification); r != Core::ERROR_NONE) {
        NMLOG_ERROR("NetworkManagerPowerClient: Register(preChange) failed (%u)", r);
    }
    if (auto r = mPowerManager->Register(&mChangedNotification); r != Core::ERROR_NONE) {
        NMLOG_ERROR("NetworkManagerPowerClient: Register(changed) failed (%u)", r);
    }
}

void NetworkManagerPowerClient::unregisterEvents()
{
    NMLOG_INFO("NetworkManagerPowerClient: unregistering events");
    if (!IsValid()) {
        NMLOG_ERROR("NetworkManagerPowerClient: not in valid state, skipping event unregistration");
        return;
    }
    // NOTE: RemovePowerModePreChangeClient MUST be called before Unregister(IModePreChangeNotification)
    // per the IPowerManager API contract.
    if (auto r = mPowerManager->RemovePowerModePreChangeClient(mClientId); r != Core::ERROR_NONE) {
        NMLOG_ERROR("NetworkManagerPowerClient: RemovePowerModePreChangeClient failed (%u)", r);
    }
    if (auto r = mPowerManager->Unregister(&mPreChangeNotification); r != Core::ERROR_NONE) {
        NMLOG_ERROR("NetworkManagerPowerClient: Unregister(preChange) failed (%u)", r);
    }
    if (auto r = mPowerManager->Unregister(&mChangedNotification); r != Core::ERROR_NONE) {
        NMLOG_ERROR("NetworkManagerPowerClient: Unregister(changed) failed (%u)", r);
    }
}

void NetworkManagerPowerClient::unregisterEventsAndDeactivate()
{
    if (IsValid()) {
        unregisterEvents();
        mPowerManager->Release();
        mPowerManager = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Power event thread
// ---------------------------------------------------------------------------

void NetworkManagerPowerClient::powerThreadLoop()
{
    NMLOG_INFO("NetworkManagerPowerClient: power event thread started");
    while (true) {
        PowerEvent event{};
        {
            std::unique_lock<std::mutex> lock(mQueueMutex);
            mQueueCv.wait(lock, [this]{ return !mEventQueue.empty() || mStopThread.load(); });

            if (mStopThread) {
                // Drain remaining events with fast acks before exiting so
                // PowerManager is never left waiting on a stale transaction.
                // CHANGED events have no ack protocol — skip them.
                std::vector<PowerEvent> pending;
                while (!mEventQueue.empty()) {
                    pending.push_back(mEventQueue.front());
                    mEventQueue.pop();
                }
                lock.unlock();
                for (const auto& e : pending) {
                    if (e.type == PowerEvent::EventType::PRE_CHANGE) {
                        sendPowerModePreChangeComplete(e.transactionId);
                    }
                }
                break;
            }

            event = mEventQueue.front();
            mEventQueue.pop();
        }
        // Lock released — process event on this thread (blocking is fine here)

        if (event.type == PowerEvent::EventType::CHANGED) {
            // Wakeup notification — no ack required.
            const bool fromDeepSleep = (event.currentState == PowerState::POWER_STATE_STANDBY_DEEP_SLEEP);
            if (fromDeepSleep && event.standbyMode) {
                NMLOG_INFO("NetworkManagerPowerClient: power thread — wakeup from DeepSleep standby ON");
                mCallback.OnPowerModeChanged(event.currentState, event.newState);
            } else {
                NMLOG_INFO("NetworkManagerPowerClient: power thread — CHANGED event, no action (fromDeepSleep=%d standbyMode=%d)",
                           fromDeepSleep, event.standbyMode);
            }
            continue;
        }

        // PRE_CHANGE event processing below
        auto sendAck = [transactionId = event.transactionId, this]() {
            sendPowerModePreChangeComplete(transactionId);
        };

        const bool toDeepSleep   = (event.newState     == PowerState::POWER_STATE_STANDBY_DEEP_SLEEP);
        const bool fromDeepSleep = (event.currentState == PowerState::POWER_STATE_STANDBY_DEEP_SLEEP);

        if ((toDeepSleep || fromDeepSleep) && !event.standbyMode) {
            // Deep-sleep transition with Network Standby OFF: delegate to
            // NetworkManagerImplementation (WiFiDisconnect / reconnect) which
            // will call sendAck() when done.
            NMLOG_INFO("NetworkManagerPowerClient: power thread — %s DeepSleep standby OFF",
                       toDeepSleep ? "to" : "from");
            mCallback.OnPowerModePreChange(event.currentState, event.newState, sendAck);
        } else {
            // standby ON or non-DeepSleep: no WiFi action needed, ack immediately.
            NMLOG_INFO("NetworkManagerPowerClient: power thread — fast-path ack (standbyMode=%d toDeepSleep=%d fromDeepSleep=%d)",
                       event.standbyMode, toDeepSleep, fromDeepSleep);
            sendAck();
        }
    }
    NMLOG_INFO("NetworkManagerPowerClient: power event thread stopped");
}

// ---------------------------------------------------------------------------
// PreChangeNotification
// ---------------------------------------------------------------------------

void NetworkManagerPowerClient::PreChangeNotification::OnPowerModePreChange(
    const PowerState currentState, const PowerState newState,
    const int transactionId, const int stateChangeAfter)
{
    NMLOG_INFO("NetworkManagerPowerClient::OnPowerModePreChange current=%d new=%d txId=%d after=%ds",
               static_cast<int>(currentState), static_cast<int>(newState), transactionId, stateChangeAfter);

    // Query standby mode inline on the COM-RPC dispatcher thread — it is a
    // COM-RPC call and must not be made from the power thread.
    const bool standbyMode = mClient.getNetworkStandbyMode();

    // Cache for use by ChangedNotification
    mClient.mLastChangeStandbyMode = standbyMode;

    // Reserve a delay window now (before returning) so PowerManager knows to
    // wait at least 5 s.
    if (newState == PowerState::POWER_STATE_STANDBY_DEEP_SLEEP && !standbyMode) {
        mClient.sendDelayPowerModeChange(transactionId, 5);
    }

    // Enqueue and return immediately — the power thread does the real work.
    {
        std::lock_guard<std::mutex> lock(mClient.mQueueMutex);
        mClient.mEventQueue.push(PowerEvent{PowerEvent::EventType::PRE_CHANGE,
                                            currentState, newState, standbyMode, transactionId});
    }
    mClient.mQueueCv.notify_one();
}

// ---------------------------------------------------------------------------
// ChangedNotification
// ---------------------------------------------------------------------------

void NetworkManagerPowerClient::ChangedNotification::OnPowerModeChanged(
    const PowerState currentState, const PowerState newState)
{
    NMLOG_INFO("NetworkManagerPowerClient::OnPowerModeChanged current=%d new=%d",
               static_cast<int>(currentState), static_cast<int>(newState));

    // Use the standby mode cached
    const bool standbyMode = mClient.mLastChangeStandbyMode;

    // Enqueue and return immediately so the COM-RPC dispatcher thread is freed.
    {
        std::lock_guard<std::mutex> lock(mClient.mQueueMutex);
        mClient.mEventQueue.push(PowerEvent{PowerEvent::EventType::CHANGED,
                                            currentState, newState, standbyMode, 0});
    }
    mClient.mQueueCv.notify_one();
}

#endif // ENABLE_POWERMANAGER

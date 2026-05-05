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
    : mPreChangeNotification(callback, *this)
    , mChangedNotification(callback)
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
        // Return value intentionally ignored; a stale transactionId is harmless
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
        }
    } else {
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
// PreChangeNotification
// ---------------------------------------------------------------------------

void NetworkManagerPowerClient::PreChangeNotification::OnPowerModePreChange(
    const PowerState currentState, const PowerState newState,
    const int transactionId, const int stateChangeAfter)
{
    NMLOG_INFO("NetworkManagerPowerClient::OnPowerModePreChange current=%d new=%d txId=%d after=%ds",
               static_cast<int>(currentState), static_cast<int>(newState), transactionId, stateChangeAfter);

    auto sendAck = [transactionId, this]() {
        mClient.sendPowerModePreChangeComplete(transactionId);
    };

    if (newState == PowerState::POWER_STATE_STANDBY_DEEP_SLEEP) {
        // Transitioning TO DeepSleep: request a 5-second window so WiFiDisconnect
        // can complete before PowerManager proceeds.
        mClient.sendDelayPowerModeChange(transactionId, 5);
        mCallback.OnPowerModePreChange(currentState, newState, sendAck);
    } else if (currentState == PowerState::POWER_STATE_STANDBY_DEEP_SLEEP) {
        // Waking FROM DeepSleep: reconnect is fire-and-forget; no delay needed.
        mCallback.OnPowerModePreChange(currentState, newState, sendAck);
    } else {
        // All other transitions: fast-path ack, no network action needed.
        sendAck();
    }
}

// ---------------------------------------------------------------------------
// ChangedNotification
// ---------------------------------------------------------------------------

void NetworkManagerPowerClient::ChangedNotification::OnPowerModeChanged(
    const PowerState currentState, const PowerState newState)
{
    NMLOG_INFO("NetworkManagerPowerClient::OnPowerModeChanged current=%d new=%d",
               static_cast<int>(currentState), static_cast<int>(newState));
    mCallback.OnPowerModeChanged(currentState, newState);
}

#endif // ENABLE_POWERMANAGER

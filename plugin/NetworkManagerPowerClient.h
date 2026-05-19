/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
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

#pragma once

#include "Module.h"
#include <interfaces/IPowerManager.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace WPEFramework {
namespace Plugin {

/**
 * Callback interface that decouples NetworkManagerPowerClient from
 * NetworkManagerImplementation.  The implementation receives power state
 * transitions and must call sendAck() exactly once per OnPowerModePreChange.
 */
struct INetworkPowerCallback {
    virtual ~INetworkPowerCallback() = default;

    /**
     * Called when a power mode pre-change event arrives that involves
     * POWER_STATE_STANDBY_DEEP_SLEEP (either as the new state or the current
     * state).  The implementation MUST call sendAck() exactly once.
     */
    virtual void OnPowerModePreChange(const Exchange::IPowerManager::PowerState currentState,
                                      const Exchange::IPowerManager::PowerState newState,
                                      std::function<void()> sendAck) = 0;

    /**
     * Called when a power mode changed event arrives (informational only;
     * no ack required).
     */
    virtual void OnPowerModeChanged(const Exchange::IPowerManager::PowerState currentState,
                                    const Exchange::IPowerManager::PowerState newState) = 0;
};

/**
 * Thunder COMRPC client to the PowerManager plugin.
 *
 * Follows the same pattern as Mediarite's PowerManagerPluginClient:
 *   - Inherits SmartInterfaceType<IPowerManager> for automatic
 *     reconnect / Operational() lifecycle callbacks.
 *   - Registers as an AddPowerModePreChangeClient so it participates
 *     in the pre-change ack protocol.
 *   - Delegates DeepSleep transitions to INetworkPowerCallback.
 *   - Sends a fast-path PowerModePreChangeComplete for all other transitions.
 *
 * Lifecycle:
 *   Construction  → Open() connects to PowerManager (async).
 *   Operational(true)  → registers events; IsValid() returns true.
 *   Operational(false) → unregisters events and releases proxy.
 *   Destruction   → Close() then unregisterEventsAndDeactivate().
 */
class NetworkManagerPowerClient : protected RPC::SmartInterfaceType<Exchange::IPowerManager> {
public:
    using PowerState = Exchange::IPowerManager::PowerState;

    explicit NetworkManagerPowerClient(INetworkPowerCallback& callback);
    ~NetworkManagerPowerClient() override;

    NetworkManagerPowerClient(const NetworkManagerPowerClient&) = delete;
    NetworkManagerPowerClient& operator=(const NetworkManagerPowerClient&) = delete;

    /** Returns true when the PowerManager COMRPC proxy is available. */
    bool IsValid() const;

    /** Queries the current Network Standby mode from PowerManager. */
    bool getNetworkStandbyMode() const;

    /** Sends PowerModePreChangeComplete to PowerManager; ignores return value. */
    void sendPowerModePreChangeComplete(int transactionId);

    /** Requests a delay window extension via DelayPowerModeChangeBy. */
    void sendDelayPowerModeChange(int transactionId, int seconds);

private:
    // -----------------------------------------------------------------------
    // IModePreChangeNotification sink
    // -----------------------------------------------------------------------
    class PreChangeNotification : public Exchange::IPowerManager::IModePreChangeNotification {
    public:
        explicit PreChangeNotification(NetworkManagerPowerClient& client)
            : mClient(client) {}

        void OnPowerModePreChange(const PowerState currentState, const PowerState newState,
                                  const int transactionId, const int stateChangeAfter) override;

        BEGIN_INTERFACE_MAP(PreChangeNotification)
        INTERFACE_ENTRY(Exchange::IPowerManager::IModePreChangeNotification)
        END_INTERFACE_MAP

    private:
        NetworkManagerPowerClient& mClient;
    };

    // -----------------------------------------------------------------------
    // IModeChangedNotification sink
    // -----------------------------------------------------------------------
    class ChangedNotification : public Exchange::IPowerManager::IModeChangedNotification {
    public:
        explicit ChangedNotification(NetworkManagerPowerClient& client) : mClient(client) {}

        void OnPowerModeChanged(const PowerState currentState, const PowerState newState) override;

        BEGIN_INTERFACE_MAP(ChangedNotification)
        INTERFACE_ENTRY(Exchange::IPowerManager::IModeChangedNotification)
        END_INTERFACE_MAP

    private:
        NetworkManagerPowerClient& mClient;
    };

    // -----------------------------------------------------------------------
    // SmartInterfaceType lifecycle callback
    // -----------------------------------------------------------------------
    void Operational(bool upAndRunning) override;

    void registerEvents();
    void unregisterEvents();
    void unregisterEventsAndDeactivate();

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------
    INetworkPowerCallback&            mCallback;
    Exchange::IPowerManager*          mPowerManager{nullptr};
    Core::Sink<PreChangeNotification> mPreChangeNotification;
    Core::Sink<ChangedNotification>   mChangedNotification;
    uint32_t                          mClientId{0};

    // Power-event thread: receives events enqueued by the COM-RPC dispatcher
    // thread and processes them (WiFiDisconnect etc.) without blocking the
    // dispatcher.
    struct PowerEvent {
        enum class EventType { PRE_CHANGE, CHANGED };

        EventType  type;
        PowerState currentState;
        PowerState newState;
        bool       standbyMode;
        int        transactionId;
    };

    std::thread                       mPowerThread;
    std::queue<PowerEvent>            mEventQueue;
    std::mutex                        mQueueMutex;
    std::condition_variable           mQueueCv;
    std::atomic<bool>                 mStopThread{false};

    // Cached standby mode from the last PRE_CHANGE event.
    bool                              mLastChangeStandbyMode{false};

    void powerThreadLoop();
};

} // namespace Plugin
} // namespace WPEFramework

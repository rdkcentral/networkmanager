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
#include "INetworkManager.h"
#include <interfaces/IConnectivityCheck.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace WPEFramework {
namespace Plugin {

/**
 * COM-RPC client that delegates internet-connectivity queries to the
 * ConnectivityCheckMgr plugin (org.rdk.ConnectivityCheckMgr).
 *
 * Because the delegation lives in the out-of-process implementation, both the
 * JSON-RPC surface (shell -> implementation) and direct COM-RPC callers of
 * INetworkManager::IsConnectedToInternet / GetCaptivePortalURI are served
 * consistently.
 *
 * Mirrors NetworkManagerPowerClient:
 *   - Inherits SmartInterfaceType<IConnectivityCheck> for automatic
 *     connect / reconnect and Operational() lifecycle callbacks.
 *
 * Lifecycle:
 *   Construction       -> Open() connects to ConnectivityCheckMgr (async).
 *   Operational(true)  -> acquires the proxy; IsValid() returns true.
 *   Operational(false) -> releases the proxy.
 *   Destruction        -> Close().
 *
 * All queries fall back to INTERNET_UNKNOWN / empty when the plugin is not
 * available, so callers never crash on a boot-order race or NM/CCM restart.
 */
class NetworkManagerConnectivityClient : protected RPC::SmartInterfaceType<Exchange::IConnectivityCheck> {
public:
    using NmInternetStatus = Exchange::INetworkManager::InternetStatus;
    using InternetStatusChangeHandler = std::function<void(NmInternetStatus)>;

    NetworkManagerConnectivityClient();
    explicit NetworkManagerConnectivityClient(Exchange::IConnectivityCheck* connectivity);
    ~NetworkManagerConnectivityClient() override;

    NetworkManagerConnectivityClient(const NetworkManagerConnectivityClient&) = delete;
    NetworkManagerConnectivityClient& operator=(const NetworkManagerConnectivityClient&) = delete;

    /** Returns true when the ConnectivityCheckMgr COMRPC proxy is available. */
    bool IsValid() const;

    /** Status for an application IsConnectedToInternet query; promotes a pending status after threshold expiry. */
    NmInternetStatus getInternetStateForQuery(std::string& reason);

    /** Delegated captive-portal URI (empty when unavailable / not captive). */
    std::string getCaptivePortalURI();

    /** Sets/clears callback invoked when ConnectivityCheckMgr publishes internet-status changes. */
    void SetInternetStatusChangeHandler(InternetStatusChangeHandler handler);

private:
    class Notification : public Exchange::IConnectivityCheck::INotification {
    public:
        explicit Notification(NetworkManagerConnectivityClient& parent)
            : mParent(parent) {}

        void OnInternetStatusChange(const Exchange::IConnectivityCheck::InternetStatus status,
                                    const string& reason) override;

        BEGIN_INTERFACE_MAP(Notification)
        INTERFACE_ENTRY(Exchange::IConnectivityCheck::INotification)
        END_INTERFACE_MAP

    private:
        NetworkManagerConnectivityClient& mParent;
    };

    class LimitedInternetNotification : public Exchange::IConnectivityCheck::ILimitedInternetNotification {
    public:
        explicit LimitedInternetNotification(NetworkManagerConnectivityClient& parent)
            : mParent(parent) {}

        void OnLimitedInternetThresholdExpired() override;

        BEGIN_INTERFACE_MAP(LimitedInternetNotification)
        INTERFACE_ENTRY(Exchange::IConnectivityCheck::ILimitedInternetNotification)
        END_INTERFACE_MAP

    private:
        NetworkManagerConnectivityClient& mParent;
    };

    // SmartInterfaceType lifecycle callback.
    void Operational(bool upAndRunning) override;

    void registerEvents();
    void unregisterEvents();
    void notifyInternetStatusChanged(Exchange::IConnectivityCheck::InternetStatus status,
                                     const std::string& reason);
    void notifyLimitedInternetThresholdExpired();
    void openThreadLoop();
    NmInternetStatus queryInternetState(std::string& reason, bool& succeeded);

    /* Returns an AddRef'd proxy (nullptr when unavailable) so the caller can invoke
     * ConnectivityCheckMgr without holding mLock across the blocking COM-RPC call. */
    Exchange::IConnectivityCheck* acquireInterface() const;

    // 1:1 mapping ConnectivityCheckMgr InternetStatus -> NetworkManager InternetStatus.
    static NmInternetStatus mapStatus(Exchange::IConnectivityCheck::InternetStatus status);

    mutable std::mutex               mLock;
    Exchange::IConnectivityCheck*    mConnectivity{nullptr};
    Core::Sink<Notification>         mNotification;
    Core::Sink<LimitedInternetNotification> mLimitedInternetNotification;
    InternetStatusChangeHandler      mInternetStatusChangeHandler;
    bool                             mNotificationRegistered{false}; // guarded by mLock
    uint64_t                         m_statusRevision{0};               // guarded by mLock

    // Status cache kept current by OnInternetStatusChange, so the hot
    // isConnectedToInternet path needs no cross-process round trip.
    NmInternetStatus                 mCachedStatus{Exchange::INetworkManager::INTERNET_UNKNOWN};
    std::string                      mCachedReason;
    bool                             mHasCachedStatus{false};

    // PENDING handling: hold the prior status on a pending-quiet LIMITED_INTERNET,
    // then promote it on the first app query after threshold expiry. Guarded by mLock.
    bool                             mHoldPendingStatus{false};
    bool                             mPendingExpired{false};

    // Open() runs on its own thread so plugin Configure() never blocks on, or
    // re-enters, a ConnectivityCheckMgr that is not activated yet.
    std::thread                      mOpenThread;
    std::mutex                       mOpenMutex;
    std::condition_variable          mOpenCv;
    std::atomic<bool>                mStopOpenThread{false};
};

} // namespace Plugin
} // namespace WPEFramework

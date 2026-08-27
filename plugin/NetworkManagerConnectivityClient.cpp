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

#include "NetworkManagerConnectivityClient.h"
#include "NetworkManagerLogger.h"
#include <com/com.h>

using namespace WPEFramework;
using namespace WPEFramework::Exchange;
using namespace WPEFramework::Plugin;

// ---------------------------------------------------------------------------
// NetworkManagerConnectivityClient
// ---------------------------------------------------------------------------

NetworkManagerConnectivityClient::NetworkManagerConnectivityClient()
    : mNotification(*this)
{
    NMLOG_INFO("ConnectivityCheckMgr client created; opening link asynchronously");
    mOpenThread = std::thread(&NetworkManagerConnectivityClient::openThreadLoop, this);
}

NetworkManagerConnectivityClient::~NetworkManagerConnectivityClient()
{
    NMLOG_INFO("shutting down");
    {
        std::lock_guard<std::mutex> lock(mOpenMutex);
        mStopOpenThread = true;
    }
    mOpenCv.notify_one();
    if (mOpenThread.joinable()) {
        mOpenThread.join();
    }

    // Unregister without holding mLock to avoid deadlock when a notification
    // arrives concurrently and tries to acquire mLock inside notifyInternetStatusChanged.
    unregisterEvents();
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (mConnectivity != nullptr) {
            mConnectivity->Release();
            mConnectivity = nullptr;
        }
    }
    Close(Core::infinite);
}

void NetworkManagerConnectivityClient::openThreadLoop()
{
    constexpr auto retryInterval = std::chrono::seconds(5);

    while (!mStopOpenThread.load()) {
        NMLOG_INFO("connecting to ConnectivityCheckMgr");
        const uint32_t r = Open(RPC::CommunicationTimeOut, Connector(), "org.rdk.ConnectivityCheckMgr");
        if (r == Core::ERROR_NONE) {
            // Connected; Operational() is invoked by the framework when the proxy is ready.
            NMLOG_INFO("link to ConnectivityCheckMgr opened");
            return;
        }

        NMLOG_WARNING("failed to open link to ConnectivityCheckMgr (error %u); retrying", r);

        std::unique_lock<std::mutex> lock(mOpenMutex);
        mOpenCv.wait_for(lock, retryInterval, [this] { return mStopOpenThread.load(); });
    }
}

bool NetworkManagerConnectivityClient::IsValid() const
{
    LOG_ENTRY_FUNCTION();
    std::lock_guard<std::mutex> lock(mLock);
    return mConnectivity != nullptr;
}

void NetworkManagerConnectivityClient::Operational(bool upAndRunning)
{
    NMLOG_DEBUG("Operational(%s)", upAndRunning ? "true" : "false");
    if (upAndRunning) {
        {
            std::lock_guard<std::mutex> lock(mLock);
            if (mConnectivity != nullptr) {
                return; // already connected
            }
            mConnectivity = Interface();
            mHasCachedStatus = false;
            mCachedStatus = Exchange::INetworkManager::INTERNET_UNKNOWN;
            mCachedReason.clear();
        }
        // Register without holding mLock: Register() can dispatch a synchronous
        // notification on some Thunder builds, which would deadlock if mLock is held.
        registerEvents();
    } else {
        // Unregister without holding mLock for the same reason.
        unregisterEvents();
        std::lock_guard<std::mutex> lock(mLock);
        if (mConnectivity != nullptr) {
            mConnectivity->Release();
            mConnectivity = nullptr;
        }
        mHasCachedStatus = false;
        mCachedStatus = Exchange::INetworkManager::INTERNET_UNKNOWN;
        mCachedReason.clear();
    }
}

Exchange::IConnectivityCheck* NetworkManagerConnectivityClient::acquireInterface() const
{
    std::lock_guard<std::mutex> lock(mLock);
    if (mConnectivity != nullptr) {
        mConnectivity->AddRef();
    }
    return mConnectivity;
}

void NetworkManagerConnectivityClient::SetInternetStatusChangeHandler(InternetStatusChangeHandler handler)
{
    std::lock_guard<std::mutex> lock(mLock);
    mInternetStatusChangeHandler = std::move(handler);
}

void NetworkManagerConnectivityClient::registerEvents()
{
    Exchange::IConnectivityCheck* connectivity = nullptr;
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (mConnectivity == nullptr || mNotificationRegistered) {
            return;
        }
        // Claim the flag before dropping mLock so a concurrent caller cannot register twice.
        mNotificationRegistered = true;
        connectivity = mConnectivity;
        connectivity->AddRef();
    }

    const uint32_t r = connectivity->Register(&mNotification);
    connectivity->Release();

    if (r != Core::ERROR_NONE) {
        NMLOG_ERROR("ConnectivityCheckMgr register(notification) failed (%u)", r);
        std::lock_guard<std::mutex> lock(mLock);
        mNotificationRegistered = false;
        return;
    }

    NMLOG_INFO("registered for ConnectivityCheckMgr internet-status notifications");
}

void NetworkManagerConnectivityClient::unregisterEvents()
{
    Exchange::IConnectivityCheck* connectivity = nullptr;
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (!mNotificationRegistered) {
            return;
        }
        mNotificationRegistered = false;
        if (mConnectivity == nullptr) {
            return;
        }
        connectivity = mConnectivity;
        connectivity->AddRef();
    }

    if (const uint32_t r = connectivity->Unregister(&mNotification); r != Core::ERROR_NONE) {
        NMLOG_ERROR("ConnectivityCheckMgr unregister(notification) failed (%u)", r);
    }
    connectivity->Release();
}

void NetworkManagerConnectivityClient::notifyInternetStatusChanged(Exchange::IConnectivityCheck::InternetStatus status,
                                                                   const std::string& reason)
{
    const NmInternetStatus mapped = mapStatus(status);

    InternetStatusChangeHandler handler;
    {
        std::lock_guard<std::mutex> lock(mLock);
        mCachedStatus = mapped;
        mCachedReason = (status == Exchange::IConnectivityCheck::NO_INTERNET) ? reason : std::string();
        mHasCachedStatus = true;
        handler = mInternetStatusChangeHandler;
    }

    if (!handler) {
        return;
    }

    handler(mapped, reason);
}

void NetworkManagerConnectivityClient::Notification::OnInternetStatusChange(
    const Exchange::IConnectivityCheck::InternetStatus status,
    const string& reason)
{
    mParent.notifyInternetStatusChanged(status, reason);
}

NetworkManagerConnectivityClient::NmInternetStatus
NetworkManagerConnectivityClient::mapStatus(Exchange::IConnectivityCheck::InternetStatus status)
{
    switch (status) {
        case Exchange::IConnectivityCheck::NO_INTERNET:      return Exchange::INetworkManager::INTERNET_NOT_AVAILABLE;
        case Exchange::IConnectivityCheck::LIMITED_INTERNET: return Exchange::INetworkManager::INTERNET_LIMITED;
        case Exchange::IConnectivityCheck::CAPTIVE_PORTAL:   return Exchange::INetworkManager::INTERNET_CAPTIVE_PORTAL;
        case Exchange::IConnectivityCheck::FULLY_CONNECTED:  return Exchange::INetworkManager::INTERNET_FULLY_CONNECTED;
        case Exchange::IConnectivityCheck::UNKNOWN:
        default:                                             return Exchange::INetworkManager::INTERNET_UNKNOWN;
    }
}

NetworkManagerConnectivityClient::NmInternetStatus
NetworkManagerConnectivityClient::getInternetState()
{
    std::string reason;
    return getInternetState(reason);
}

NetworkManagerConnectivityClient::NmInternetStatus
NetworkManagerConnectivityClient::getInternetState(std::string& reason)
{
    LOG_ENTRY_FUNCTION();
    reason.clear();

    {
        std::lock_guard<std::mutex> lock(mLock);
        if (mHasCachedStatus) {
            reason = mCachedReason;
            return mCachedStatus;
        }
    }

    // Cold path only: no notification received yet, so seed the cache once.
    Exchange::IConnectivityCheck* connectivity = acquireInterface();
    if (connectivity == nullptr) {
        NMLOG_WARNING("ConnectivityCheckMgr not available; returning INTERNET_UNKNOWN");
        return Exchange::INetworkManager::INTERNET_UNKNOWN;
    }

    Exchange::IConnectivityCheck::StatusInfo info{};
    const uint32_t r = connectivity->GetInternetStatus(info);
    connectivity->Release();

    if (r != Core::ERROR_NONE) {
        NMLOG_ERROR("ConnectivityCheckMgr GetInternetStatus failed (%u)", r);
        return Exchange::INetworkManager::INTERNET_UNKNOWN;
    }

    const NmInternetStatus mapped = mapStatus(info.status);
    if (info.status == Exchange::IConnectivityCheck::NO_INTERNET) {
        reason = info.reason;
    }

    {
        std::lock_guard<std::mutex> lock(mLock);
        // A notification may have landed while the RPC was in flight; it is newer.
        if (!mHasCachedStatus) {
            mCachedStatus = mapped;
            mCachedReason = reason;
            mHasCachedStatus = true;
        }
    }
    return mapped;
}

std::string NetworkManagerConnectivityClient::getCaptivePortalURI()
{
    LOG_ENTRY_FUNCTION();
    std::string uri;

    Exchange::IConnectivityCheck* connectivity = acquireInterface();
    if (connectivity == nullptr) {
        NMLOG_WARNING("ConnectivityCheckMgr not available; returning empty captive-portal URI");
        return uri;
    }

    const uint32_t r = connectivity->GetCaptivePortalURI(uri);
    connectivity->Release();

    if (r != Core::ERROR_NONE) {
        NMLOG_ERROR("ConnectivityCheckMgr GetCaptivePortalURI failed (%u)", r);
        uri.clear();
    }
    return uri;
}

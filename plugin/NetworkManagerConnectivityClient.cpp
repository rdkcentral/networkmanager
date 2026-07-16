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
{
    NMLOG_INFO("connecting to ConnectivityCheckMgr");
    if (auto r = Open(RPC::CommunicationTimeOut, Connector(), "org.rdk.ConnectivityCheckMgr"); r == Core::ERROR_NONE) {
        // Connected; Operational() will be called by the framework when the proxy is ready.
    } else {
        NMLOG_ERROR("failed to open link to ConnectivityCheckMgr (error %u)", r);
    }
}

NetworkManagerConnectivityClient::~NetworkManagerConnectivityClient()
{
    NMLOG_INFO("shutting down");
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (mConnectivity != nullptr) {
            mConnectivity->Release();
            mConnectivity = nullptr;
        }
    }
    Close(Core::infinite);
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
    std::lock_guard<std::mutex> lock(mLock);
    if (upAndRunning) {
        if (mConnectivity == nullptr) {
            mConnectivity = Interface();
        }
    } else {
        if (mConnectivity != nullptr) {
            mConnectivity->Release();
            mConnectivity = nullptr;
        }
    }
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
    LOG_ENTRY_FUNCTION();
    std::lock_guard<std::mutex> lock(mLock);
    if (mConnectivity == nullptr) {
        NMLOG_WARNING("ConnectivityCheckMgr not available; returning INTERNET_UNKNOWN");
        return Exchange::INetworkManager::INTERNET_UNKNOWN;
    }

    Exchange::IConnectivityCheck::StatusInfo info{};
    if (auto r = mConnectivity->GetInternetStatus(info); r != Core::ERROR_NONE) {
        NMLOG_ERROR("ConnectivityCheckMgr GetInternetStatus failed (%u)", r);
        return Exchange::INetworkManager::INTERNET_UNKNOWN;
    }
    return mapStatus(info.status);
}

std::string NetworkManagerConnectivityClient::getCaptivePortalURI()
{
    LOG_ENTRY_FUNCTION();
    std::lock_guard<std::mutex> lock(mLock);
    std::string uri;
    if (mConnectivity == nullptr) {
        NMLOG_WARNING("ConnectivityCheckMgr not available; returning empty captive-portal URI");
        return uri;
    }
    if (auto r = mConnectivity->GetCaptivePortalURI(uri); r != Core::ERROR_NONE) {
        NMLOG_ERROR("ConnectivityCheckMgr GetCaptivePortalURI failed (%u)", r);
        uri.clear();
    }
    return uri;
}

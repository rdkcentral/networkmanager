/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#ifndef __THUNDERCOMRPCPROVIDER_H__
#define __THUNDERCOMRPCPROVIDER_H__

#include "INetworkData.h"
#include "Module.h"
// NOTE: INetworkManager.h is intentionally NOT included here.
// Both INetworkManager.h and INetworkConnectionStats.h define
// WPEFramework::Exchange::myIDs in the same namespace.  Including both
// in the same translation unit (which happens via NetworkDataProviderFactory.h
// → ThunderComRPCProvider.h inside NetworkConnectionStatsImplementation.cpp)
// causes a redefinition error.  INetworkManager.h is included only in
// ThunderComRPCProvider.cpp where INetworkConnectionStats.h is never seen.
#include <string>
#include <functional>
#include <core/JSON.h>

#define NETWORK_MANAGER_CALLSIGN "org.rdk.NetworkManager"

// Full definition lives in ThunderComRPCProvider.cpp — see note above.
class NetworkManagerNotification;

class NetworkComRPCProvider : public INetworkData
{
public:
    NetworkComRPCProvider();
    virtual ~NetworkComRPCProvider();

    bool Initialize(WPEFramework::PluginHost::IShell* service) override;

    std::string getIpv4Address(std::string interface_name) override;
    std::string getIpv6Address(std::string interface_name) override;
    std::string getIpv4Gateway() override;
    std::string getIpv6Gateway() override;
    std::string getIpv4PrimaryDns() override;
    std::string getIpv6PrimaryDns() override;
    std::string getConnectionType() override;
    std::string getDnsEntries() override;
    void populateNetworkData() override;
    std::string getInterface() override;
    bool pingToGatewayCheck(std::string endpoint, std::string ipversion, int count, int timeout) override;
    std::string getPacketLoss() override;
    std::string getAvgRtt() override;
    uint32_t SubscribeToEvent(const std::string& eventName,
        std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> callback) override;
    uint32_t invokeWiFiConnect() override;

private:
    // Cached data from last API calls
    std::string m_ipv4Gateway;
    std::string m_ipv6Gateway;
    std::string m_ipv4PrimaryDns;
    std::string m_ipv6PrimaryDns;
    std::string m_packetLoss;
    std::string m_avgRtt;

    // PluginHost::IShell used for QueryInterfaceByCallsign
    WPEFramework::PluginHost::IShell* m_service;

    // COM-RPC notification sink — heap-allocated, owned by this object.
    // Type is incomplete here; see ThunderComRPCProvider.cpp for the definition.
    NetworkManagerNotification* m_notification;
    bool m_notificationRegistered;
};

#endif /* __THUNDERCOMRPCPROVIDER_H__ */


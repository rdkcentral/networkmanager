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

#ifndef __NETWORKCONNECTIONSTATS_H__
#define __NETWORKCONNECTIONSTATS_H__

#include <WPEFramework/core/core.h>
#include <WPEFramework/plugins/Service.h>
#include <string.h>
#include <glib.h>


namespace WPEFramework {
namespace Plugin {

class NetworkConnectionStatsInterfaceData
{
  public:
    ~NetworkConnectionStatsInterfaceData();

    /* @brief Retrieve IPv4 address for specified interface */
    std::string getIpv4Address(std::string interface_name);

    /* @brief Retrieve IPv6 address for specified interface */
    std::string getIpv6Address(std::string interface_name);

    /* @brief Get current network connection type */
    std::string getConnectionType();

    /* @brief Get DNS server entries */
    std::string getDnsEntries();

    /* @brief Populate network interface data */
    void populateNetworkData();

  private:
    NetworkConnectionStatsInterfaceData();

    /* @brief Handle for JSON-RPC communication */
    WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>* controller = nullptr;

    /* @brief Handle for NetworkManager events */
    WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>* networkManager = nullptr;

};

class NetworkConnectionStats : public WPEFramework::Plugin::Service, public NetworkConnectionStatsInterfaceData {
public:
    NetworkConnectionStats(const NetworkConnectionStats&) = delete;

    NetworkConnectionStats& operator=(const NetworkConnectionStats&) = delete;  
    NetworkConnectionStats();

    virtual ~NetworkConnectionStats();
    
    /* @brief Get singleton instance of NetworkConnectionStats */
    static NetworkConnectionStats* getInstance();

    /* @brief Check and validate current connection type */
    void connectionTypeCheck();

    /* @brief Verify IP address configuration */
    void connectionIpCheck();

    /* @brief Check default IPv4 route availability */
    void defaultIpv4RouteCheck();

    /* @brief Check default IPv6 route availability */
    void defaultIpv6RouteCheck();

    /* @brief Monitor packet loss to gateway */
    void gatewayPacketLossCheck();

    /* @brief Validate DNS configuration and resolution */
    void networkDnsCheck();

    /* @brief Generate network diagnostics report */
    void generateReport();

private:
    /* @brief Singleton instance pointer */
    static NetworkConnectionStats* m_instance;
};

} // namespace Plugin
} // namespace WPEFramework 


#endif /* __NETWORKCONNECTIONSTATS_H__ */

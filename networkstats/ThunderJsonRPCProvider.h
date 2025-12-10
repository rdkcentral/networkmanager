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

#ifndef __THUNDERPROVIDER_H__
#define __THUNDERPROVIDER_H__

#include "INetworkData.h"
#include <string>
//#include <json/json.h>


class NetworkJsonRPCProvider : public INetworkData
{
  public:
    NetworkJsonRPCProvider();
    ~NetworkJsonRPCProvider();
    /* @brief Retrieve IPv4 address for specified interface
     * @param interface_name Interface name (e.g., eth0, wlan0)
     * @return IPv4 address string
     */
    std::string getIpv4Address(std::string interface_name);

    /* @brief Retrieve IPv6 address for specified interface
     * @param interface_name Interface name (e.g., eth0, wlan0)
     * @return IPv6 address string
     */
    std::string getIpv6Address(std::string interface_name);

    /* @brief Get IPv4 gateway/route address from last getIpv4Address call */
    std::string getIpv4Gateway();

    /* @brief Get IPv6 gateway/route address from last getIpv6Address call */
    std::string getIpv6Gateway();

    /* @brief Get IPv4 primary DNS from last getIpv4Address call */
    std::string getIpv4PrimaryDns();

    /* @brief Get IPv6 primary DNS from last getIpv6Address call */
    std::string getIpv6PrimaryDns();

    /* @brief Get current network connection type */
    std::string getConnectionType();

    /* @brief Get DNS server entries */
    std::string getDnsEntries();

    /* @brief Populate network interface data */
    void populateNetworkData();

    /* @brief Get current active interface name */
    std::string getInterface();

    /* @brief Ping to gateway to check packet loss
     * @param endpoint Gateway IP address to ping
     * @param ipversion Either "IPv4" or "IPv6"
     * @param count Number of ping packets to send
     * @param timeout Timeout in seconds
     * @return true if ping successful, false otherwise
     */
    bool pingToGatewayCheck(std::string endpoint, std::string ipversion, int count, int timeout);

    /* @brief Get packet loss from last ping call */
    std::string getPacketLoss();

    /* @brief Get average RTT from last ping call */
    std::string getAvgRtt();

private:
    // Cached data from last API calls
    std::string m_ipv4Gateway;
    std::string m_ipv6Gateway;
    std::string m_ipv4PrimaryDns;
    std::string m_ipv6PrimaryDns;
    std::string m_packetLoss;
    std::string m_avgRtt;
};

#endif /* __THUNDERPROVIDER_H__ */

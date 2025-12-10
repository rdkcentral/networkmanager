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
#include <string>
#include <json/json.h>

#define NETWORK_MANAGER_CALLSIGN "org.rdk.NetworkManager.1"

class NetworkComRPCProvider : public INetworkData
{
public:
    NetworkComRPCProvider();
    virtual ~NetworkComRPCProvider();

            /* @brief Retrieve IPv4 address for specified interface
             * @param interface_name Interface name (e.g., eth0, wlan0)
             * @return IPv4 address string
             */
            virtual std::string getIpv4Address(std::string interface_name) override;

            /* @brief Retrieve IPv6 address for specified interface
             * @param interface_name Interface name (e.g., eth0, wlan0)
             * @return IPv6 address string
             */
            virtual std::string getIpv6Address(std::string interface_name) override;

            /* @brief Get IPv4 gateway/route address from last getIpv4Address call */
            virtual std::string getIpv4Gateway() override;

            /* @brief Get IPv6 gateway/route address from last getIpv6Address call */
            virtual std::string getIpv6Gateway() override;

            /* @brief Get IPv4 primary DNS from last getIpv4Address call */
            virtual std::string getIpv4PrimaryDns() override;

            /* @brief Get IPv6 primary DNS from last getIpv6Address call */
            virtual std::string getIpv6PrimaryDns() override;

            /* @brief Get current network connection type */
            virtual std::string getConnectionType() override;

            /* @brief Get DNS server entries */
            virtual std::string getDnsEntries() override;

            /* @brief Populate network interface data */
            virtual void populateNetworkData() override;

            /* @brief Get current active interface name */
            virtual std::string getInterface() override;

            /* @brief Ping to gateway to check packet loss
             * @param endpoint Gateway IP address to ping
             * @param ipversion Either "IPv4" or "IPv6"
             * @param count Number of ping packets to send
             * @param timeout Timeout in seconds
             * @return true if ping successful, false otherwise
             */
            virtual bool pingToGatewayCheck(std::string endpoint, std::string ipversion, int count, int timeout) override;

            /* @brief Get packet loss from last ping call */
            virtual std::string getPacketLoss() override;

            /* @brief Get average RTT from last ping call */
            virtual std::string getAvgRtt() override;

private:
    /* @brief Initialize COM-RPC connection to NetworkManager */
    bool initializeConnection();

    /* @brief Release COM-RPC connection */
    void releaseConnection();
    
    /* @brief Convert COM-RPC response to Json::Value */
    void convertToJsonResponse(const std::string& jsonString, Json::Value& response);

private:
    void* m_networkManager;  // Will be casted to appropriate interface
    bool m_isConnected;
    
    // Cached data from last API calls
    std::string m_ipv4Gateway;
    std::string m_ipv6Gateway;
    std::string m_ipv4PrimaryDns;
    std::string m_ipv6PrimaryDns;
    std::string m_packetLoss;
    std::string m_avgRtt;
};

#endif /* __THUNDERCOMRPCPROVIDER_H__ */
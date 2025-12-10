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

#include <string.h>


class INetworkData
{
  public:
    ~NetworkConnectionStatsInterfaceData();

    /* @brief Retrieve IPv4 address for specified interface */
    std::string getIpv4Address(std::string interface_name) =0;

    /* @brief Retrieve IPv6 address for specified interface */
    std::string getIpv6Address(std::string interface_name) =0;

    /* @brief Get current network connection type */
    std::string getConnectionType() =0;

    /* @brief Get DNS server entries */
    std::string getDnsEntries() =0;

    /* @brief Populate network interface data */
    void populateNetworkData() =0;

    /* @brief Get current active interface name */
    std::string getInterface() =0;

  private:
    NetworkConnectionStatsInterfaceData();

};


#endif /* __NETWORKCONNECTIONSTATS_H__ */

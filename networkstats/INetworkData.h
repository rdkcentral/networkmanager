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

#ifndef __INETWORKDATA_H__
#define __INETWORKDATA_H__

#include <string>

class INetworkData
{
  public:
    virtual ~INetworkData() {}

    /* @brief Retrieve IPv4 address for specified interface */
    virtual std::string getIpv4Address(std::string interface_name) = 0;

    /* @brief Retrieve IPv6 address for specified interface */
    virtual std::string getIpv6Address(std::string interface_name) = 0;

    /* @brief Get current network connection type */
    virtual std::string getConnectionType() = 0;

    /* @brief Get DNS server entries */
    virtual std::string getDnsEntries() = 0;

    /* @brief Populate network interface data */
    virtual void populateNetworkData() = 0;

    /* @brief Get current active interface name */
    virtual std::string getInterface() = 0;

  protected:
    INetworkData() {}

};


#endif /* __INETWORKDATA_H__ */

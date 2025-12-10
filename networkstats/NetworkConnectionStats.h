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

#include "INetworkData.h"
#include "ThunderProvider.h"
#include <string>
#include <glib.h>

<<<<<<< HEAD

namespace WPEFramework {
namespace Plugin {


class NetworkConnectionStats : public WPEFramework::Plugin::Service, public NetworkConnectionStatsInterfaceData {
=======
class NetworkConnectionStats {
>>>>>>> cf99e2a (Initial Skeleton.)
public:
    NetworkConnectionStats(const NetworkConnectionStats&) = delete;

    NetworkConnectionStats& operator=(const NetworkConnectionStats&) = delete;  
    NetworkConnectionStats();

    virtual ~NetworkConnectionStats();

    /* @brief Generate network diagnostics report */
    void generateReport();
    
<<<<<<< HEAD
    /* @brief Generate network diagnostics report */
=======
    /* @brief Generate Periodic reporting diagnostics report */
>>>>>>> cf99e2a (Initial Skeleton.)
    void periodic_reporting();

protected:
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

<<<<<<< HEAD

=======
>>>>>>> cf99e2a (Initial Skeleton.)
private:
    /* @brief Singleton instance pointer */
    static NetworkConnectionStats* m_instance;
    INetworkData* iprovider;
};

#endif /* __NETWORKCONNECTIONSTATS_H__ */

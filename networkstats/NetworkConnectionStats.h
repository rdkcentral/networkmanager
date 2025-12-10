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
#include "ThunderJsonRPCProvider.h"
#include <string>
// #include <plugins/plugins.h>  // Commented for standalone compilation
#include <mutex>
#if USE_TELEMETRY
#include <telemetry_busmessage_sender.h>
#endif

namespace WPEFramework {
    namespace Plugin {
        class NetworkConnectionStats {
        public:
            NetworkConnectionStats(const NetworkConnectionStats&) = delete;

            NetworkConnectionStats& operator=(const NetworkConnectionStats&) = delete;  
            NetworkConnectionStats();

            virtual ~NetworkConnectionStats();

            /* @brief Initialize network connection stats */
            const std::string Initialize(/* PluginHost::IShell* service */);
            
            /* @brief Deinitialize and cleanup resources */
            void Deinitialize(/* PluginHost::IShell* service */);

            /* @brief Generate network diagnostics report */
            void generateReport();
            
            /* @brief Generate Periodic reporting diagnostics report */
            void periodic_reporting();

            /* @brief Get singleton instance of NetworkConnectionStats */
            static NetworkConnectionStats* getInstance();

        protected:

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

            /* @brief Telemetry Logging */
            void logTelemetry(const std::string& eventName, const std::string& message);

    private:
        /* @brief Singleton instance pointer */
        static NetworkConnectionStats* m_instance;
        /* @brief Mutex for thread-safe singleton initialization */
        static std::mutex m_instance_mutex;
        INetworkData* iprovider;
        // PluginHost::IShell* m_service;  // Commented for standalone compilation
        
        /* @brief Cached network data */
        std::string m_interface;
        std::string m_ipv4Address;
        std::string m_ipv6Address;

        std::string m_ipv4Route;
        std::string m_ipv6Route;
        
        std::string m_ipv4Dns;
        std::string m_ipv6Dns;
        };
    } // namespace Plugin
} // namespace WPEFramework

#endif /* __NETWORKCONNECTIONSTATS_H__ */
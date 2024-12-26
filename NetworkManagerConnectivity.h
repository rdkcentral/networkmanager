/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 */

#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <cerrno>
#include <cstdlib>
#include <fstream>

enum nsm_ipversion
{
    NSM_IPRESOLVE_WHATEVER  = 0, /* default, resolves addresses to all IP*/
    NSM_IPRESOLVE_V4        = 1, /* resolve to IPv4 addresses */
    NSM_IPRESOLVE_V6        = 2  /* resolve to IPv6 addresses */
};

enum nsm_internetState {
    NO_INTERNET,
    LIMITED_INTERNET,
    CAPTIVE_PORTAL,
    FULLY_CONNECTED,
    UNKNOWN,
};

enum nsm_connectivity_httpcode {
    HttpStatus_response_error               = 99,
    HttpStatus_200_OK                      = 200,
    HttpStatus_204_No_Content              = 204,
    HttpStatus_301_Moved_Permanentl        = 301,
    HttpStatus_302_Found                   = 302,     // captive portal
    HttpStatus_307_Temporary_Redirect      = 307,
    HttpStatus_308_Permanent_Redirect      = 308,
    HttpStatus_403_Forbidden               = 403,
    HttpStatus_404_Not_Found               = 404,
    HttpStatus_511_Authentication_Required = 511      // captive portal RFC 6585
};

#define NMCONNECTIVITY_CURL_HEAD_REQUEST          true
#define NMCONNECTIVITY_CURL_GET_REQUEST           false
#define NMCONNECTIVITY_MONITOR_CACHE_FILE         "/tmp/nm.plugin.endpoints"
#define NMCONNECTIVITY_MONITOR_MIN_INTERVAL         5      // sec
#define NMCONNECTIVITY_MONITOR_RETRY_INTERVAL       30     //  sec
#define NMCONNECTIVITY_CURL_REQUEST_TIMEOUT_MS      5000   // ms
#define NM_CONNECTIVITY_MONITOR_RETRY_COUNT         3      // 3 retry

namespace WPEFramework
{
    namespace Plugin
    {

        class DnsResolver
        {
            public:
                DnsResolver(std::string url, nsm_ipversion ipversion = NSM_IPRESOLVE_WHATEVER);
                ~DnsResolver(){};
                bool operator()() { return (ipv6Resolved || ipv4Resolved);}

            private:
                std::string m_domain{};
                bool ipv4Resolved = false;
                bool ipv6Resolved = false;
                std::string convertUrIToDomainName(std::string& url);
                bool resolveIP(std::string& uri, nsm_ipversion ipversion = NSM_IPRESOLVE_WHATEVER);
        };

        /*
         * Save user specific endpoint into a cache file and load from the file 
         * if endpoints are empty in case plugin is restarted.
         */
        class EndpointManager {
        public:
            EndpointManager();
            ~EndpointManager() {}
            void writeEndpointsToFile(const std::vector<std::string>& endpoints);
            std::vector<std::string> readEndpointsFromFile();
            void setConnectivityMonitorEndpoints(const std::vector<std::string>& endpoints);
            std::vector<std::string> getConnectivityMonitorEndpoints();
            std::vector<std::string> operator()() { return getConnectivityMonitorEndpoints(); }

        private:
            std::string m_CachefilePath;
            std::vector<std::string> m_Endpoints;
            std::mutex m_endpointMutex;
        };

        class TestConnectivity
        {
            TestConnectivity(const TestConnectivity&) = delete;
            const TestConnectivity& operator=(const TestConnectivity&) = delete;

        public:
            TestConnectivity(const std::vector<std::string>& endpoints, long timeout_ms = 2000, bool  = true, nsm_ipversion ipversion = NSM_IPRESOLVE_WHATEVER);
            ~TestConnectivity(){}
            std::string getCaptivePortal() {return captivePortalURI;}
            nsm_internetState getInternetState(){return internetSate;}
        private:
            nsm_internetState checkCurlResponse(const std::vector<std::string>& endpoints, long timeout_ms,  bool headReq, nsm_ipversion ipversion);
            nsm_internetState checkInternetStateFromResponseCode(const std::vector<int>& responses);
            std::string captivePortalURI;
            nsm_internetState internetSate;
        };

        class ConnectivityMonitor
        {
        public:
            ConnectivityMonitor();
            ~ConnectivityMonitor();
            bool stopConnectivityMonitor();
            bool startConnectivityMonitor();
            bool switchToInitialCheck();
            void setConnectivityMonitorEndpoints(const std::vector<std::string> &endpoints);
            std::vector<std::string> getConnectivityMonitorEndpoints();
            bool isConnectedToInternet(nsm_ipversion ipversion);
            nsm_internetState getInternetState(nsm_ipversion& ipversion);
            std::string getCaptivePortalURI();

        private:
            ConnectivityMonitor(const ConnectivityMonitor&) = delete;
            ConnectivityMonitor& operator=(const ConnectivityMonitor&) = delete;
            void connectivityMonitorFunction();
            void notifyInternetStatusChangedEvent(nsm_internetState newState);
            /* connectivity monitor */
            std::thread m_cmThrdID;
            std::atomic<bool> m_cmRunning;
            std::condition_variable m_cmCv;
            std::mutex m_cmMutex;
            std::atomic<bool> m_notify;
            std::atomic<bool> m_switchToInitial;
            std::string m_captiveURI;
            std::atomic<nsm_internetState> m_InternetState;
            std::atomic<nsm_internetState> m_Ipv4InternetState;
            std::atomic<nsm_internetState> m_Ipv6InternetState;
            /* manages endpoints */
            EndpointManager m_endpoint;
        };
    } // namespace Plugin
} // namespace WPEFramework

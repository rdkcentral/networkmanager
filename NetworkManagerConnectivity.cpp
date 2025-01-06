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

#include <curl/curl.h>
#include <resolv.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <thread>
#include <algorithm>

#include "NetworkManagerImplementation.h"
#include "NetworkManagerConnectivity.h"
#include "NetworkManagerLogger.h"
#include "INetworkManager.h"

namespace WPEFramework
{
    namespace Plugin
    {

    extern NetworkManagerImplementation* _instance;

    constexpr auto INTERNET_NOT_AVAILABLE = Exchange::INetworkManager::InternetStatus::INTERNET_NOT_AVAILABLE;
    constexpr auto INTERNET_LIMITED = Exchange::INetworkManager::InternetStatus::INTERNET_LIMITED;
    constexpr auto INTERNET_CAPTIVE_PORTAL = Exchange::INetworkManager::InternetStatus::INTERNET_CAPTIVE_PORTAL;
    constexpr auto INTERNET_FULLY_CONNECTED = Exchange::INetworkManager::InternetStatus::INTERNET_FULLY_CONNECTED;
    constexpr auto INTERNET_UNKNOWN = Exchange::INetworkManager::InternetStatus::INTERNET_UNKNOWN;

    constexpr auto IP_ADDRESS_V4 = Exchange::INetworkManager::IPVersion::IP_ADDRESS_V4;
    constexpr auto IP_ADDRESS_V6 = Exchange::INetworkManager::IPVersion::IP_ADDRESS_V6;

    static const char* getInternetStateString(Exchange::INetworkManager::InternetStatus state)
    {
        switch(state)
        {
            case INTERNET_NOT_AVAILABLE: return "NO_INTERNET";
            case INTERNET_LIMITED: return "LIMITED_INTERNET";
            case INTERNET_CAPTIVE_PORTAL: return "CAPTIVE_PORTAL";
            case INTERNET_FULLY_CONNECTED: return "FULLY_CONNECTED";
            default: return "UNKNOWN";
        }
    }

    void EndpointManager::writeEndpointsToFile(const std::vector<std::string>& endpoints)
    {
        std::ofstream outputFile(m_CachefilePath);
        if (outputFile.is_open())
        {
            for (const std::string& str : endpoints)
            {
                outputFile << str << '\n';
            }
            outputFile.close();
        }
        else
        {
            NMLOG_ERROR("Connectivity endpoints file write error");
        }
    }

    std::vector<std::string> EndpointManager::readEndpointsFromFile()
    {
        std::vector<std::string> readStrings;
        std::ifstream inputFile(m_CachefilePath);
        if (inputFile.is_open())
        {
            std::string line;
            while (std::getline(inputFile, line))
            {
                readStrings.push_back(line);
            }
            inputFile.close();
        }
        else
        {
            NMLOG_ERROR("Failed to open connectivity endpoint cache file");
        }
        return readStrings;
    }

    void EndpointManager::setConnectivityMonitorEndpoints(const std::vector<std::string>& endpoints)
    {
        const std::lock_guard<std::mutex> lock(m_endpointMutex);
        if(endpoints.empty())
        {
            NMLOG_ERROR("Empty endpoints");
            return;
        }

        m_Endpoints.clear();
        for (auto endpoint : endpoints) {
            if(!endpoint.empty() && endpoint.size() > 3)
                m_Endpoints.push_back(endpoint.c_str());
            else
                NMLOG_ERROR("endpoint not vallied = %s", endpoint.c_str());
        }

        // write the endpoints to a file
        writeEndpointsToFile(m_Endpoints);

        std::string endpointsStr;
        for (const auto& endpoint : m_Endpoints)
            endpointsStr.append(endpoint).append(" ");
        NMLOG_INFO("Connectivity monitor endpoints updated -: %d :- %s", static_cast<int>(m_Endpoints.size()), endpointsStr.c_str());
    }

    EndpointManager::EndpointManager()
    {
        m_CachefilePath = NMCONNECTIVITY_MONITOR_CACHE_FILE;
        m_Endpoints.clear(); // default value will be loaded from NetworkManagerImplementation configuration

        std::ifstream inputFile(m_CachefilePath);
        if (inputFile.is_open())
        {
            std::string line;
            std::vector<std::string> endpoints{};
            while (std::getline(inputFile, line))
            {
                if(!line.empty() && line.size() > 3)
                    endpoints.push_back(line);
            }
            NMLOG_WARNING("cached connectivity endpoints loaded ..");
            setConnectivityMonitorEndpoints(endpoints);
            inputFile.close();
        }
        else
        {
            NMLOG_ERROR("no endpoint cache file found");
        }
    }

    std::vector<std::string> EndpointManager::getConnectivityMonitorEndpoints()
    {
        const std::lock_guard<std::mutex> lock(m_endpointMutex);
        return m_Endpoints;
    }

    bool DnsResolver::resolveIP(std::string& uri, Exchange::INetworkManager::IPVersion& ipversion)
    {
        struct addrinfo sockAddrProps, *resultAddr= NULL;
        char ipStr[INET6_ADDRSTRLEN] = {0};

        if(ipversion == IP_ADDRESS_V4)
            sockAddrProps.ai_family = AF_INET;
        else if(ipversion == IP_ADDRESS_V6)
            sockAddrProps.ai_family = AF_INET6;
        else
            sockAddrProps.ai_family = AF_UNSPEC;

        sockAddrProps.ai_socktype = SOCK_STREAM;
        sockAddrProps.ai_flags = 0;
        sockAddrProps.ai_protocol = 0;

        int ret = getaddrinfo(uri.c_str(), NULL, &sockAddrProps, &resultAddr);
        if (ret != 0) {
            NMLOG_WARNING("Resolved IP Failed getaddrinfo: %s", gai_strerror(ret));
            return false;
        }

        NMLOG_DEBUG("Resolved IP addresses for %s", uri.c_str());

        for (struct addrinfo * resutIP = resultAddr; resutIP != NULL; resutIP = resutIP->ai_next)
        {
            void *addr= NULL;
            if (resutIP->ai_family == AF_INET) {
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)resutIP->ai_addr;
                addr = &(ipv4->sin_addr);
                ipv4Resolved = true;
            } else if (resutIP->ai_family == AF_INET6) {
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)resutIP->ai_addr;
                addr = &(ipv6->sin6_addr);
                ipv6Resolved = true;
            } else {
                continue;
            }

            inet_ntop(resutIP->ai_family, addr, ipStr, sizeof(ipStr));
            NMLOG_DEBUG("Resolved IP --> %s", ipStr);
        }

        freeaddrinfo(resultAddr);
        return true;
    }

    std::string DnsResolver::convertUrIToDomainName(std::string& url)
    {
        size_t domainStart = 0;
        size_t protocolEnd = url.find("://");
        if (protocolEnd != std::string::npos) {  
            domainStart = protocolEnd + 3;
        }

        size_t domainEnd = url.find('/', domainStart);
        if (domainEnd != std::string::npos)
            return url.substr(domainStart, domainEnd - domainStart);
        else
            return url.substr(domainStart);
    }

    DnsResolver::DnsResolver(std::string url, Exchange::INetworkManager::IPVersion ipversion, int curlErrorCode)
    {
        NMLOG_DEBUG("url %s - ipversion %d - curl error %d ", url.c_str(), ipversion, curlErrorCode);
        if(url.empty()) {
            NMLOG_ERROR("URI/hostname missing");
            return;
        }
        switch (curlErrorCode)
        {
            case CURLE_COULDNT_RESOLVE_HOST:    // 6 - Could not resolve host
            case CURLE_OPERATION_TIMEDOUT:      // 28 - Operation timeout. time-out period
            case CURLE_RECV_ERROR:              // 56 - Failure with receiving network data
            {
                m_domain = convertUrIToDomainName(url);
                resolveIP(m_domain, ipversion);
                break;
            }
            default:
                ipv4Resolved = false;
                ipv6Resolved = false;
                break;
        }
    }

    TestConnectivity::TestConnectivity(const std::vector<std::string>& endpoints, long timeout_ms, bool headReq, Exchange::INetworkManager::IPVersion ipversion)
    {
        internetSate = INTERNET_UNKNOWN;
        if(endpoints.size() < 1) {
            NMLOG_ERROR("Endpoints size error ! curl check not possible");
            return;
        }

        internetSate = checkCurlResponse(endpoints, timeout_ms, headReq, ipversion);
    }

    static bool curlVerboseEnabled() {
        std::ifstream fileStream("/tmp/nm.plugin.debug");
        return fileStream.is_open();
    }

    static long current_time ()
    {
        struct timespec ts;
        clock_gettime (CLOCK_MONOTONIC, &ts);
        return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    }
    static size_t writeFunction(void* ptr, size_t size, size_t nmemb, std::string* data) {
    #ifdef DBG_CURL_GET_RESPONSE
        LOG_DBG("%s",(char*)ptr);
    #endif
        return size * nmemb;
    }

    Exchange::INetworkManager::InternetStatus TestConnectivity::checkCurlResponse(const std::vector<std::string>& endpoints, long timeout_ms,  bool headReq, Exchange::INetworkManager::IPVersion ipversion)
    {
        long deadline = current_time() + timeout_ms, time_now = 0, time_earlier = 0;

        CURLM *curl_multi_handle = curl_multi_init();
        if (!curl_multi_handle)
        {
            NMLOG_ERROR("curl_multi_init returned NULL");
            return INTERNET_NOT_AVAILABLE;
        }

        CURLMcode mc;
        std::vector<CURL*> curl_easy_handles;
        std::vector<int> http_responses;
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Cache-Control: no-cache, no-store");
        chunk = curl_slist_append(chunk, "Connection: close");
        for (const auto& endpoint : endpoints)
        {
            CURL *curl_easy_handle = curl_easy_init();
            if (!curl_easy_handle)
            {
                NMLOG_ERROR("endpoint = <%s> curl_easy_init returned NULL", endpoint.c_str());
                continue;
            }
            curl_easy_setopt(curl_easy_handle, CURLOPT_URL, endpoint.c_str());
            curl_easy_setopt(curl_easy_handle, CURLOPT_PRIVATE, endpoint.c_str());
            /* set our custom set of headers */
            curl_easy_setopt(curl_easy_handle, CURLOPT_HTTPHEADER, chunk);
            curl_easy_setopt(curl_easy_handle, CURLOPT_USERAGENT, "RDKCaptiveCheck/1.0");
            if(!headReq)
            {
                /* HTTPGET request added insted of HTTPHEAD request fix for DELIA-61526 */
                curl_easy_setopt(curl_easy_handle, CURLOPT_HTTPGET, 1L);
            }
            curl_easy_setopt(curl_easy_handle, CURLOPT_WRITEFUNCTION, writeFunction);
            curl_easy_setopt(curl_easy_handle, CURLOPT_TIMEOUT_MS, deadline - current_time());
            if (IP_ADDRESS_V4 == ipversion) {
                curl_easy_setopt(curl_easy_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
                NMLOG_DEBUG("curlopt ipversion = IPv4 reqtyp = %s", headReq? "HEAD":"GET");
            }
            else if (IP_ADDRESS_V6 == ipversion) {
                curl_easy_setopt(curl_easy_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
                NMLOG_DEBUG("curlopt ipversion = IPv6 reqtyp = %s", headReq? "HEAD":"GET");
            }
            else
                NMLOG_DEBUG("curlopt ipversion = whatever reqtyp = %s", headReq? "HEAD":"GET");

            if(curlVerboseEnabled())
                curl_easy_setopt(curl_easy_handle, CURLOPT_VERBOSE, 1L);
            if (CURLM_OK != (mc = curl_multi_add_handle(curl_multi_handle, curl_easy_handle)))
            {
                NMLOG_ERROR("endpoint = <%s> curl_multi_add_handle returned %d (%s)", endpoint.c_str(), mc, curl_multi_strerror(mc));
                curl_easy_cleanup(curl_easy_handle);
                continue;
            }
            curl_easy_handles.push_back(curl_easy_handle);
        }
        int handles, msgs_left;
        char *url = nullptr;
    #if LIBCURL_VERSION_NUM < 0x074200
        int numfds, repeats = 0;
    #endif
        char *endpoint = nullptr;
        while (1)
        {
            if (CURLM_OK != (mc = curl_multi_perform(curl_multi_handle, &handles)))
            {
                NMLOG_ERROR("curl_multi_perform returned %d (%s)", mc, curl_multi_strerror(mc));
                break;
            }
            for (CURLMsg *msg; NULL != (msg = curl_multi_info_read(curl_multi_handle, &msgs_left)); )
            {
                long response_code = -1;
                if (msg->msg != CURLMSG_DONE)
                    continue;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &endpoint);
                if (CURLE_OK == msg->data.result) {
                    if (curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code) == CURLE_OK)
                    {
                        if (HttpStatus_302_Found == response_code) {
                            if ( (curl_easy_getinfo(msg->easy_handle, CURLINFO_REDIRECT_URL, &url) == CURLE_OK) && url != nullptr) {
                                NMLOG_INFO("captive portal found !!!");
                                captivePortalURI = url;
                            }
                        }
                    }
                }
                else {
                    NMLOG_ERROR("endpoint = <%s> curl error = %d (%s)", endpoint, msg->data.result, curl_easy_strerror(msg->data.result));
                    curlErrorCode = static_cast<int>(msg->data.result);
                }
                http_responses.push_back(response_code);
            }
            time_earlier = time_now;
            time_now = current_time();
            if (handles == 0 || time_now >= deadline)
                break;
    #if LIBCURL_VERSION_NUM < 0x074200
            if (CURLM_OK != (mc = curl_multi_wait(curl_multi_handle, NULL, 0, deadline - time_now, &numfds)))
            {
                LOGERR("curl_multi_wait returned %d (%s)", mc, curl_multi_strerror(mc));
                break;
            }
            if (numfds == 0)
            {
                repeats++;
                if (repeats > 1)
                    usleep(10*1000); /* sleep 10 ms */
            }
            else
                repeats = 0;
    #else
            if (CURLM_OK != (mc = curl_multi_poll(curl_multi_handle, NULL, 0, deadline - time_now, NULL)))
            {
                NMLOG_ERROR("curl_multi_poll returned %d (%s)", mc, curl_multi_strerror(mc));
                break;
            }
    #endif
        }

        if(curlVerboseEnabled()) {
            NMLOG_DEBUG("endpoints count = %d response count %d, handles = %d, deadline = %ld, time_now = %ld, time_earlier = %ld",
                static_cast<int>(endpoints.size()), static_cast<int>(http_responses.size()), handles, deadline, time_now, time_earlier);
        }

        for (const auto& curl_easy_handle : curl_easy_handles)
        {
            curl_easy_getinfo(curl_easy_handle, CURLINFO_PRIVATE, &endpoint);
            //LOG_DBG("endpoint = <%s> terminating attempt", endpoint);
            curl_multi_remove_handle(curl_multi_handle, curl_easy_handle);
            curl_easy_cleanup(curl_easy_handle);
        }
        curl_multi_cleanup(curl_multi_handle);
        /* free the custom headers */
        curl_slist_free_all(chunk);
        return checkInternetStateFromResponseCode(http_responses);
    }

    /*
    * verifying Most occurred response code is 50 % or more
    * Example 1 :
    *      if we have 5 endpoints so response also 5 ( 204 302 204 204 200 ) . Here count is 204 :- 3, 302 :- 1, 200 :- 1
    *      Return Internet State: FULLY_CONNECTED - 60 %
    * Example 2 :
    *      if we have 4 endpoints so response also 4 ( 204 204 200 200 ) . Here count is 204 :- 2, 200 :- 2
    *      Return Internet State: FULLY_CONNECTED - 50 %
    */

    Exchange::INetworkManager::InternetStatus TestConnectivity::checkInternetStateFromResponseCode(const std::vector<int>& responses)
    {
        Exchange::INetworkManager::InternetStatus InternetConnectionState = INTERNET_NOT_AVAILABLE;
        nsm_connectivity_httpcode http_response_code = HttpStatus_response_error;

        int max_count = 0;
        for (int element : responses)
        {
            int element_count = count(responses.begin(), responses.end(), element);
            if (element_count > max_count)
            {
                http_response_code = static_cast<nsm_connectivity_httpcode>(element);
                max_count = element_count;
            }
        }

        /* Calculate the percentage of the most frequent code occurrences */
        float percentage = (static_cast<float>(max_count) / responses.size());

        /* 50 % connectivity check */
        if (percentage >= 0.5)
        {
            switch (http_response_code)
            {
                case HttpStatus_204_No_Content:
                    InternetConnectionState = INTERNET_FULLY_CONNECTED;
                    NMLOG_INFO("Internet State: FULLY_CONNECTED - %.1f%%", (percentage*100));
                break;
                case HttpStatus_200_OK:
                    InternetConnectionState = INTERNET_LIMITED;
                    NMLOG_INFO("Internet State: LIMITED_INTERNET - %.1f%%", (percentage*100));
                break;
                case HttpStatus_511_Authentication_Required:
                case HttpStatus_302_Found:
                    InternetConnectionState = INTERNET_CAPTIVE_PORTAL;
                    NMLOG_INFO("Internet State: CAPTIVE_PORTAL - %.1f%%", (percentage*100));
                break;
                default:
                    InternetConnectionState = INTERNET_NOT_AVAILABLE;
                    if(http_response_code == -1)
                        NMLOG_ERROR("Internet State: NO_INTERNET (curl error)");
                    else
                        NMLOG_WARNING("Internet State: NO_INTERNET (http code: %d - %.1f%%)", static_cast<int>(http_response_code), percentage * 100);
                    break;
            }
        }
        return InternetConnectionState;
    }

    ConnectivityMonitor::ConnectivityMonitor()
    {
        NMLOG_WARNING("ConnectivityMonitor");
        m_cmRunning = false;
        m_notify = true;
        m_InternetState = INTERNET_UNKNOWN;
        m_Ipv4InternetState = INTERNET_UNKNOWN;
        m_Ipv6InternetState = INTERNET_UNKNOWN;
        startConnectivityMonitor();
    }

    ConnectivityMonitor::~ConnectivityMonitor()
    {
        NMLOG_WARNING("~ConnectivityMonitor");
        stopConnectivityMonitor();
    }

    std::vector<std::string> ConnectivityMonitor::getConnectivityMonitorEndpoints()
    {
        return m_endpoint.getConnectivityMonitorEndpoints();
    }

    void ConnectivityMonitor::setConnectivityMonitorEndpoints(const std::vector<std::string> &endpoints)
    {
        m_endpoint.setConnectivityMonitorEndpoints(endpoints);
    }

    Exchange::INetworkManager::InternetStatus ConnectivityMonitor::getInternetState(Exchange::INetworkManager::IPVersion& ipversion, bool ipVersionNotSpecified)
    {
        if(ipVersionNotSpecified) {
            ipversion = m_ipversion;
           return m_InternetState.load();
        }
        else if(ipversion == IP_ADDRESS_V4)
            return m_Ipv4InternetState.load();
        else if(ipversion == IP_ADDRESS_V6)
            return m_Ipv6InternetState.load();
        else
            return INTERNET_UNKNOWN;
    }

    std::string ConnectivityMonitor::getCaptivePortalURI()
    {
        if(m_Ipv4InternetState == INTERNET_CAPTIVE_PORTAL || m_Ipv6InternetState == INTERNET_CAPTIVE_PORTAL)
        {
            NMLOG_INFO("captive portal URI = %s", m_captiveURI.c_str());
            return m_captiveURI;
        }

        NMLOG_WARNING("No captive portal found !");
        return std::string("");
    }

    bool ConnectivityMonitor::startConnectivityMonitor()
    {
        if (m_cmRunning)
        {
            m_cmCv.notify_one();
            NMLOG_DEBUG("connectivity monitor is already running");
            return true;
        }

        m_cmRunning = true;
        m_cmThrdID = std::thread(&ConnectivityMonitor::connectivityMonitorFunction, this);

        if(_instance != nullptr) {
            NMLOG_INFO("connectivity monitor started - eth %s - wlan %s", 
                        _instance->m_ethConnected? "up":"down", _instance->m_wlanConnected? "up":"down");
        }

        return true;
    }

    bool ConnectivityMonitor::stopConnectivityMonitor()
    {
        m_cmRunning = false;
        m_cmCv.notify_one();
        if(m_cmThrdID.joinable())
            m_cmThrdID.join();
        m_InternetState = INTERNET_UNKNOWN;
        m_Ipv4InternetState = INTERNET_UNKNOWN;
        m_Ipv6InternetState = INTERNET_UNKNOWN;
        NMLOG_INFO("connectivity monitor stopping ...");
        return true;
    }

    bool ConnectivityMonitor::switchToInitialCheck()
    {
        m_switchToInitial = true;
        m_notify = true; // m_notify internet state because some network state change may happen
        m_cmCv.notify_one();
        if(_instance != nullptr) {
            NMLOG_INFO("switching to initial check - eth %s - wlan %s",
                        _instance->m_ethConnected? "up":"down", _instance->m_wlanConnected? "up":"down");
        }
        return true;
    }

    void ConnectivityMonitor::notifyInternetStatusChangedEvent(Exchange::INetworkManager::InternetStatus newInternetState)
    {
        static Exchange::INetworkManager::InternetStatus oldState = INTERNET_UNKNOWN;
        if(_instance != nullptr)
        {
            NMLOG_INFO("notifying internet state %s", getInternetStateString(newInternetState));
            Exchange::INetworkManager::InternetStatus newState = newInternetState;
            _instance->ReportInternetStatusChange(oldState , newState);
            m_InternetState = newInternetState;
            oldState = newState; // 'm_InternetState' not exactly previous state, it may change to unknow when interface changed
        }
        else
            NMLOG_FATAL("NetworkManagerImplementation Instance NULL notifyInternetStatusChange failed.");
    }

    void ConnectivityMonitor::connectivityMonitorFunction()
    {
        int timeoutInSec = NMCONNECTIVITY_MONITOR_MIN_INTERVAL;
        Exchange::INetworkManager::InternetStatus currentInternetState = INTERNET_NOT_AVAILABLE;
        int InitialRetryCount = 0;
        int IdealRetryCount = 0;
        m_switchToInitial = true;
        m_InternetState = INTERNET_UNKNOWN;
        m_Ipv4InternetState = INTERNET_UNKNOWN;
        m_Ipv6InternetState = INTERNET_UNKNOWN;
        m_notify = true;

        while (m_cmRunning) {
            // Check if no interfaces are connected
            if (_instance != nullptr && !_instance->m_ethConnected && !_instance->m_wlanConnected) {
                NMLOG_DEBUG("no interface connected, no ccm check");
                timeoutInSec = NMCONNECTIVITY_MONITOR_MIN_INTERVAL;
                m_InternetState = INTERNET_NOT_AVAILABLE;
                m_Ipv4InternetState = INTERNET_NOT_AVAILABLE;
                m_Ipv6InternetState = INTERNET_NOT_AVAILABLE;
                currentInternetState = INTERNET_NOT_AVAILABLE;
                if (InitialRetryCount == 0)
                    m_notify = true;
                InitialRetryCount = 1;
            }
            // Initial case monitoring
            else if (m_switchToInitial) {
                NMLOG_INFO("Initial cm check - retry count %d - %s", InitialRetryCount, getInternetStateString(m_InternetState));
                timeoutInSec = NMCONNECTIVITY_MONITOR_MIN_INTERVAL;

                // Lambda functions to check connectivity for IPv4 and IPv6
                auto curlCheckThrdIpv4 = [&]() {
                    TestConnectivity testInternet(m_endpoint(), NMCONNECTIVITY_CURL_REQUEST_TIMEOUT_MS,
                                                  NMCONNECTIVITY_CURL_HEAD_REQUEST, IP_ADDRESS_V4);
                    m_Ipv4InternetState = testInternet.getInternetState();
                    if(m_Ipv6InternetState == INTERNET_CAPTIVE_PORTAL)
                        m_captiveURI = testInternet.getCaptivePortal();
                };

                auto curlCheckThrdIpv6 = [&]() {
                    TestConnectivity testInternet(m_endpoint(), NMCONNECTIVITY_CURL_REQUEST_TIMEOUT_MS,
                                                  NMCONNECTIVITY_CURL_HEAD_REQUEST, IP_ADDRESS_V6);
                    m_Ipv6InternetState = testInternet.getInternetState();
                    if(m_Ipv6InternetState == INTERNET_CAPTIVE_PORTAL)
                        m_captiveURI = testInternet.getCaptivePortal();
                };

                // Start threads for IPv4 and IPv6 checks
                std::thread ipv4thread(curlCheckThrdIpv4);
                std::thread ipv6thread(curlCheckThrdIpv6);

                // Wait for both threads to finish
                ipv4thread.join();
                ipv6thread.join();

                // Determine the current internet state based on the results
                if (m_Ipv4InternetState == INTERNET_NOT_AVAILABLE && m_Ipv6InternetState == INTERNET_NOT_AVAILABLE) {
                    currentInternetState = INTERNET_NOT_AVAILABLE;
                    if (InitialRetryCount == 0)
                        m_notify = true;
                    InitialRetryCount = 1; // continue same check for 5 sec
                } else {
                    if (m_Ipv4InternetState == INTERNET_FULLY_CONNECTED || m_Ipv6InternetState == INTERNET_FULLY_CONNECTED)
                    {
                        currentInternetState = INTERNET_FULLY_CONNECTED;
                        m_ipversion = (m_Ipv4InternetState == INTERNET_FULLY_CONNECTED) ? IP_ADDRESS_V4 : IP_ADDRESS_V6;
                    }
                    else if (m_Ipv4InternetState == INTERNET_CAPTIVE_PORTAL || m_Ipv6InternetState == INTERNET_CAPTIVE_PORTAL)
                    {
                        currentInternetState = INTERNET_CAPTIVE_PORTAL;
                        m_ipversion = (m_Ipv4InternetState == INTERNET_CAPTIVE_PORTAL) ? IP_ADDRESS_V4 : IP_ADDRESS_V6;
                    } else if (m_Ipv4InternetState == INTERNET_LIMITED || m_Ipv6InternetState == INTERNET_LIMITED) {
                        currentInternetState = INTERNET_LIMITED;
                        m_ipversion = (m_Ipv4InternetState == INTERNET_LIMITED) ? IP_ADDRESS_V4 : IP_ADDRESS_V6;
                    } else {
                        currentInternetState = INTERNET_NOT_AVAILABLE;
                        m_ipversion = IP_ADDRESS_V4;
                    }

                    if (InitialRetryCount == 0)
                        m_notify = true;

                    if (currentInternetState != m_InternetState) {
                        NMLOG_DEBUG("initial connectivity state change %s", getInternetStateString(m_InternetState));
                        m_InternetState = currentInternetState;
                        InitialRetryCount = 1; // reset retry count to get continuous 3 same state
                    }
                    InitialRetryCount++;
                }

                if (InitialRetryCount > NM_CONNECTIVITY_MONITOR_RETRY_COUNT) {
                    m_switchToInitial = false;
                    m_notify = true;
                    InitialRetryCount = 0;
                    IdealRetryCount = 0;
                }
            }
            // Ideal case monitoring
            else {

                timeoutInSec = NMCONNECTIVITY_MONITOR_RETRY_INTERVAL;
                InitialRetryCount = 0;

                TestConnectivity testInternet(m_endpoint(), NMCONNECTIVITY_CURL_REQUEST_TIMEOUT_MS,
                                                NMCONNECTIVITY_CURL_HEAD_REQUEST, m_ipversion);
                currentInternetState = testInternet.getInternetState();

                if (currentInternetState == INTERNET_CAPTIVE_PORTAL) // if captive portal found copy the URL
                    m_captiveURI = testInternet.getCaptivePortal();

                if (currentInternetState != m_InternetState)
                {
                    if (currentInternetState == INTERNET_NOT_AVAILABLE && m_InternetState == INTERNET_FULLY_CONNECTED)
                    {
                        DnsResolver dnsResolver(getConnectivityMonitorEndpoints()[0], m_ipversion, testInternet.getCurlError() ); // only first endpoint with specific ipversion
                        if (dnsResolver()) {
                            NMLOG_INFO("DNS resolved, success !!!");
                            IdealRetryCount = 0;
                            currentInternetState = INTERNET_FULLY_CONNECTED;
                        }
                        else
                        {
                            NMLOG_WARNING("DNS resolve failed !!!");
                            timeoutInSec = NMCONNECTIVITY_MONITOR_MIN_INTERVAL; // retry in 5 sec
                            IdealRetryCount++;
                            if (IdealRetryCount >= NM_CONNECTIVITY_MONITOR_RETRY_COUNT) {
                                IdealRetryCount = 0;
                                m_InternetState = INTERNET_NOT_AVAILABLE;
                                m_Ipv4InternetState = INTERNET_NOT_AVAILABLE;
                                m_Ipv6InternetState = INTERNET_NOT_AVAILABLE; // reset all states to NO_INTERNET
                                currentInternetState = INTERNET_NOT_AVAILABLE;
                                m_switchToInitial = true; // switch to initial check after 3 retries
                                InitialRetryCount = 1; // reset initial retry count it will not post the event
                                m_notify = true;
                                NMLOG_DEBUG("No internet retrying completed notifying !!!");
                            } else {
                                NMLOG_INFO("No internet retrying connection check %d ...", IdealRetryCount);
                            }
                        }
                    } else {
                        // a state change happened but it is not fully connected to no internet
                        // switch to initial check to get the correct state
                        m_switchToInitial = true;
                        InitialRetryCount = 0;
                        IdealRetryCount = 0;

                        if (m_ipversion == IP_ADDRESS_V4)
                            m_Ipv4InternetState = currentInternetState;
                        else if (m_ipversion == IP_ADDRESS_V4)
                            m_Ipv6InternetState = currentInternetState;
                        else
                            m_InternetState = currentInternetState;
                    }
                } else { // ideal case no change in network state
                    IdealRetryCount = 0;
                }
            }

            if (m_notify) {
                m_InternetState = currentInternetState;
                notifyInternetStatusChangedEvent(m_InternetState);
                m_notify = false;
            }

            if (!m_cmRunning)
                break;

            // Wait for next interval
            std::unique_lock<std::mutex> lock(m_cmMutex);
            if (m_cmCv.wait_for(lock, std::chrono::seconds(timeoutInSec)) != std::cv_status::timeout) {
                NMLOG_INFO("connectivity monitor received signal. skipping %d sec interval", timeoutInSec);
            }
        }
    }

    } // namespace Plugin
} // namespace WPEFramework

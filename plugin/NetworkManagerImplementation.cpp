/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#include <thread>
#include <chrono>
#include "NetworkManagerImplementation.h"
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using namespace NetworkManagerLogger;

#define CIDR_NETMASK_IP_LEN 32
#define SSID_COMMAND        "wpa_cli status"
#define BSS_COMMAND         "wpa_cli bss"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(NetworkManagerImplementation, NETWORKMANAGER_MAJOR_VERSION, NETWORKMANAGER_MINOR_VERSION, NETWORKMANAGER_PATCH_VERSION);

        NetworkManagerImplementation::NetworkManagerImplementation()
            : _notificationCallbacks({})
        {
            /* Initialize STUN Endpoints */
            m_stunEndpoint = "stun.l.google.com";
            m_stunPort = 19302;
            m_stunBindTimeout = 30;
            m_stunCacheTimeout = 0;
            m_defaultInterface = "";
            m_publicIP = "";
            m_ethConnected = false;
            m_wlanConnected = false;

            /* Set NetworkManager Out-Process name to be NWMgrPlugin */
            Core::ProcessInfo().Name("NWMgrPlugin");

            /* Initialize Network Manager */
            NetworkManagerLogger::Init();
            NMLOG_INFO((_T("NWMgrPlugin Out-Of-Process Instantiation; SHA: " _T(EXPAND_AND_QUOTE(PLUGIN_BUILD_REFERENCE)))));
            m_processMonThread = std::thread(&NetworkManagerImplementation::processMonitor, this, NM_PROCESS_MONITOR_INTERVAL_SEC);
        }

        NetworkManagerImplementation::~NetworkManagerImplementation()
        {
            NMLOG_INFO("NetworkManager Out-Of-Process Shutdown/Cleanup");
            if(m_registrationThread.joinable())
            {
                m_registrationThread.join();
            }
            stopWiFiSignalQualityMonitor();

            {
                std::unique_lock<std::mutex> lock(m_processMonMutex);
                m_processMonThreadStop = true;
                m_processMonCondVar.notify_one();
            }

            if (m_processMonThread.joinable()) {
                m_processMonThread.join();
            }
        }

        /**
         * Register a notification callback
         */
        uint32_t NetworkManagerImplementation::Register(INetworkManager::INotification *notification)
        {
            LOG_ENTRY_FUNCTION();
            _notificationLock.Lock();

            // Make sure we can't register the same notification callback multiple times
            if (std::find(_notificationCallbacks.begin(), _notificationCallbacks.end(), notification) == _notificationCallbacks.end()) {
                _notificationCallbacks.push_back(notification);
                notification->AddRef();
            }

            _notificationLock.Unlock();

            return Core::ERROR_NONE;
        }

        /**
         * Unregister a notification callback
         */
        uint32_t NetworkManagerImplementation::Unregister(INetworkManager::INotification *notification)
        {
            LOG_ENTRY_FUNCTION();
            _notificationLock.Lock();

            // Make sure we can't register the same notification callback multiple times
            auto itr = std::find(_notificationCallbacks.begin(), _notificationCallbacks.end(), notification);
            if (itr != _notificationCallbacks.end()) {
                (*itr)->Release();
                _notificationCallbacks.erase(itr);
            }

            _notificationLock.Unlock();

            return Core::ERROR_NONE;
        }

        uint32_t NetworkManagerImplementation::Configure(PluginHost::IShell* service)
        {
            LOG_ENTRY_FUNCTION();
            Configuration config;
            if (service)
            {
                string configLine = service->ConfigLine();
                if(configLine.empty())
                {
                    NMLOG_FATAL("config line : is empty !");
                    return Core::ERROR_GENERAL;
                }
                else
                {
                    NMLOG_INFO("Loading the incoming configuration : %s", configLine.c_str());
                    config.FromString(configLine);
                }
            }
            else
            {
                NMLOG_FATAL("Service is NULL!");
                return Core::ERROR_GENERAL;

            }

            NetworkManagerLogger::SetLevel(static_cast <NetworkManagerLogger::LogLevel>(config.loglevel.Value()));
            NMLOG_DEBUG("loglevel %d", config.loglevel.Value());

            /* STUN configuration copy */
            m_stunEndpoint = config.stun.stunEndpoint.Value();
            m_stunPort = config.stun.port.Value();
            m_stunBindTimeout = config.stun.interval.Value();

            NMLOG_INFO("stun endpoint %s", m_stunEndpoint.c_str());
            NMLOG_DEBUG("stun port %d", m_stunPort);
            NMLOG_DEBUG("stun interval %d", m_stunBindTimeout);


            /* Connectivity monitor endpoints configuration */
            std::vector<std::string> connectEndpts;
            if(!config.connectivityConf.endpoint_1.Value().empty()) {
                NMLOG_INFO("connectivity endpoint 1 %s", config.connectivityConf.endpoint_1.Value().c_str());
                connectEndpts.push_back(config.connectivityConf.endpoint_1.Value().c_str());
            }
            if(!config.connectivityConf.endpoint_2.Value().empty()) {
                NMLOG_DEBUG("connectivity endpoint 2 %s", config.connectivityConf.endpoint_2.Value().c_str());
                connectEndpts.push_back(config.connectivityConf.endpoint_2.Value().c_str());
            }
            if(!config.connectivityConf.endpoint_3.Value().empty()) {
                NMLOG_DEBUG("connectivity endpoint 3 %s", config.connectivityConf.endpoint_3.Value().c_str());
                connectEndpts.push_back(config.connectivityConf.endpoint_3.Value().c_str());
            }
            if(!config.connectivityConf.endpoint_4.Value().empty()) {
                NMLOG_DEBUG("connectivity endpoint 4 %s", config.connectivityConf.endpoint_4.Value().c_str());
                connectEndpts.push_back(config.connectivityConf.endpoint_4.Value().c_str());
            }
            if(!config.connectivityConf.endpoint_5.Value().empty()) {
                NMLOG_DEBUG("connectivity endpoint 5 %s", config.connectivityConf.endpoint_5.Value().c_str());
                connectEndpts.push_back(config.connectivityConf.endpoint_5.Value().c_str());
            }

            /* check whether the endpoint is already loaded from Cache; if Yes, do not use the one from configuration */
            if (connectivityMonitor.getConnectivityMonitorEndpoints().size() < 1)
            {
                NMLOG_INFO("Use the connectivity endpoint from config");
                connectivityMonitor.setConnectivityMonitorEndpoints(connectEndpts);
            }
            else if (connectEndpts.size() < 1)
            {
                std::vector<std::string> backup;
                NMLOG_INFO("Connectivity endpoints are empty in config; use the default");
                backup.push_back("http://clients3.google.com/generate_204");
                connectivityMonitor.setConnectivityMonitorEndpoints(backup);
            }

            /* As all the configuration is set, lets instantiate platform */
            NetworkManagerImplementation::platform_init();
            /* change gnome networkmanager or netsrvmgr logg level */
            NetworkManagerImplementation::platform_logging(static_cast <NetworkManagerLogger::LogLevel>(config.loglevel.Value()));
            return(Core::ERROR_NONE);
        }

        /* @brief Get STUN Endpoint to be used for identifying Public IP */
        uint32_t NetworkManagerImplementation::GetStunEndpoint (string &endpoint /* @out */, uint32_t& port /* @out */, uint32_t& bindTimeout /* @out */, uint32_t& cacheTimeout /* @out */) const
        {
            LOG_ENTRY_FUNCTION();
            endpoint = m_stunEndpoint;
            port = m_stunPort;
            bindTimeout = m_stunBindTimeout;
            cacheTimeout = m_stunCacheTimeout;
            return Core::ERROR_NONE;
        }

        /* @brief Set STUN Endpoint to be used to identify Public IP */
        uint32_t NetworkManagerImplementation::SetStunEndpoint (string const endpoint /* @in */, const uint32_t port /* @in */, const uint32_t bindTimeout /* @in */, const uint32_t cacheTimeout /* @in */)
        {
            LOG_ENTRY_FUNCTION();
            if (!endpoint.empty())
                m_stunEndpoint = endpoint;

            if (0 != port)
                m_stunPort = port;

            if (0 != bindTimeout)
                m_stunBindTimeout = bindTimeout;

            if (0 != cacheTimeout)
                m_stunCacheTimeout = cacheTimeout;

            return Core::ERROR_NONE;
        }

        /* @brief Get ConnectivityTest Endpoints */
        uint32_t NetworkManagerImplementation::GetConnectivityTestEndpoints(IStringIterator*& endpoints/* @out */) const
        {
            LOG_ENTRY_FUNCTION();
            std::vector<std::string> tmpEndpoints = connectivityMonitor.getConnectivityMonitorEndpoints();
            endpoints = (Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(tmpEndpoints));

            return Core::ERROR_NONE;
        }

        /* @brief Set ConnectivityTest Endpoints */
        uint32_t NetworkManagerImplementation::SetConnectivityTestEndpoints(IStringIterator* const endpoints /* @in */)
        {
            LOG_ENTRY_FUNCTION();
            std::vector<std::string> tmpEndpoints;

            if(endpoints && (endpoints->Count() >= 1))
            {
                string endpoint{};
                while(endpoints->Next(endpoint))
                {
                    /* The url must be atleast 7 letters to be a valid `http://` url */
                    if(!endpoint.empty() && endpoint.size() > 7)
                    {
                        tmpEndpoints.push_back(endpoint);
                    }
                }
                connectivityMonitor.setConnectivityMonitorEndpoints(tmpEndpoints);
            }
            return Core::ERROR_NONE;
        }

        /* @brief Get Internet Connectivty Status */ 
        uint32_t NetworkManagerImplementation::IsConnectedToInternet(string &ipversion /* @inout */, string &interface /* @inout */, InternetStatus &result /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            Exchange::INetworkManager::IPVersion curlIPversion = Exchange::INetworkManager::IP_ADDRESS_V4;
            bool ipVersionNotSpecified = false;

            if(!ipversion.empty() && ipversion != "IPv4" && ipversion != "IPv6")
            {
                NMLOG_WARNING("Invalid Ipversion argument %s", ipversion.c_str());
                return Core::ERROR_BAD_REQUEST;
            }
            else
            {
                if(ipversion == "IPv4")
                    curlIPversion = Exchange::INetworkManager::IP_ADDRESS_V4;
                else if(ipversion == "IPv6")
                    curlIPversion = Exchange::INetworkManager::IP_ADDRESS_V6;
                else
                    ipVersionNotSpecified = true;
            }

            if(!interface.empty()&& interface != "wlan0" && interface != "eth0")
            {
                NMLOG_WARNING("Invalid Interface argument %s", interface.c_str());
                return Core::ERROR_BAD_REQUEST;
            }

            result = connectivityMonitor.getInternetState(interface, curlIPversion, ipVersionNotSpecified);
            if (Exchange::INetworkManager::IP_ADDRESS_V6 == curlIPversion)
                ipversion = "IPv6";
            else
                ipversion = "IPv4";

            if(interface.empty())
                interface = m_defaultInterface;

            return Core::ERROR_NONE;
        }

        /* @brief Get Authentication URL if the device is behind Captive Portal */ 
        uint32_t NetworkManagerImplementation::GetCaptivePortalURI(string &uri /* @out */) const
        {
            LOG_ENTRY_FUNCTION();
            uri = connectivityMonitor.getCaptivePortalURI();
            return Core::ERROR_NONE;
        }

        /* @brief Get the Public IP used for external world communication */
        uint32_t NetworkManagerImplementation::GetPublicIP (string& interface /* @inout */, string &ipversion /* @inout */,  string& ipaddress /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            stun::bind_result result;
            bool isIPv6 = (ipversion == "IPv6");

            // Either Interface must be connected to get the public IP
            if (!(m_ethConnected | m_wlanConnected))
            {
                NMLOG_WARNING("No interface Connected");
                return Core::ERROR_GENERAL;
            }

            stun::protocol  proto (isIPv6 ? stun::protocol::af_inet6  : stun::protocol::af_inet);
            if(stunClient.bind(m_stunEndpoint, m_stunPort, interface, proto, m_stunBindTimeout, m_stunCacheTimeout, result))
            {
                if (isIPv6)
                    ipversion = "IPv6";
                else
                    ipversion = "IPv4";

                if (interface.empty())
                    interface = m_defaultInterface;

                ipaddress = result.public_ip;
                return Core::ERROR_NONE;
            }
            else
            {
                return Core::ERROR_GENERAL;
            }
        }

        /* @brief Set the network manager plugin log level */
        uint32_t NetworkManagerImplementation::SetLogLevel(const Logging& level /* @in */)
        {
            NetworkManagerLogger::SetLevel(static_cast<NetworkManagerLogger::LogLevel>(level));
            platform_logging(static_cast<NetworkManagerLogger::LogLevel>(level));
            NMLOG_DEBUG("loglevel %d", level);
	        return Core::ERROR_NONE;
        }

        /* @brief Get the network manager plugin log level */
        uint32_t NetworkManagerImplementation::GetLogLevel(Logging& level /* @out */)
        {
            LogLevel inLevel;
            NetworkManagerLogger::GetLevel(inLevel);

            level = static_cast<Logging>(inLevel);
            return Core::ERROR_NONE;
        }

        /* @brief Request for ping and get the response in as event. The GUID used in the request will be returned in the event. */
        uint32_t NetworkManagerImplementation::Ping (const string ipversion /* @in */,  const string endpoint /* @in */, const uint32_t noOfRequest /* @in */, const uint16_t timeOutInSeconds /* @in */, const string guid /* @in */, string& response /* @out */)
        {   
            char cmd[100] = "";
            string tempResult = "";
            if(0 == strcasecmp("IPv6", ipversion.c_str()))
            {   
                snprintf(cmd, sizeof(cmd), "ping6 -c %d -W %d -i 0.2 '%s' 2>&1", noOfRequest, timeOutInSeconds, endpoint.c_str());
            }
            else
            {   
                snprintf(cmd, sizeof(cmd), "ping  -c %d -W %d -i 0.2 '%s' 2>&1", noOfRequest, timeOutInSeconds, endpoint.c_str());
            }
            
            NMLOG_DEBUG ("The Command is %s", cmd);
            string commandToExecute(cmd);
            executeExternally(NETMGR_PING, commandToExecute, tempResult);

            JsonObject temp;
            temp.FromString(tempResult);
            temp["endpoint"] = endpoint;
            temp.ToString(response);

            return Core::ERROR_NONE;
        }

        /* @brief Request for trace get the response in as event. The GUID used in the request will be returned in the event. */
        uint32_t NetworkManagerImplementation::Trace (const string ipversion /* @in */,  const string endpoint /* @in */, const uint32_t noOfRequest /* @in */, const string guid /* @in */, string& response /* @out */)
        {
            char cmd[256] = "";
            string tempResult = "";
            if(0 == strcasecmp("IPv6", ipversion.c_str()))
            {
                snprintf(cmd, 256, "traceroute6 -w 3 -m 6 -q %d %s 64 2>&1", noOfRequest, endpoint.c_str());
            }
            else
            {
                snprintf(cmd, 256, "traceroute -w 3 -m 6 -q %d %s 52 2>&1", noOfRequest, endpoint.c_str());
            }

            NMLOG_DEBUG ("The Command is %s", cmd);
            string commandToExecute(cmd);
            executeExternally(NETMGR_TRACE, commandToExecute, tempResult);

            JsonObject temp;
            temp["endpoint"] = endpoint;
            temp["results"] = tempResult;
            temp.ToString(response);

            return Core::ERROR_NONE;
        }

        void NetworkManagerImplementation::executeExternally(NetworkEvents event, const string commandToExecute, string& response)
        {
            FILE *pipe = NULL;
            string output{};
            char buffer[1024];
            JsonObject pingResult;
            int exitStatus;

            pipe = popen(commandToExecute.c_str(), "r");
            if (pipe == NULL)
            {   
                NMLOG_INFO ("%s: failed to open file '%s' for read mode with result: %s", __FUNCTION__, commandToExecute.c_str(), strerror(errno));
                return;
            }

            if (NETMGR_PING == event)
            {
                while (!feof(pipe) && fgets(buffer, 1024, pipe) != NULL)
                {
                    // remove newline from buffer
                    buffer[strcspn(buffer, "\n")] = '\0';
                    string line(buffer);

                    if( line.find( "packet" ) != string::npos )
                    {
                        //Example: 10 packets transmitted, 10 packets received, 0% packet loss
                        stringstream ss( line );
                        int transCount;
                        ss >> transCount;
                        pingResult["packetsTransmitted"] = transCount;

                        string token;
                        getline( ss, token, ',' );
                        getline( ss, token, ',' );
                        stringstream ss2( token );
                        int rxCount;
                        ss2 >> rxCount;
                        pingResult["packetsReceived"] = rxCount;

                        getline( ss, token, ',' );
                        string prefix = token.substr(0, token.find("%"));
                        pingResult["packetLoss"] = prefix.c_str();

                    }
                    else if( line.find( "min/avg/max" ) != string::npos )
                    {
                        //Example: round-trip min/avg/max = 17.038/18.310/20.197 ms
                        stringstream ss( line );
                        string fullpath;
                        getline( ss, fullpath, '=' );
                        getline( ss, fullpath, '=' );

                        string prefix;
                        int index = fullpath.find("/");
                        if (index >= 0)
                        {
                            prefix = fullpath.substr(0, fullpath.find("/"));
                            pingResult["tripMin"] = prefix.c_str();
                        }

                        index = fullpath.find("/");
                        if (index >= 0)
                        {
                            fullpath = fullpath.substr(index + 1, fullpath.length());
                            prefix = fullpath.substr(0, fullpath.find("/"));
                            pingResult["tripAvg"] = prefix.c_str();
                        }

                        index = fullpath.find("/");
                        if (index >= 0)
                        {
                            fullpath = fullpath.substr(index + 1, fullpath.length());
                            prefix = fullpath.substr(0, fullpath.find("/"));
                            pingResult["tripMax"] = prefix.c_str();
                        }

                        index = fullpath.find("/");
                        if (index >= 0)
                        {
                            fullpath = fullpath.substr(index + 1, fullpath.length());
                            pingResult["tripStdDev"] = fullpath.c_str();
                        }
                    }
                    else if( line.find( "bad" ) != string::npos )
                    {
                        pingResult["success"] = false;
                        pingResult["error"] = "Bad Address";
                    }
                }
                exitStatus = pclose(pipe);
                // Check the exit status to determine if the command was successful
                if (WIFEXITED(exitStatus) && WEXITSTATUS(exitStatus) == 0) {
                    pingResult["success"] = true;
                } else {
                    pingResult["success"] = false;
                    pingResult["error"] = "Could not ping endpoint";
                }

                pingResult.ToString(response);
                NMLOG_INFO("Response is, %s", response.c_str());
            }
            else if (NETMGR_TRACE == event)
            {

                // We return the entire output of the trace command but since this contains newlines it is not valid as
                // a json value so we will parse the output into an array of strings, one element for each line.
                JsonArray list;
                while (!feof(pipe) && fgets(buffer, 1024, pipe) != NULL)
                {
                    // remove newline from buffer
                    buffer[strcspn(buffer, "\n")] = ' ';
                    string line(buffer);
                    list.Add(line);
                }

                pclose(pipe);
                list.ToString(response);
                NMLOG_INFO("Response is, %s", response.c_str());
            }
            return;
        }


        void NetworkManagerImplementation::filterScanResults(JsonArray &ssids)
        {
            JsonArray result;
            double filterFreq = 0.0;
            std::unordered_set<std::string> scanForSsidsSet(m_filterSsidslist.begin(), m_filterSsidslist.end());

            // If neither SSID list nor frequency is provided, exit
            if (m_filterSsidslist.empty() && m_filterfrequency.empty())
            {
                NMLOG_DEBUG("Neither SSID nor Frequency is provided. Exiting function.");
                return;
            }

            if (!m_filterfrequency.empty())
            {
                filterFreq = std::stod(m_filterfrequency);
            }

            for (int i = 0; i < ssids.Length(); i++)
            {
                JsonObject object = ssids[i].Object();
                string ssid = object["ssid"].String();
                string frequency = object["frequency"].String();


                double frequencyValue = std::stod(frequency);
                bool ssidMatches = scanForSsidsSet.empty() || scanForSsidsSet.find(ssid) != scanForSsidsSet.end();
                bool freqMatches = m_filterfrequency.empty() || (filterFreq == frequencyValue);

                if (ssidMatches && freqMatches)
                    result.Add(object);
            }
            ssids = result;
            NMLOG_DEBUG("After filtering, found %d SSIDs.", ssids.Length());
        }

        // WiFi Specific Methods
        /* @brief Initiate a WIFI Scan; This is Async method and returns the scan results as Event */
        uint32_t NetworkManagerImplementation::GetSupportedSecurityModes(ISecurityModeIterator*& securityModes /* @out */) const
        {
            LOG_ENTRY_FUNCTION();
            std::vector<WIFISecurityModeInfo> modeInfo {
                                                            {WIFI_SECURITY_NONE,     "NONE"},
                                                            {WIFI_SECURITY_WPA_PSK,  "WPA_PSK"},
                                                            {WIFI_SECURITY_SAE,      "SAE"},
                                                            {WIFI_SECURITY_EAP,      "EAP"},
                                                        };

            using Implementation = RPC::IteratorType<Exchange::INetworkManager::ISecurityModeIterator>;
            securityModes = Core::Service<Implementation>::Create<Exchange::INetworkManager::ISecurityModeIterator>(modeInfo);

            return Core::ERROR_NONE;
        }

        void NetworkManagerImplementation::ReportInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface)
        {
            LOG_ENTRY_FUNCTION();
            if(Exchange::INetworkManager::INTERFACE_LINK_DOWN == state || Exchange::INetworkManager::INTERFACE_REMOVED == state)
            {
                if(interface == "eth0")
                    m_ethConnected = false;
                else if(interface == "wlan0")
                    m_wlanConnected = false;
                connectivityMonitor.switchToInitialCheck();
            }

            /* Only the Ethernet connection status is changing here. The WiFi status is updated in the WiFi state callback. */
            if(Exchange::INetworkManager::INTERFACE_LINK_UP == state && interface == "eth0")
                m_ethConnected = true;

            _notificationLock.Lock();
            NMLOG_INFO("Posting onInterfaceChange %s - %u", interface.c_str(), (unsigned)state);
            for (const auto callback : _notificationCallbacks) {
                callback->onInterfaceStateChange(state, interface);
            }
            _notificationLock.Unlock();
        }

        void NetworkManagerImplementation::ReportActiveInterfaceChange(const string prevActiveInterface, const string currentActiveinterface)
        {
            _notificationLock.Lock();
            NMLOG_INFO("Posting onActiveInterfaceChange %s", currentActiveinterface.c_str());

            if(currentActiveinterface == "eth0")
                m_ethConnected = true;
            else if (currentActiveinterface == "wlan0")
                m_wlanConnected = true;

            // FIXME : This could be the place to define `m_defaultInterface` to incoming `currentActiveinterface`.
            // m_defaultInterface = currentActiveinterface;

            for (const auto callback : _notificationCallbacks) {
                callback->onActiveInterfaceChange(prevActiveInterface, currentActiveinterface);
            }
            _notificationLock.Unlock();
        }

        void NetworkManagerImplementation::ReportIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status)
        {
            LOG_ENTRY_FUNCTION();
            if (Exchange::INetworkManager::IP_ACQUIRED == status) {
                // Switch the connectivity monitor to initial check
                // if ipaddress is aquired means there should be interface connected
                if(interface == "eth0")
                    m_ethConnected = true;
                else if(interface == "wlan0")
                    m_wlanConnected = true;

                // FIXME : Availability of ip address for a given interface does not mean that its the default interface. This hardcoding will work for RDKProxy but not for Gnome.
                if (m_ethConnected && m_wlanConnected)
                    m_defaultInterface = "eth0";
                else
                    m_defaultInterface = interface;

                connectivityMonitor.switchToInitialCheck();
            }

            _notificationLock.Lock();
            NMLOG_INFO("Posting onIPAddressChange %s - %s", ipaddress.c_str(), interface.c_str());
            for (const auto callback : _notificationCallbacks) {
                callback->onIPAddressChange(interface, ipversion, ipaddress, status);
            }
            _notificationLock.Unlock();
        }

        void NetworkManagerImplementation::ReportInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState)
        {
            _notificationLock.Lock();
            NMLOG_INFO("Posting onInternetStatusChange with current state as %u", (unsigned)currState);
            for (const auto callback : _notificationCallbacks) {
                callback->onInternetStatusChange(prevState, currState);
            }
            _notificationLock.Unlock();
        }

        int32_t NetworkManagerImplementation::logSSIDs(Logging level, const JsonArray &ssids)
        {
            Logging inLevel;
            GetLogLevel(inLevel);
            if (level > inLevel)
                return ssids.Length();

            printf ("{\n"); fflush(stdout);
            for (int i = 0; i < ssids.Length(); i++)
            {
                JsonObject ssid = ssids[i].Object();
                string json;
                ssid.ToString(json);
                printf("\t%s\n", json.c_str()); fflush(stdout);
            }
            printf("}\n"); fflush(stdout);
            return ssids.Length();
        }

        void NetworkManagerImplementation::ReportAvailableSSIDs(const JsonArray &arrayofWiFiScanResults)
        {
            _notificationLock.Lock();
            string jsonOfWiFiScanResults;
            string jsonOfFilterScanResults;
            JsonArray filterResult = arrayofWiFiScanResults;

            arrayofWiFiScanResults.ToString(jsonOfWiFiScanResults);
            NMLOG_DEBUG("Discovered %d SSIDs before filtering as,", filterResult.Length());
            logSSIDs(LOG_LEVEL_DEBUG, filterResult);

            filterScanResults(filterResult);
            filterResult.ToString(jsonOfFilterScanResults);

            NMLOG_INFO("Posting onAvailableSSIDs event with %d SSIDs as,", filterResult.Length());
            logSSIDs(LOG_LEVEL_INFO, filterResult);

            for (const auto callback : _notificationCallbacks) {
                callback->onAvailableSSIDs(jsonOfFilterScanResults);
            }
            _notificationLock.Unlock();
        }

        void NetworkManagerImplementation::startWiFiSignalQualityMonitor(int interval)
        {
            if (m_isRunning) {
                NMLOG_INFO("WiFiSignalQualityMonitor Thread is already running.");
                return;
            }
            m_isRunning = true;
            m_stopThread = false;
            try {
                m_monitorThread = std::thread(&NetworkManagerImplementation::monitorThreadFunction, this, interval);
                NMLOG_INFO("monitorThreadFunction thread creation successful");
            } catch (const std::exception& err) {
                NMLOG_INFO("monitorThreadFunction thread creation failed: %s\n", err.what());
            }
        }

        void NetworkManagerImplementation::stopWiFiSignalQualityMonitor()
        {
            if (!m_isRunning) {
                return; // No thread to stop
            }
            {
                std::unique_lock<std::mutex> lock(m_condVariableMutex);
                m_stopThread = true;
                m_condVariable.notify_one();
            }
            if (m_monitorThread.joinable()) {
                m_monitorThread.join();
            }
            m_isRunning = false;
        }

        /* The below implementation of GetWiFiSignalQuality is a temporary mitigation. Need to be revisited */
        uint32_t NetworkManagerImplementation::GetWiFiSignalQuality(string& ssid /* @out */, string& strength /* @out */, string& noise /* @out */, string& snr /* @out */, WiFiSignalQuality& quality /* @out */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            int16_t readSnr = 0;

            std::string key, value;
            int16_t readNoise = 0;
            string bssid{};
            char buff[512] = {'\0'};

            FILE *fp = NULL;

            /* SSID */
            fp = popen(SSID_COMMAND, "r");
            if (!fp)
            {
                NMLOG_ERROR("Failed in getting output from command %s",SSID_COMMAND);
                return Core::ERROR_RPC_CALL_FAILED;
            }
            while ((!feof(fp)) && (fgets(buff, sizeof (buff), fp) != NULL))                                                                     {
                std::istringstream mystream(buff);
                if(std::getline(std::getline(mystream, key, '=') >> std::ws, value))
                    if (key == "bssid") {
                        bssid = value;
                        break;
                    }
            }
            pclose(fp);

            /* Noise n RSSI */
            std::string bssCommand = std::string(BSS_COMMAND) + " " + bssid;
            fp = popen(bssCommand.c_str(), "r");
            if (!fp)
            {
                NMLOG_ERROR("Failed in getting output from command %s",BSS_COMMAND);
                return Core::ERROR_RPC_CALL_FAILED;
            }
            while ((!feof(fp)) && (fgets(buff, sizeof (buff), fp) != NULL))
            {
                std::istringstream mystream(buff);
                if(std::getline(std::getline(mystream, key, '=') >> std::ws, value))
                    if (key == "level") {
                        strength = value;
                    }
                    else if (key == "noise") {
                        noise = value;
                    }
                    else if (key == "ssid") {
                        ssid = value;
                    }
                    else if (key == "snr") {
                        snr = value;
                    }
                    if (!strength.empty() && !noise.empty() && !ssid.empty() && !snr.empty())
                        break;
            }
            pclose(fp);

            /* NOTE: The std::stoi() will throw exception if the string input is empty; so set to 0 */
            if (noise.empty())
                noise= "0";
            if (snr.empty())
                snr = "0";
            if (strength.empty())
                strength = "0";

            readNoise = std::stoi(noise);
            readSnr = std::stoi(snr);

            /* Check the Noise is within range */
            if(!(readNoise < 0 && readNoise >= DEFAULT_NOISE))
            {
                NMLOG_WARNING("Received Noise (%d) from wifi driver is not valid", readNoise);
                noise = "0";
            }

            /* mapping rssi value when the SNR value is not proper */
            if(!(readSnr > 0 && readSnr <= MAX_SNR_VALUE))
            {
                NMLOG_WARNING("Received SNR (%d) from wifi driver is not valid; Lets map with RSSI (%s)", readSnr, strength.c_str());
                readSnr = std::stoi(strength);
                /* Take the absolute value */
                readSnr = (readSnr < 0) ? -readSnr : readSnr;

                snr = std::to_string(readSnr);
            }

            NMLOG_INFO("RSSI: %s dBm; Noise: %s dBm; SNR: %s dBm", strength.c_str(), noise.c_str(), snr.c_str());

            if (readSnr == 0)
            {
                quality = WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED;
                strength = "0";
            }
            else if (readSnr > 0 && readSnr < NM_WIFI_SNR_THRESHOLD_FAIR)
            {
                quality = WiFiSignalQuality::WIFI_SIGNAL_WEAK;
            }
            else if (readSnr > NM_WIFI_SNR_THRESHOLD_FAIR && readSnr < NM_WIFI_SNR_THRESHOLD_GOOD)
            {
                quality = WiFiSignalQuality::WIFI_SIGNAL_FAIR;
            }
            else if (readSnr > NM_WIFI_SNR_THRESHOLD_GOOD && readSnr < NM_WIFI_SNR_THRESHOLD_EXCELLENT)
            {
                quality = WiFiSignalQuality::WIFI_SIGNAL_GOOD;
            }
            else
            {
                quality = WiFiSignalQuality::WIFI_SIGNAL_EXCELLENT;
            }

            rc = Core::ERROR_NONE;

            return rc;
        }

        void NetworkManagerImplementation::processMonitor(uint16_t interval)
        {
            pid_t pid = getpid();

            string path = "/proc/";
            path += std::to_string(pid);
            path += "/statm";
            const uint32_t pageSize = getpagesize();

            do
            {
                int fd;
                char buffer[128] = "";
                int processSize = 0;
                int processRSS = 0;
                int processShare = 0;

                if ((fd = open(path.c_str(), O_RDONLY)) >= 0)
                {
                    ssize_t readAmount = 0;
                    if ((readAmount = read(fd, buffer, sizeof(buffer))) > 0)
                    {
                        ssize_t nulIndex = std::min(readAmount, static_cast<ssize_t>(sizeof(buffer) - 1));
                        buffer[nulIndex] = '\0';
                        sscanf(buffer, "%d %d %d", &processSize, &processRSS, &processShare);

                        /* Update the sizes */
                        processSize  *= pageSize;
                        processRSS   *= pageSize;
                        processShare *= pageSize;

                        /* Convert to KB */
                        processSize  >>= 10;
                        processRSS   >>= 10;
                        processShare >>= 10;
                    }
                    close(fd);
                    NMLOG_INFO("VSS = %d KB   RSS = %d KB", processSize, processRSS);
                }

                std::unique_lock<std::mutex> lock(m_processMonMutex);
                // Wait for the specified interval or until notified to stop
                if (m_processMonCondVar.wait_for(lock, std::chrono::seconds(interval), [this](){ return m_processMonThreadStop == true; }))
                {
                    NMLOG_INFO("Received stop signal");
                }
            } while (!m_processMonThreadStop);

            return;
        }

        void NetworkManagerImplementation::monitorThreadFunction(int interval)
        {
            static Exchange::INetworkManager::WiFiSignalQuality oldSignalQuality = Exchange::INetworkManager::WIFI_SIGNAL_DISCONNECTED;
            NMLOG_INFO("WiFiSignalQualityMonitor thread started ! (%d)", interval);
            while (!m_stopThread) {
                std::string ssid{};
                std::string strength{};
                std::string noise{};
                std::string snr{};
                Exchange::INetworkManager::WiFiSignalQuality newSignalQuality;

                GetWiFiSignalQuality(ssid, strength, noise, snr, newSignalQuality);

                m_lastConnectedSSID = ssid; // last connected ssid used in wifiConnect

                if (oldSignalQuality != newSignalQuality) {
                    oldSignalQuality = newSignalQuality;
                    NetworkManagerImplementation::ReportWiFiSignalQualityChange(ssid, strength, noise, snr, newSignalQuality);
                }

                if (newSignalQuality == Exchange::INetworkManager::WIFI_SIGNAL_DISCONNECTED) {
                    NMLOG_WARNING("WiFiSignalQualityChanged to disconnect - WiFiSignalQualityMonitor exiting");
                    m_stopThread = true; // Signal thread to stop
                    break; // Exit the loop
                }

                std::unique_lock<std::mutex> lock(m_condVariableMutex);
                // Wait for the specified interval or until notified to stop
                if (m_condVariable.wait_for(lock, std::chrono::seconds(interval), [this](){ return m_stopThread == true; }))
                {
                    NMLOG_INFO("WiFiSignalQualityMonitor received stop signal or timed out");
                }
            }
            m_isRunning = false;
        }

        void NetworkManagerImplementation::ReportWiFiStateChange(const Exchange::INetworkManager::WiFiState state)
        {
            /* start signal strength monitor when wifi connected */
            if(INetworkManager::WiFiState::WIFI_STATE_CONNECTED == state)
            {
                m_wlanConnected = true;
                if (!m_monitoringStarted)
                {
                    startWiFiSignalQualityMonitor(DEFAULT_WIFI_SIGNAL_TEST_INTERVAL_SEC);
                    m_monitoringStarted = true;
                }
            }
            else
            {
                if (m_monitoringStarted)
                {
                    stopWiFiSignalQualityMonitor();
                    m_monitoringStarted = false;
                }
                m_wlanConnected = false; /* Any other state is considered as WiFi not connected. */
            }

            _notificationLock.Lock();
            NMLOG_INFO("Posting onWiFiStateChange (%d)", state);
            for (const auto callback : _notificationCallbacks) {
                callback->onWiFiStateChange(state);
            }
            _notificationLock.Unlock();
        }

        void NetworkManagerImplementation::ReportWiFiSignalQualityChange(const string ssid, const string strength, const string noise, const string snr, const Exchange::INetworkManager::WiFiSignalQuality quality)
        {
            _notificationLock.Lock();
            NMLOG_INFO("Posting onWiFiSignalQualityChange %s", strength.c_str());
            for (const auto callback : _notificationCallbacks) {
                callback->onWiFiSignalQualityChange(ssid, strength, noise, snr, quality);
            }
            _notificationLock.Unlock();
        }
    }
}

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
#define SIGNAL_POLL_COMMAND "wpa_cli signal_poll"

namespace WPEFramework
{
    namespace Plugin
    {
        extern NetworkManagerImplementation* _instance;
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
            m_ethConnected.store(false);
            m_wlanConnected.store(false);
            m_ethEnabled.store(false);
            m_wlanEnabled.store(false);

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
            connectivityMonitor.stopConnectivityMonitor();
            _instance = nullptr;
            if(m_registrationThread.joinable())
            {
                m_registrationThread.join();
            }
            /* Stop WiFi Signal Monitoring */
            stopWiFiSignalQualityMonitor();

            {
                std::unique_lock<std::mutex> lock(m_processMonMutex);
                m_processMonThreadStop.store(true);
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

        uint32_t NetworkManagerImplementation::Configure(const string configLine)
        {
            LOG_ENTRY_FUNCTION();
            Configuration config;
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

            // If no parameters are provided, return error
            if (endpoint.empty() && port == 0 && bindTimeout == 0 && cacheTimeout == 0) {
                NMLOG_WARNING("No parameters provided to update STUN Endpoint");
                return Core::ERROR_BAD_REQUEST;
            }

            // If port is provided but is 0, reject as invalid
            if (port == 0 && !endpoint.empty()) {
                NMLOG_WARNING("Invalid STUN port: 0");
                return Core::ERROR_BAD_REQUEST;
            }

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

        /* @brief Get the active Interface used for external world communication */
        uint32_t NetworkManagerImplementation::GetPrimaryInterface (string& interface /* @out */)
        {
            if(m_ethEnabled.load() && m_ethConnected.load())
                interface = "eth0";
            else if(m_wlanEnabled.load() && m_wlanConnected.load())
                interface = "wlan0";
            else
                interface = ""; // no active interface

            NMLOG_DEBUG("Primary interface: %s, eth0: [enabled=%d, connected=%d], wlan0: [enabled=%d, connected=%d]", 
                        interface.c_str(), m_ethEnabled.load(), m_ethConnected.load(), m_wlanEnabled.load(), m_wlanConnected.load());

            m_defaultInterface = interface;
            return Core::ERROR_NONE;
        }

        /* @brief Get the Public IP used for external world communication */
        uint32_t NetworkManagerImplementation::GetPublicIP (string& interface /* @inout */, string &ipversion /* @inout */,  string& ipaddress /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            stun::bind_result result;
            bool isIPv6 = (ipversion == "IPv6");

            // Either Interface must be connected to get the public IP
            if (!(m_ethConnected.load() || m_wlanConnected.load()))
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
                NMLOG_ERROR("stun bind failed for endpoint %s:%d", m_stunEndpoint.c_str(), m_stunPort);
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
            if (endpoint.empty() || (ipversion != "IPv4" && ipversion != "IPv6"))
            {
                NMLOG_WARNING("Invalid arguments: endpoint=%s, ipversion=%s", endpoint.c_str(), ipversion.c_str());
                return Core::ERROR_BAD_REQUEST;
            }
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
            if (endpoint.empty() || (ipversion != "IPv4" && ipversion != "IPv6"))
            {
                NMLOG_WARNING("Invalid arguments: endpoint=%s, ipversion=%s", endpoint.c_str(), ipversion.c_str());
                return Core::ERROR_BAD_REQUEST;
            }
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
        uint32_t NetworkManagerImplementation::GetSupportedSecurityModes(ISecurityModeIterator*& security /* @out */) const
        {
            LOG_ENTRY_FUNCTION();
            std::vector<WIFISecurityModeInfo> modeInfo {
                                                            {WIFI_SECURITY_NONE,     "NONE"},
                                                            {WIFI_SECURITY_WPA_PSK,  "WPA_PSK"},
                                                            {WIFI_SECURITY_SAE,      "SAE"},
                                                            {WIFI_SECURITY_EAP,      "EAP"},
                                                        };

            using Implementation = RPC::IteratorType<Exchange::INetworkManager::ISecurityModeIterator>;
            security = Core::Service<Implementation>::Create<Exchange::INetworkManager::ISecurityModeIterator>(modeInfo);

            return Core::ERROR_NONE;
        }

        void NetworkManagerImplementation::ReportInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface)
        {
            LOG_ENTRY_FUNCTION();
            if(Exchange::INetworkManager::INTERFACE_LINK_DOWN == state || Exchange::INetworkManager::INTERFACE_REMOVED == state)
            {
                if(interface == "eth0")
                {
                    m_ethIPv4Address = {};
                    m_ethIPv6Address = {};
                    m_ethConnected.store(false);
                    m_defaultInterface = "wlan0"; // If WiFi is connected, make it the default interface
                    // As default interface is changed to wlan0, switch connectivity monitor to initial check
                    connectivityMonitor.switchToInitialCheck();
                }
                else if(interface == "wlan0")
                {
                    m_wlanIPv4Address = {};
                    m_wlanIPv6Address = {};
                    m_wlanConnected.store(false);
                    if(m_ethConnected.load())
                        m_defaultInterface = "eth0"; // If Ethernet is connected, make it the default interface

                    if(m_defaultInterface == interface)
                    {
                        // When WiFi is disconnected while Ethernet is connected, we don't need to trigger connectivity monitor.
                        // For WiFi-only state and WiFi disconnected, we should trigger connectivity monitor.
                        connectivityMonitor.switchToInitialCheck();
                    }
                }
            }

            /* Only the Ethernet connection status is changing here. The WiFi status is updated in the WiFi state callback. */
            if(Exchange::INetworkManager::INTERFACE_LINK_UP == state)
            {
                if(interface == "eth0")
                {
                    m_ethConnected.store(true);
                    m_ethEnabled.store(true);
                }
                else if(interface == "wlan0")
                {
                    m_wlanConnected.store(true);
                    m_wlanEnabled.store(true);
                }
                // connectivityMonitor.switchToInitialCheck();
                // FIXME : Availability of interface does not mean that it has internet connection, so not triggering connectivity monitor check here.
            }

            if(Exchange::INetworkManager::INTERFACE_ADDED == state)
            {
                if(interface == "eth0")
                    m_ethEnabled.store(true);
                else if(interface == "wlan0")
                    m_wlanEnabled.store(true);
            }

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
            {
                m_ethConnected.store(true);
                m_ethEnabled.store(true);
            }
            else if (currentActiveinterface == "wlan0")
            {
                m_wlanConnected.store(true);
                m_wlanEnabled.store(true);
            }

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
                {
                    m_ethConnected.store(true);
                    m_ethEnabled.store(true);
                }
                else if(interface == "wlan0")
                {
                    m_wlanConnected.store(true);
                    m_wlanEnabled.store(true);
                }

                // FIXME : Availability of ip address for a given interface does not mean that its the default interface. This hardcoding will work for RDKProxy but not for Gnome.
                if (m_ethConnected.load() && m_wlanConnected.load())
                    m_defaultInterface = "eth0";
                else
                    m_defaultInterface = interface;

                if(m_defaultInterface == interface) {
                    // As default interface is connected, switch connectivity monitor to initial check any way
                    connectivityMonitor.switchToInitialCheck();
                }
                else
                    NMLOG_DEBUG("No need to trigger connectivity monitor interface is %s", interface.c_str());
            }

            _notificationLock.Lock();
            NMLOG_INFO("Posting onIPAddressChange %s - %s", ipaddress.c_str(), interface.c_str());
            for (const auto callback : _notificationCallbacks) {
                callback->onIPAddressChange(interface, ipversion, ipaddress, status);
            }
            _notificationLock.Unlock();
        }

        void NetworkManagerImplementation::ReportInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState, const string interface)
        {
            _notificationLock.Lock();
            NMLOG_INFO("Posting onInternetStatusChange with current state as %u", (unsigned)currState);
            for (const auto callback : _notificationCallbacks) {
                callback->onInternetStatusChange(prevState, currState, interface);
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
            if (m_isRunning.load()) {
                NMLOG_INFO("WiFiSignalQualityMonitor Thread is already running.");
                return;
            }
            try {
                m_isRunning.store(true);
                m_monitorThread = std::thread(&NetworkManagerImplementation::monitorThreadFunction, this, interval);
                NMLOG_INFO("monitorThreadFunction thread creation successful");
            } catch (const std::exception& err) {
                NMLOG_INFO("monitorThreadFunction thread creation failed: %s", err.what());
            }
        }

        void NetworkManagerImplementation::stopWiFiSignalQualityMonitor()
        {
            if (!m_isRunning.load())
                return; // No thread to stop

            {
                std::lock_guard<std::mutex> lock(m_condVariableMutex);
                m_stopThread.store(true);
            }
            m_condVariable.notify_one();

            if (m_monitorThread.joinable()) {
                m_monitorThread.join();
            }
            m_isRunning.store(false);
        }

        /* Updated implementation using wpa_cli signal_poll for real-time signal data */
        uint32_t NetworkManagerImplementation::GetWiFiSignalQuality(string& ssid /* @out */, string& strength /* @out */, string& noise /* @out */, string& snr /* @out */, WiFiSignalQuality& quality /* @out */)
        {
            std::string key{}, value{}, bssid{};
            char buff[512] = {'\0'};
            FILE *fp = NULL;

            /* Get BSSID and SSID from wpa_cli status */
            fp = popen(SSID_COMMAND, "r");
            if (!fp)
            {
                NMLOG_ERROR("Failed in getting output from command %s", SSID_COMMAND);
                return Core::ERROR_GENERAL;
            }

            while ((!feof(fp)) && (fgets(buff, sizeof (buff), fp) != NULL))
            {
                std::istringstream mystream(buff);
                if(std::getline(std::getline(mystream, key, '=') >> std::ws, value))
                {
                    if (key == "ssid") {
                        ssid = value;
                    }
                    else if (key == "bssid") {
                        bssid = value;
                    }
                }
            }
            pclose(fp);

            /* If BSSID is empty, WiFi is disconnected */
            if (bssid.empty()) {
                NMLOG_WARNING("WiFi is disconnected (BSSID is empty)");
                quality = WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED;
                ssid = "";
                strength = "0";
                noise = "0";
                snr = "0";
                return Core::ERROR_NONE;
            }

            /*Get real-time signal data from wpa_cli signal_poll */
            fp = popen(SIGNAL_POLL_COMMAND, "r");
            if (!fp)
            {
                NMLOG_ERROR("Failed in getting output from command %s", SIGNAL_POLL_COMMAND);
                return Core::ERROR_GENERAL;
            }

            while ((!feof(fp)) && (fgets(buff, sizeof (buff), fp) != NULL))
            {
                std::istringstream mystream(buff);
                if(std::getline(std::getline(mystream, key, '=') >> std::ws, value))
                {
                    if (key == "RSSI") {
                        strength = value;
                    }
                    else if (key == "NOISE") {
                        noise = value;
                    }
                    else if (key == "AVG_RSSI") { // if RSSI is not available
                        if (strength.empty())
                            strength = value;
                    }
                }
            }
            pclose(fp);

            /* Set defaults if empty */
            if (strength.empty())
                strength = "0";
            if (noise.empty())
                noise = "0";

            int16_t readRssi = std::stoi(strength);
            int16_t readNoise = std::stoi(noise);

            /* Validate RSSI is in valid range (-10 to -100 dBm) */
            if (readRssi >= 0 || readRssi < -100) {
                NMLOG_WARNING("RSSI (%d dBm) is out of valid range (-10 to -100 dBm)", readRssi);
                quality = WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED;
                strength = "0";
                noise = "0";
                snr = "0";
                return Core::ERROR_NONE;
            }

            /* Check the Noise is within range between 0 and -96 dbm*/
            if((readNoise >= 0) || (readNoise < DEFAULT_NOISE))
            {
                NMLOG_DEBUG("Received Noise (%d) from wifi driver is not valid; so clamping it", readNoise);
                if (readNoise >= 0) {
                    noise = std::to_string(0);
                }
                else if (readNoise < DEFAULT_NOISE) {
                    noise = std::to_string(DEFAULT_NOISE);
                }
            }

            /*Calculate SNR = RSSI - Noise */
            int16_t calculatedSnr = readRssi - readNoise;
            snr = std::to_string(calculatedSnr);

            /* mapping rssi value when the SNR value is not proper */
            if(!(calculatedSnr > 0 && calculatedSnr <= MAX_SNR_VALUE))
            {
                NMLOG_WARNING("Received SNR (%d) from wifi driver is not valid; Lets map with RSSI (%s)", calculatedSnr, strength.c_str());
                calculatedSnr = std::stoi(strength);
                /* Take the absolute value */
                calculatedSnr = (calculatedSnr < 0) ? -calculatedSnr : calculatedSnr;

                snr = std::to_string(calculatedSnr);
            }

            NMLOG_INFO("SSID: %s, RSSI: %s dBm, Noise: %s dBm, SNR: %s dBm", ssid.c_str(), strength.c_str(), noise.c_str(), snr.c_str());

            /* Calculate quality based on RSSI (signal strength) */
            if (readRssi == 0) {
                quality = WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED;
            }
            else if (readRssi >= -50) {
                quality = WiFiSignalQuality::WIFI_SIGNAL_EXCELLENT;
            }
            else if (readRssi >= -60) {
                quality = WiFiSignalQuality::WIFI_SIGNAL_GOOD;
            }
            else if (readRssi >= -67) {
                quality = WiFiSignalQuality::WIFI_SIGNAL_FAIR;
            }
            else {
                quality = WiFiSignalQuality::WIFI_SIGNAL_WEAK;
            }

            return Core::ERROR_NONE;
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
                if (m_processMonCondVar.wait_for(lock, std::chrono::seconds(interval), [this](){ return m_processMonThreadStop.load(); }))
                {
                    NMLOG_INFO("Received stop signal");
                }
            } while (!m_processMonThreadStop.load());

            return;
        }

        void NetworkManagerImplementation::monitorThreadFunction(int interval)
        {
            static Exchange::INetworkManager::WiFiSignalQuality oldSignalQuality = Exchange::INetworkManager::WIFI_SIGNAL_DISCONNECTED;
            NMLOG_INFO("WiFiSignalQualityMonitor thread started ! (%d)", interval);
            while (true)
            {
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
                    break; // Exit the loop
                }

                std::unique_lock<std::mutex> lock(m_condVariableMutex);
                // Wait for the specified interval or until notified to stop
                if (m_condVariable.wait_for(lock, std::chrono::seconds(interval), [this](){ return m_stopThread.load(); }))
                {
                    NMLOG_INFO("WiFiSignalQualityMonitor received stop signal or timed out");
                    break;
                }
            }
            m_stopThread.store(false);
        }

        void NetworkManagerImplementation::ReportWiFiStateChange(const Exchange::INetworkManager::WiFiState state)
        {
            /* start signal strength monitor when wifi connected */
            if(INetworkManager::WiFiState::WIFI_STATE_CONNECTED == state)
            {
                m_wlanConnected.store(true);
                startWiFiSignalQualityMonitor(DEFAULT_WIFI_SIGNAL_TEST_INTERVAL_SEC);
            }
            else
            {
                stopWiFiSignalQualityMonitor();
                m_wlanConnected.store(false); /* Any other state is considered as WiFi not connected. */
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

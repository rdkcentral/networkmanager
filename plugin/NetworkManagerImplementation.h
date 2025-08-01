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

#pragma once


#include "Module.h"
#include <iostream>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <atomic>

using namespace std;

#include "INetworkManager.h"
#include <interfaces/IConfiguration.h>
#include "NetworkManagerLogger.h"
#include "NetworkManagerConnectivity.h"
#include "NetworkManagerStunClient.h"

#define DEFAULT_NOISE                              -180
#define MAX_SNR_VALUE                              180

#define DEFAULT_WIFI_SIGNAL_TEST_INTERVAL_SEC      60
#define NM_PROCESS_MONITOR_INTERVAL_SEC            60
#define NM_WIFI_SNR_THRESHOLD_EXCELLENT            40
#define NM_WIFI_SNR_THRESHOLD_GOOD                 25
#define NM_WIFI_SNR_THRESHOLD_FAIR                 18
#define ROUTE_METRIC_PRIORITY_HIGH                 1
#define ROUTE_METRIC_PRIORITY_LOW                  100

namespace WPEFramework
{
    namespace Plugin
    {
        class NetworkManagerImplementation : public Exchange::INetworkManager, public Exchange::IConfiguration
        {
            enum NetworkEvents
            {
                NETMGR_PING,
                NETMGR_TRACE,
            };

            class ConnectivityConf : public Core::JSON::Container {
                public:
                    ConnectivityConf& operator=(const ConnectivityConf&) = delete;

                    ConnectivityConf()
                        : Core::JSON::Container()
                        , endpoint_1(_T("http://clients3.google.com/generate_204"))
                        , endpoint_2(_T(""))
                        , endpoint_3(_T(""))
                        , endpoint_4(_T(""))
                        , endpoint_5(_T(""))
                        , ConnectivityCheckInterval(6)
                    {
                        Add(_T("endpoint_1"), &endpoint_1);
                        Add(_T("endpoint_2"), &endpoint_2);
                        Add(_T("endpoint_3"), &endpoint_3);
                        Add(_T("endpoint_4"), &endpoint_4);
                        Add(_T("endpoint_5"), &endpoint_5);
                        Add(_T("interval"), &ConnectivityCheckInterval);
                    }
                    ~ConnectivityConf() override = default;

                public:
                    /* connectivity configuration */
                    Core::JSON::String endpoint_1;
                    Core::JSON::String endpoint_2;
                    Core::JSON::String endpoint_3;
                    Core::JSON::String endpoint_4;
                    Core::JSON::String endpoint_5;
                    Core::JSON::DecUInt32 ConnectivityCheckInterval;
            };

            class Stun : public Core::JSON::Container {
                    public:
                        Stun& operator=(const Stun&) = delete;

                        Stun()
                            : Core::JSON::Container()
                            , stunEndpoint(_T("stun.l.google.com"))
                            , port(19302)
                            , interval(30)
                        {
                            Add(_T("endpoint"), &stunEndpoint);
                            Add(_T("port"), &port);
                            Add(_T("interval"), &interval);
                        }
                        ~Stun() override = default;

                    public:
                        /* stun configuration */
                        Core::JSON::String stunEndpoint;
                        Core::JSON::DecUInt32 port;
                        Core::JSON::DecUInt32 interval;
            };

            class WiFiConfig : public Core::JSON::Container
            {
                public:
                    Core::JSON::String ssid;
                    Core::JSON::String password;
                    Core::JSON::DecUInt8 security;

                    WiFiConfig() : Core::JSON::Container()
                    {
                        Add(_T("ssid"), &ssid);
                        Add(_T("password"), &password);
                        Add(_T("security"), &security);
                    }

                    ~WiFiConfig() override = default;
                    WiFiConfig& operator=(const WiFiConfig&) = delete;
            };

            class Configuration : public Core::JSON::Container {
            private:
                Configuration(const Configuration&);
                Configuration& operator=(const Configuration&);

            public:
                Configuration()
                    : Core::JSON::Container()
                    {
                        Add(_T("connectivity"), &connectivityConf);
                        Add(_T("stun"), &stun);
                        Add(_T("loglevel"), &loglevel);
                    }
                ~Configuration() override = default;

            public:
                ConnectivityConf connectivityConf;
                Stun stun;
                Core::JSON::DecUInt32 loglevel;
            };


            class Job : public Core::IDispatch {
            public:
                Job(function<void()> work)
                : _work(work)
                {
                }
                void Dispatch() override
                {
                    _work();
                }

            private:
                function<void()> _work;
            };

            public:
                NetworkManagerImplementation();
                ~NetworkManagerImplementation() override;

                // Do not allow copy/move constructors
                NetworkManagerImplementation(const NetworkManagerImplementation &) = delete;
                NetworkManagerImplementation &operator=(const NetworkManagerImplementation &) = delete;

                BEGIN_INTERFACE_MAP(NetworkManagerImplementation)
                INTERFACE_ENTRY(Exchange::INetworkManager)
                INTERFACE_ENTRY(Exchange::IConfiguration)
                END_INTERFACE_MAP

                // Handle Notification registration/removal
                uint32_t Register(INetworkManager::INotification *notification) override;
                uint32_t Unregister(INetworkManager::INotification *notification) override;

            public:
                // Below Control APIs will work with RDK or GNome NW.
                /* @brief Get all the Available Interfaces */
                uint32_t GetAvailableInterfaces (IInterfaceDetailsIterator*& interfaces/* @out */) override;

                /* @brief Get the active Interface used for external world communication */
                uint32_t GetPrimaryInterface (string& interface /* @out */) override;

                /* @brief Enable/Disable the given interface */
                uint32_t SetInterfaceState(const string& interface/* @in */, const bool isEnabled/* @in */) override;
                /* @brief Get the state of given interface */
                uint32_t GetInterfaceState(const string& interface/* @in */, bool& isEnabled/* @out */) override;

                /* @brief Get IP Address Of the Interface */
                uint32_t GetIPSettings(string& interface /* @inout */, const string& ipversion /* @in */, IPAddress& address /* @out */) override;
                /* @brief Set IP Address Of the Interface */
                uint32_t SetIPSettings(const string& interface /* @in */, const IPAddress& address /* @in */) override;

                // WiFi Specific Methods
                /* @brief Initiate a WIFI Scan; This is Async method and returns the scan results as Event */
                uint32_t StartWiFiScan(const string& frequency /* @in */, IStringIterator* const ssids/* @in */) override;
                uint32_t StopWiFiScan(void) override;

                uint32_t GetKnownSSIDs(IStringIterator*& ssids /* @out */) override;
                uint32_t AddToKnownSSIDs(const WiFiConnectTo& ssid /* @in */) override;
                uint32_t RemoveKnownSSID(const string& ssid /* @in */) override;

                uint32_t WiFiConnect(const WiFiConnectTo& ssid /* @in */) override;
                uint32_t WiFiDisconnect(void) override;
                uint32_t GetConnectedSSID(WiFiSSIDInfo&  ssidInfo /* @out */) override;

                uint32_t StartWPS(const WiFiWPS& method /* @in */, const string& wps_pin /* @in */) override;
                uint32_t StopWPS(void) override;
                uint32_t GetWifiState(WiFiState &state) override;
                uint32_t GetWiFiSignalQuality(string& ssid /* @out */, string& strength /* @out */, string& noise /* @out */, string& snr /* @out */, WiFiSignalQuality& quality /* @out */) override;

                uint32_t SetStunEndpoint (string const endpoint /* @in */, const uint32_t port /* @in */, const uint32_t bindTimeout /* @in */, const uint32_t cacheTimeout /* @in */) override;
                uint32_t GetStunEndpoint (string &endpoint /* @out */, uint32_t& port /* @out */, uint32_t& bindTimeout /* @out */, uint32_t& cacheTimeout /* @out */) const override;

                /* @brief Get ConnectivityTest Endpoints */
                uint32_t GetConnectivityTestEndpoints(IStringIterator*& endpoints/* @out */) const override;
                /* @brief Set ConnectivityTest Endpoints */
                uint32_t SetConnectivityTestEndpoints(IStringIterator* const endpoints /* @in */) override;

                /* @brief Get Internet Connectivty Status */ 
                uint32_t IsConnectedToInternet(string &ipversion /* @inout */, string &interface /* @inout */, InternetStatus &result /* @out */) override;
                /* @brief Get Authentication URL if the device is behind Captive Portal */
                uint32_t GetCaptivePortalURI(string &endpoints/* @out */) const override;

                /* @brief Get the Public IP used for external world communication */
                uint32_t GetPublicIP (string& interface /* @inout */, string &ipversion /* @inout */,  string& ipaddress /* @out */) override;

                /* @brief Request for ping and get the response in as event. The GUID used in the request will be returned in the event. */
                uint32_t Ping (const string ipversion /* @in */,  const string endpoint /* @in */, const uint32_t noOfRequest /* @in */, const uint16_t timeOutInSeconds /* @in */, const string guid /* @in */, string& response /* @out */) override;

                /* @brief Request for trace get the response in as event. The GUID used in the request will be returned in the event. */
                uint32_t Trace (const string ipversion /* @in */,  const string endpoint /* @in */, const uint32_t noOfRequest /* @in */, const string guid /* @in */, string& response /* @out */) override;

                uint32_t GetSupportedSecurityModes(ISecurityModeIterator*& security /* @out */) const override;

                /* @brief Set the network manager plugin log level */
                uint32_t SetLogLevel(const Logging& level /* @in */) override;
                uint32_t GetLogLevel(Logging& level /* @out */) override;

                /* @brief configure network manager plugin */
                uint32_t Configure(PluginHost::IShell* service) override;

                /* Events */
                void ReportInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface);
                void ReportActiveInterfaceChange(const string prevActiveInterface, const string currentActiveinterface);
                void ReportIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status);
                void ReportInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState);
                void ReportAvailableSSIDs(const JsonArray &arrayofWiFiScanResults);
                void ReportWiFiStateChange(const Exchange::INetworkManager::WiFiState state);
                void ReportWiFiSignalQualityChange(const string ssid, const string strength, const string noise, const string snr, const Exchange::INetworkManager::WiFiSignalQuality quality);

            private:
                void platform_init(void);
                void platform_logging(const NetworkManagerLogger::LogLevel& level);
                void getInitialConnectionState(void);
                void executeExternally(NetworkEvents event, const string commandToExecute, string& response);
                void threadEventRegistration(bool iarmInit, bool iarmConnect);
                void filterScanResults(JsonArray &ssids);
                void startWiFiSignalQualityMonitor(int interval);
                void stopWiFiSignalQualityMonitor();
                void monitorThreadFunction(int interval);
                int32_t logSSIDs(Logging level, const JsonArray &ssids);
                void processMonitor(uint16_t interval);

            private:
                std::list<Exchange::INetworkManager::INotification *> _notificationCallbacks;
                Core::CriticalSection _notificationLock;
                string m_defaultInterface;
                string m_publicIP;
                stun::client stunClient;
                string m_stunEndpoint;
                uint16_t m_stunPort;
                uint16_t m_stunBindTimeout;
                uint16_t m_stunCacheTimeout;
                std::thread m_registrationThread;
                string m_filterfrequency;
                std::vector<std::string> m_filterSsidslist;
                std::thread m_monitorThread;

                std::thread m_processMonThread;
                std::mutex m_processMonMutex;
                std::atomic<bool> m_processMonThreadStop{false};
                std::condition_variable m_processMonCondVar;

                std::atomic<bool> m_isRunning{false};
                std::atomic<bool> m_stopThread{false};
                std::mutex m_condVariableMutex;
                std::condition_variable m_condVariable;

            public:
                std::atomic<bool> m_ethConnected;
                std::atomic<bool> m_wlanConnected;
                std::string m_lastConnectedSSID;
                mutable ConnectivityMonitor connectivityMonitor;
        };
    }
}

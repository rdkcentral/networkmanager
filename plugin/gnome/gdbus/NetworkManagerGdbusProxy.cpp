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

#include "NetworkManagerImplementation.h"
#include "NetworkManagerGdbusClient.h"
#include "NetworkManagerGdbusEvent.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using namespace std;

namespace WPEFramework
{
    namespace Plugin
    {
        NetworkManagerImplementation* _instance = nullptr;
        NetworkManagerClient* _nmGdbusClient = nullptr;
        NetworkManagerEvents* _nmGdbusEvents = nullptr;
        void NetworkManagerInternalEventHandler(const char *owner, int eventId, void *data, size_t len)
        {
            return;
        }

        void NetworkManagerImplementation::getInitialConnectionState()
        {
            // check the connection state and post event
            Exchange::INetworkManager::IInterfaceDetailsIterator* _interfaces{};
            uint32_t rc = GetAvailableInterfaces(_interfaces);

            if (Core::ERROR_NONE == rc)
            {
                if (_interfaces != nullptr)
                {
                    Exchange::INetworkManager::InterfaceDetails iface{};
                    while (_interfaces->Next(iface) == true)
                    {
                        Core::JSON::EnumType<Exchange::INetworkManager::InterfaceType> type{iface.type};
                        if(iface.enabled)
                        {
                            NMLOG_INFO("'%s' interface is enabled", iface.name.c_str());
                            // ReportInterfaceStateChange(Exchange::INetworkManager::INTERFACE_ADDED, iface.name);
                            if(iface.connected)
                            {
                                NMLOG_INFO("'%s' interface is connected", iface.name.c_str());
                                ReportActiveInterfaceChange(iface.name, iface.name);
                                std::string ipversion = {};
                                Exchange::INetworkManager::IPAddress addr;
                                rc = GetIPSettings(iface.name, ipversion, addr);
                                if (Core::ERROR_NONE == rc)
                                {
                                    if(!addr.ipaddress.empty()) {
                                        NMLOG_INFO("'%s' interface have ip '%s'", iface.name.c_str(), addr.ipaddress.c_str());
                                        ReportIPAddressChange(iface.name, addr.ipversion, addr.ipaddress, Exchange::INetworkManager::IP_ACQUIRED);
                                    }
                                }
                            }
                        }
                    }

                    _interfaces->Release();
                }
            }
            else
                NMLOG_INFO("GetAvailableInterfaces Failed");
        }

        void NetworkManagerImplementation::platform_logging(const NetworkManagerLogger::LogLevel& level)
        {
            /* set networkmanager daemon log level based on current plugin log level */
            const char* command = "nmcli general logging level TRACE domains ALL";

            if(NetworkManagerLogger::DEBUG_LEVEL != level)
                return;

            // Execute the command using popen
            FILE *pipe = popen(command, "r");

            if (pipe == NULL) {
                NMLOG_ERROR("popen failed %s ", command);
                return;
            }

            // Close the pipe and retrieve the command's exit status
            int status = pclose(pipe);
            if (status == -1) {
                NMLOG_ERROR("pclose failed");
                return;
            }

            // Extract the exit status from the status code
            int exitCode = WEXITSTATUS(status);
            if (exitCode == 0)
                NMLOG_INFO("NetworkManager daemon log level changed ! logLevel: %d", level);
            else
                NMLOG_INFO(" '%s' failed with exit code %d.", command, exitCode);
        }

        void NetworkManagerImplementation::platform_init()
        {
            ::_instance = this;
            _nmGdbusClient = NetworkManagerClient::getInstance();
            _nmGdbusEvents = NetworkManagerEvents::getInstance();
            getInitialConnectionState();
        }

        uint32_t NetworkManagerImplementation::GetAvailableInterfaces (Exchange::INetworkManager::IInterfaceDetailsIterator*& interfacesItr/* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            std::vector<Exchange::INetworkManager::InterfaceDetails> interfaceList;
            if(_nmGdbusClient->getAvailableInterfaces(interfaceList))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("GetAvailableInterfaces failed");
            using Implementation = RPC::IteratorType<Exchange::INetworkManager::IInterfaceDetailsIterator>;
            interfacesItr = Core::Service<Implementation>::Create<Exchange::INetworkManager::IInterfaceDetailsIterator>(interfaceList);

            return rc;
        }

        uint32_t NetworkManagerImplementation::GetPrimaryInterface (string& interface /* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->getPrimaryInterface(interface))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("GetPrimaryInterface failed");
            return rc;
        }

        uint32_t NetworkManagerImplementation::SetPrimaryInterface (const string& interface/* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->setPrimaryInterface(interface))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("SetPrimaryInterface failed");
            return rc;
        }
        uint32_t NetworkManagerImplementation::SetInterfaceState(const string& interface/* @in */, const bool enabled /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->setInterfaceState(interface, enabled))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("SetInterfaceState failed");
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetInterfaceState(const string& interface/* @in */, bool& isEnabled /* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->getInterfaceState(interface, isEnabled))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("GetInterfaceState failed");
            return rc;
        }

       /* @brief Get IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::GetIPSettings(string& interface /* @inout */, const string &ipversion /* @in */, IPAddress& result /* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->getIPSettings(interface, ipversion, result))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("GetIPSettings failed");
            return rc;
        }

        /* @brief Set IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::SetIPSettings(const string& interface /* @in */, const IPAddress& address /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->setIPSettings(interface, address))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("SetIPSettings failed");
            return rc;
        }

        uint32_t NetworkManagerImplementation::StartWiFiScan(const string& frequency /* @in */, IStringIterator* const ssids/* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            _nmGdbusEvents->setwifiScanOptions(true); /* Enable event posting */
            if(_nmGdbusClient->startWifiScan())
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("StartWiFiScan failed");
            return rc;
        }

        uint32_t NetworkManagerImplementation::StopWiFiScan(void)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            _nmGdbusEvents->setwifiScanOptions(false); /* disable event posting */
            rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetKnownSSIDs(IStringIterator*& ssids /* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            std::list<string> ssidList;
            if(_nmGdbusClient->getKnownSSIDs(ssidList) && !ssidList.empty())
            {
                ssids = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(ssidList);
                rc = Core::ERROR_NONE;
            }
            else
                NMLOG_ERROR("GetKnownSSIDs failed");

            return rc;
        }

        uint32_t NetworkManagerImplementation::AddToKnownSSIDs(const WiFiConnectTo& ssid /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->addToKnownSSIDs(ssid))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("addToKnownSSIDs failed");
            return rc;
        }

        uint32_t NetworkManagerImplementation::RemoveKnownSSID(const string& ssid /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->removeKnownSSIDs(ssid))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("RemoveKnownSSID failed");
            return rc;
        }

        uint32_t NetworkManagerImplementation::WiFiConnect(const WiFiConnectTo& ssid /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(ssid.ssid.empty())
            {
                NMLOG_WARNING("ssid is not sepecified; so attampting to connect know ssids !");
                _nmGdbusEvents->setwifiScanOptions(false); /* Enable event posting */
                if(_nmGdbusClient->startWifiScan())
                    rc = Core::ERROR_NONE;
                else
                    NMLOG_ERROR("StartWiFiScan failed");
            }
            else
            {
                if(_nmGdbusClient->wifiConnect(ssid))
                    rc = Core::ERROR_NONE;
                else
                    NMLOG_ERROR("WiFiConnect failed");
            }

            return rc;
        }

        uint32_t NetworkManagerImplementation::WiFiDisconnect(void)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->wifiDisconnect())
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("WiFiDisconnect failed");
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetConnectedSSID(WiFiSSIDInfo&  ssidInfo /* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->getConnectedSSID(ssidInfo))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("getConnectedSSID failed");
            return rc;
        }

#if 0
        uint32_t NetworkManagerImplementation::GetWiFiSignalQuality(string& ssid /* @out */, string& strength /* @out */, WiFiSignalQuality& quality /* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->getWiFiSignalQuality(ssid, strength, quality))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("GetWiFiSignalQuality failed");
            return rc;
        }
#endif

        uint32_t NetworkManagerImplementation::GetWifiState(WiFiState &state)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->getWifiState(state))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("GetWifiState failed");
            return rc;
        }

        uint32_t NetworkManagerImplementation::StartWPS(const WiFiWPS& method /* @in */, const string& wps_pin /* @in */)
        {
            uint32_t rc = Core::ERROR_NONE;
            if(method == WIFI_WPS_SERIALIZED_PIN || method == WIFI_WPS_PIN)
            {
                NMLOG_ERROR("WPS PIN method is not supported as of now");
                return Core::ERROR_RPC_CALL_FAILED;
            }
            _nmGdbusEvents->setwifiScanOptions(false);
            if(!_nmGdbusClient->startWifiScan())
            {
                NMLOG_WARNING("scanning reuest failed; trying to connect wps");
            }

            if(_nmGdbusClient->startWPS())
                NMLOG_INFO ("start WPS success");
            else {
                rc = Core::ERROR_GENERAL;
                NMLOG_ERROR("start WPS failed");
            }

            return rc;
        }

        uint32_t NetworkManagerImplementation::StopWPS(void)
        {
            uint32_t rc = Core::ERROR_NONE;
            if(_nmGdbusClient->stopWPS())
                NMLOG_INFO ("stop success");
            else {
                NMLOG_ERROR("stop WPS failed");
                rc = Core::ERROR_GENERAL;
            }
            return rc;
        }
    }
}

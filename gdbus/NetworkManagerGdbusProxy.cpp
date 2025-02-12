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

        void NetworkManagerImplementation::platform_init()
        {
            ::_instance = this;
            _nmGdbusClient = NetworkManagerClient::getInstance();
            _nmGdbusEvents = NetworkManagerEvents::getInstance();
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
            if(_nmGdbusClient->wifiConnect(ssid))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("WiFiConnect failed");
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

        uint32_t NetworkManagerImplementation::GetWiFiSignalStrength(string& ssid /* @out */, string& strength /* @out */, WiFiSignalQuality& quality /* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(_nmGdbusClient->getWiFiSignalStrength(ssid, strength, quality))
                rc = Core::ERROR_NONE;
            else
                NMLOG_ERROR("GetWiFiSignalStrength failed");
            return rc;
        }

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
            return Core::ERROR_NONE;
        }

        uint32_t NetworkManagerImplementation::StopWPS(void)
        {
            return Core::ERROR_NONE;
        }
    }
}

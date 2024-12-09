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
#include <gio/gio.h>
#include <iostream>
#include <string>
#include <list>

#include "NetworkManagerLogger.h"
#include "NetworkManagerGdbusMgr.h"
#include "INetworkManager.h"

namespace WPEFramework
{
    namespace Plugin
    {

        class NetworkManagerClient
        {
            public:
                static NetworkManagerClient* getInstance()
                {
                    static NetworkManagerClient instance;
                    return &instance;
                }

                NetworkManagerClient(const NetworkManagerClient&) = delete;
                NetworkManagerClient& operator=(const NetworkManagerClient&) = delete;

                bool getKnownSSIDs(std::list<std::string>& ssids);
                bool getAvailableSSIDs(std::list<std::string>& ssids);
                bool getConnectedSSID(Exchange::INetworkManager::WiFiSSIDInfo& ssidinfo);
                bool addToKnownSSIDs(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo);
                bool removeKnownSSIDs(const std::string& ssid);
                bool startWifiScan(const std::string ssid = "");
                bool wifiConnect(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo);
                bool wifiDisconnect();
                bool getWifiState(Exchange::INetworkManager::WiFiState &state);
                bool getWiFiSignalStrength(std::string& ssid, std::string& signalStrength, Exchange::INetworkManager::WiFiSignalQuality& quality);
                bool startWPS();
                bool stopWPS();

            private:
                NetworkManagerClient();
                ~NetworkManagerClient();

                DbusMgr m_dbus;
        };
    } // Plugin
} // WPEFramework
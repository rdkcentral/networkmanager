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

#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include "NetworkManagerLogger.h"
#include "INetworkManager.h"
#include "NetworkManagerSecretAgent.h"
#include <iostream>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <atomic>

#define WPS_RETRY_WAIT_IN_MS        10 // 10 sec
#define WPS_RETRY_COUNT             10
#define WPS_PROCESS_WAIT_IN_MS      5

namespace WPEFramework
{
    namespace Plugin
    {
        class wifiManager
        {
        public:
            static wifiManager* getInstance()
            {
                static wifiManager instance;
                return &instance;
            }

            bool getWifiState(Exchange::INetworkManager::WiFiState& state);
            bool wifiDisconnect();
            bool activateKnownConnection(std::string iface, std::string knowConnectionID="");
            bool wifiConnectedSSIDInfo(Exchange::INetworkManager::WiFiSSIDInfo &ssidinfo);
            bool wifiConnect(Exchange::INetworkManager::WiFiConnectTo ssidInfo);
            bool wifiScanRequest(std::string ssidReq = "");
            bool isWifiScannedRecently(int timelimitInSec = 5); // default 5 sec as shotest scanning interval
            bool getKnownSSIDs(std::list<string>& ssids);
            bool addToKnownSSIDs(const Exchange::INetworkManager::WiFiConnectTo ssidinfo);
            bool removeKnownSSID(const string& ssid);
            bool quit(NMDevice *wifiNMDevice);
            bool wait(GMainLoop *loop, int timeOutMs = 10000); // default maximium set as 10 sec
            bool startWPS();
            bool stopWPS();
            bool setInterfaceState(std::string interface, bool enabled);
            bool setIpSettings(const string interface, const Exchange::INetworkManager::IPAddress address);
            bool setPrimaryInterface(const string interface);
        private:
            NMDevice *getWifiDevice();

        private:
            wifiManager();
            ~wifiManager() {
                NMLOG_INFO("~wifiManager");
                if (m_nmContext) {
                    g_main_context_pop_thread_default(m_nmContext);
                    g_main_context_unref(m_nmContext);
                    m_nmContext = NULL;
                }
                if(m_client != NULL) {
                    deleteClientConnection();
                }
                if(m_loop != NULL) {
                    g_main_loop_unref(m_loop);
                    m_loop = NULL;
                }
            }

            void wpsProcess();
            wifiManager(wifiManager const&) = delete;
            void operator=(wifiManager const&) = delete;

            bool createClientNewConnection();
            void deleteClientConnection();

        public:
            NMClient *m_client;
            GMainLoop *m_loop;
            gboolean m_createNewConnection;
            GMainContext *m_nmContext = nullptr;
            const char* m_objectPath;
            NMDevice *m_wifidevice;
            GSource *m_source;
            bool m_isSuccess = false;
            SecretAgent m_secretAgent;
        };
    }   // Plugin
}   // WPEFramework

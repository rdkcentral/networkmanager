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

#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include "NetworkManagerLogger.h"
#include "INetworkManager.h"
#include <iostream>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <atomic>

#define WPA_SUPPLICANT_CONF "/opt/secure/wifi/wpa_supplicant.conf"
#define WPA_CLI_STATUS "wpa_cli status"

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

            bool isWifiConnected();
            bool wifiDisconnect();
            bool wifiConnectedSSIDInfo(Exchange::INetworkManager::WiFiSSIDInfo &ssidinfo);
            bool wifiConnect(Exchange::INetworkManager::WiFiConnectTo wifiData);
            bool wifiScanRequest(std::string ssidReq = "");
            bool isWifiScannedRecently(int timelimitInSec = 5); // default 10 sec as shotest scanning interval
            bool getKnownSSIDs(std::list<string>& ssids);
            bool addToKnownSSIDs(const Exchange::INetworkManager::WiFiConnectTo ssidinfo);
            bool removeKnownSSID(const string& ssid);
            bool quit(NMDevice *wifiNMDevice);
            bool wait(GMainLoop *loop, int timeOutMs = 10000); // default maximium set as 10 sec
            bool initiateWPS();
            bool cancelWPS();
            void wpsAction();
            bool setInterfaceState(std::string interface, bool enabled);
            bool setIpSettings(const string interface, const Exchange::INetworkManager::IPAddress address);
        private:
            NMDevice *getWifiDevice();

        private:
            wifiManager();
            ~wifiManager() {
                NMLOG_INFO("~wifiManager");
                g_main_context_pop_thread_default(nmContext);
                if(client != NULL) {
                    g_object_unref(client);
                    client = NULL;
                }
                if(loop != NULL) {
                    g_main_loop_unref(loop);
                    loop = NULL;
                }
            }

            wifiManager(wifiManager const&) = delete;
            void operator=(wifiManager const&) = delete;

            bool createClientNewConnection();
            std::atomic<bool> wpsStop = {false};
            std::thread wpsThread;

        public:
            NMClient *client;
            GMainLoop *loop;
            gboolean createNewConnection;
            GMainContext *nmContext = nullptr;
            GMainContext *wpsContext = nullptr;
            const char* objectPath;
            NMDevice *wifidevice;
            GSource *source;
            guint wifiDeviceStateGsignal = 0;
            bool isSuccess = false;
        };
    }   // Plugin
}   // WPEFramework

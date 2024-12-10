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
#include <string.h>
#include <iostream>
#include <atomic>

#include "NetworkManagerGdbusMgr.h"
#include "INetworkManager.h"

namespace WPEFramework
{
    namespace Plugin
    {

    typedef struct {;

        GDBusProxy *wiredDeviceProxy; // eth0 dev
        GDBusProxy *wirelessDeviceProxy; // wlan0 dev
        GDBusProxy *wirelessProxy; // wireless interface
        GDBusProxy *networkManagerProxy; // networkmanager main bus
        GDBusProxy *settingsProxy; // settings 

        std::string wifiDevicePath;
        std::string ethDevicePath;

        GMainLoop *loop;
    } NMEvents;

    class NetworkManagerEvents
    {

    public:
        static void onInterfaceStateChangeCb(Exchange::INetworkManager::InterfaceState newState, std::string iface); // ReportInterfaceStateChangedEvent
        static void onAddressChangeCb(std::string iface, bool acqired, bool isIPv6, std::string ipAddress = ""); // ReportIPAddressChangedEvent
        static void onActiveInterfaceChangeCb(std::string newInterface); // ReportActiveInterfaceChangedEvent
        static void onAvailableSSIDsCb(const char* wifiDevicePath); // ReportAvailableSSIDsEvent
        static void onWIFIStateChanged(Exchange::INetworkManager::WiFiState state, std::string& wifiStateStr); // ReportWiFiStateChangedEvent

    public:
        static NetworkManagerEvents* getInstance();
        bool startNetworkMangerEventMonitor();
        void stopNetworkMangerEventMonitor();
        void setwifiScanOptions(bool doNotify);

    private:
        static void* networkMangerEventMonitor(void *arg);
        NetworkManagerEvents();
        ~NetworkManagerEvents();
        std::atomic<bool>isEventThrdActive = {false};
        std::atomic<bool>doScanNotify = {true};
        GThread *eventThrdID;
    public:
        NMEvents nmEvents;
        DbusMgr eventDbus;
        char wlanIfname[16];
        char ethIfname[16];
    };

    }   // Plugin
}   // WPEFramework

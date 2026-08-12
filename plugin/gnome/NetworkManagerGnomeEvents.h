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
#include <string.h>
#include <iostream>
#include <atomic>

namespace WPEFramework
{
    namespace Plugin
    {

    typedef struct {
        NMClient *client;
        GMainLoop *loop;
        NMDevice *device;
        NMDeviceWifi *wifiDevice;
        NMActiveConnection *activeConn;
        std::string ifnameWlan0;
        std::string ifnameEth0;
    } NMEvents;

    class GnomeNetworkManagerEvents
    {

    public:
        /* Gnome-owned cache of live NM device state, kept out of the
           backend-agnostic NetworkManagerImplementation (holds a libnm type). */
        struct InterfaceStateInfo {
            NMDeviceState state   = NM_DEVICE_STATE_UNKNOWN;
            std::string   mac;
            bool          present = false;
        };

        static void onInterfaceStateChangeCb(uint8_t newState, std::string iface); // ReportInterfaceStateChange
        static void onActiveInterfaceChangeCb(std::string newInterface); // ReportActiveInterfaceChange
        static void onAvailableSSIDsCb(NMDeviceWifi *wifiDevice, GParamSpec *pspec, gpointer userData); // ReportAvailableSSIDs
        static void onWIFIStateChanged(uint8_t state); // ReportWiFiStateChange
        static void deviceStateChangeCb(NMDevice *device, GParamSpec *pspec, NMEvents *nmEvents);

        /* Interface-state cache: sole writer is the event monitor; readers
           (GetAvailableInterfaces / GetInterfaceState) are pure. */
        static void updateInterfaceStateCache(const std::string& iface, NMDeviceState state, const char* mac);
        static void removeInterfaceStateCache(const std::string& iface);
        static bool getInterfaceStateCache(const std::string& iface, InterfaceStateInfo& out);
        /* Canonical enabled/connected definitions shared by both read APIs. */
        static bool isInterfaceStateEnabled(NMDeviceState state);
        static bool isInterfaceStateConnected(NMDeviceState state);

    public:
        static GnomeNetworkManagerEvents* getInstance();
        bool startNetworkMangerEventMonitor();
        void stopNetworkMangerEventMonitor();
        void setwifiScanOptions(bool doNotify);

    private:
        static void* networkMangerEventMonitor(void *arg);
        static bool apToJsonObject(NMAccessPoint *ap, JsonObject& ssidObj);
        void cleanupSignalHandlers();
        GnomeNetworkManagerEvents();
        ~GnomeNetworkManagerEvents();
        std::atomic<bool>isEventThrdActive = {false};
        std::atomic<bool>doScanNotify = {false};
        NMEvents nmEvents;
        GThread *eventThrdID;
    };

    }   // Plugin
}   // WPEFramework

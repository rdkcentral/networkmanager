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
#include <gio/gio.h>
#include <iostream>
#include <string>
#include <list>

/* include NetworkManager.h for the defines, but we don't link against libnm. */
// #include <NetworkManager.h>
#include <libnm/nm-dbus-interface.h>
#include "INetworkManager.h"
#include "NetworkManagerGdbusMgr.h"

using namespace std;

struct apInfo {
    string ssid;
    string bssid;
    NM80211ApFlags flag;
    NM80211ApSecurityFlags wpaFlag;
    NM80211ApSecurityFlags rsnFlag;
    NM80211Mode mode;
    guint32 frequency;
    guint32 bitrate;
    guint32 strength;
};

struct deviceInfo
{
    std::string interface;
    std::string activeConnPath;
    std::string path;
    NMDeviceState state;
    NMDeviceStateReason stateReason;
    NMDeviceType deviceType;
};

#define G_VARIANT_LOOKUP(dict, key, format, ...)            \
    if (!g_variant_lookup(dict, key, format, __VA_ARGS__)) {\
        NMLOG_ERROR("g_variant Key '%s' not found", key);               \
        return FALSE;                                       \
    }

namespace WPEFramework
{
    namespace Plugin
    {
        class GnomeUtils {
            public:
                static bool getDeviceByIpIface(DbusMgr& m_dbus, const gchar *iface_name, std::string& path);
                static bool getApDetails(DbusMgr& m_dbus, const char* apPath, Exchange::INetworkManager::WiFiSSIDInfo& wifiInfo);
                static bool getConnectionPaths(DbusMgr& m_dbus, std::list<std::string>& pathsList);
                static bool getWifiConnectionPaths(DbusMgr& m_dbus, const char* devicePath, std::list<std::string>& paths);
                static bool getDevicePropertiesByPath(DbusMgr& m_dbus, const char* devPath, deviceInfo& properties);
                static bool getDeviceInfoByIfname(DbusMgr& m_dbus, const char* ifname, deviceInfo& properties);
                static bool getCachedPropertyU(GDBusProxy* proxy, const char* propertiy, uint32_t *value);
                static bool getIPv4AddrFromIPv4ConfigProxy(GDBusProxy *ipProxy, std::string& ipAddr, uint32_t& prifix);
                static bool getIPv6AddrFromIPv6ConfigProxy(GDBusProxy *ipProxy, std::string& ipAddr, uint32_t& prifix);

                static bool convertSsidInfoToJsonObject(Exchange::INetworkManager::WiFiSSIDInfo& wifiInfo, JsonObject& ssidObj);
                static const char* convertPercentageToSignalStrengtStr(int percentage);
                static uint8_t wifiSecurityModeFromApFlags(guint32 flags, guint32 wpaFlags, guint32 rsnFlags);
                static const char* getWifiIfname();
                static const char* getEthIfname();
        };

    } // Plugin
} // WPEFramework


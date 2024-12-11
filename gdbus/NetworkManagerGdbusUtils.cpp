/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include <glib.h>
#include <gio/gio.h>
#include <string>
#include <nm-dbus-interface.h>

#include "NetworkManagerLogger.h"
#include "NetworkManagerGdbusUtils.h"
#include "NetworkManagerGdbusMgr.h"
#include <arpa/inet.h>

namespace WPEFramework
{
    namespace Plugin
    {
        static const char* ifnameEth = "eth0";
        static const char* ifnameWlan = "wlan0";

        bool GnomeUtils::getIPv4AddrFromIPv4ConfigProxy(GDBusProxy *ipProxy, std::string& ipAddr, uint32_t& prefix)
        {
            GVariantIter *iter;
            GVariantIter *dictIter;
            const gchar *key;
            GVariant *value;
            gchar *address = nullptr;
            bool ret = false;

            if(ipProxy == nullptr)
            {
                NMLOG_ERROR("ipv4 ipProxy is null");
                return false;
            }

            GVariant *addressDataVariant = g_dbus_proxy_get_cached_property(ipProxy, "AddressData");
            if (addressDataVariant != NULL) {
                g_variant_get(addressDataVariant, "aa{sv}", &iter);
                while (g_variant_iter_loop(iter, "a{sv}", &dictIter)) {
                    while (g_variant_iter_loop(dictIter, "{&sv}", &key, &value)) {
                        if (g_strcmp0(key, "address") == 0) {
                            address = g_variant_dup_string(value, NULL);
                            if(address!= NULL)
                            {
                                ipAddr = address; // strlen of "255.255.255.255"
                                NMLOG_DEBUG("Address: %s", ipAddr.c_str());
                                ret = true;
                            }
                            else
                                ret = false;
                            g_free(address);
                        } else if (g_strcmp0(key, "prefix") == 0) {
                            prefix = g_variant_get_uint32(value);
                            NMLOG_DEBUG("Prefix: %u", prefix);
                        }
                    }
                }
                g_variant_iter_free(iter);
                g_variant_unref(addressDataVariant);
            }
            return ret;
        }

        bool GnomeUtils::getIPv6AddrFromIPv6ConfigProxy( GDBusProxy *ipProxy, std::string& ipAddr, uint32_t& prefix)
        {
            GVariantIter *iter = NULL;
            GVariantIter *dictIter = NULL;
            const gchar *key = NULL;
            GVariant *value = NULL;
            gchar *address = NULL;
            std::string ipAddress{};
            if(ipProxy == NULL)
            {
                NMLOG_ERROR("ipv6 ipProxy is null");
                return false;
            }

            GVariant *addressDataVariant = g_dbus_proxy_get_cached_property(ipProxy, "AddressData");
            if (addressDataVariant != NULL) {
                g_variant_get(addressDataVariant, "aa{sv}", &iter);
                while (g_variant_iter_loop(iter, "a{sv}", &dictIter)) {
                    while (g_variant_iter_loop(dictIter, "{&sv}", &key, &value))
                    {
                        if (g_strcmp0(key, "address") == 0)
                        {
                            address = g_variant_dup_string(value, NULL);
                            if(address!= NULL)
                            {
                                ipAddress = address; // strlen of "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF"
                                if (ipAddress.compare(0, 5, "fe80:") == 0 || 
                                    ipAddress.compare(0, 6, "fe80::") == 0) { // It's link-local starts with fe80
                                    NMLOG_DEBUG("link-local ip: %s", address);
                                }
                                else
                                {
                                    // TODO use ip list multiple ipv6 SLACC adress may occure
                                    ipAddr = ipAddress;
                                    NMLOG_DEBUG("ipv6 addr: %s", ipAddress.c_str());
                                }
                            }
                            g_free(address);
                        }
                        else if (g_strcmp0(key, "prefix") == 0)
                        {
                            prefix = g_variant_get_uint32(value);
                            NMLOG_DEBUG("Prefix: %u", prefix);
                        }
                    }
                }
                g_variant_iter_free(iter);
                g_variant_unref(addressDataVariant);
            }

            if(ipAddr.empty())
                return false;
            return true;
        }

        bool GnomeUtils::convertSsidInfoToJsonObject(Exchange::INetworkManager::WiFiSSIDInfo& wifiInfo, JsonObject& ssidObj)
        {
            std::string freq{};

            ssidObj["ssid"] = wifiInfo.ssid;
            ssidObj["security"] = static_cast<int>(wifiInfo.security);
            ssidObj["strength"] = wifiInfo.strength;
            ssidObj["frequency"] = wifiInfo.frequency;
            return true;
        }

        // TODO change this function 
        const char* GnomeUtils::getWifiIfname() { return ifnameWlan; }
        const char* GnomeUtils::getEthIfname() { return ifnameEth; }

        uint8_t GnomeUtils::wifiSecurityModeFromApFlags(guint32 flags, guint32 wpaFlags, guint32 rsnFlags)
        {
            uint8_t security = Exchange::INetworkManager::WIFI_SECURITY_NONE;
            if ((flags == NM_802_11_AP_FLAGS_NONE) && (wpaFlags == NM_802_11_AP_SEC_NONE) && (rsnFlags == NM_802_11_AP_SEC_NONE))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_NONE;
            }
            else if( (flags & NM_802_11_AP_FLAGS_PRIVACY) && ((wpaFlags & NM_802_11_AP_SEC_PAIR_WEP40) || (rsnFlags & NM_802_11_AP_SEC_PAIR_WEP40)) )
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WEP_64;
            }
            else if( (flags & NM_802_11_AP_FLAGS_PRIVACY) && ((wpaFlags & NM_802_11_AP_SEC_PAIR_WEP104) || (rsnFlags & NM_802_11_AP_SEC_PAIR_WEP104)) )
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WEP_128;
            }
            else if((wpaFlags & NM_802_11_AP_SEC_PAIR_TKIP) || (rsnFlags & NM_802_11_AP_SEC_PAIR_TKIP))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK_TKIP;
            }
            else if((wpaFlags & NM_802_11_AP_SEC_PAIR_CCMP) || (rsnFlags & NM_802_11_AP_SEC_PAIR_CCMP))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK_AES;
            }
            else if ((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK) && (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_802_1X))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_WPA2_ENTERPRISE;
            }
            else if(rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_WPA2_PSK;
            }
            else if((wpaFlags & NM_802_11_AP_SEC_GROUP_CCMP) || (rsnFlags & NM_802_11_AP_SEC_GROUP_CCMP))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_PSK_AES;
            }
            else if((wpaFlags & NM_802_11_AP_SEC_GROUP_TKIP) || (rsnFlags & NM_802_11_AP_SEC_GROUP_TKIP))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_PSK_TKIP;
            }
            else if((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_OWE) || (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA3_SAE;
            }
            else
                NMLOG_WARNING("security mode not defined (flag: %d, wpaFlags: %d, rsnFlags: %d)", flags, wpaFlags, rsnFlags);
            return security;
        }

        bool GnomeUtils::getCachedPropertyU(GDBusProxy* proxy, const char* propertiy, uint32_t *value)
        {
            GVariant* result = NULL;
            result = g_dbus_proxy_get_cached_property(proxy, propertiy);
            if (result == NULL) {
                NMLOG_ERROR("Failed to get '%s' properties", propertiy);
                return false;
            }

            if (g_variant_is_of_type (result, G_VARIANT_TYPE_UINT32)) {
                *value = g_variant_get_uint32(result);
                //NMLOG_DEBUG("%s: %d", propertiy, *value);
            }
            else
                NMLOG_WARNING("Unexpected type returned property: %s", g_variant_get_type_string(result));
            g_variant_unref(result);
            return true;
        }

        bool GnomeUtils::getDevicePropertiesByPath(DbusMgr& m_dbus, const char* devicePath, deviceInfo& properties)
        {
            GVariant *devicesVar = NULL;
            GDBusProxy* nmProxy = NULL;
            u_int32_t value;

            nmProxy = m_dbus.getNetworkManagerDeviceProxy(devicePath);
            if(nmProxy == NULL)
                return false;

            if(GnomeUtils::getCachedPropertyU(nmProxy, "DeviceType", &value))
               properties.deviceType = static_cast<NMDeviceType>(value);
            else
                 NMLOG_ERROR("'DeviceType' property failed");

            devicesVar = g_dbus_proxy_get_cached_property(nmProxy, "Interface");
            if (devicesVar) {
                const gchar *iface = g_variant_get_string(devicesVar, NULL);
                if(iface != NULL)
                {
                    properties.interface = iface;
                }
                //NMLOG_DEBUG("Interface: %s", iface);
                g_variant_unref(devicesVar);
            }
            else
                NMLOG_ERROR("'Interface' property failed");

            guint32 devState, stateReason;
            devicesVar = g_dbus_proxy_get_cached_property(nmProxy, "StateReason");
            if (devicesVar) {
                g_variant_get(devicesVar, "(uu)", &devState, &stateReason);
                properties.state = static_cast<NMDeviceState>(devState);
                properties.stateReason = static_cast<NMDeviceStateReason>(stateReason);
                g_variant_unref(devicesVar);
            }
            else
                NMLOG_ERROR("'StateReason' property failed");
            
            g_object_unref(nmProxy);
            return true;
        }

        bool GnomeUtils::getDeviceInfoByIfname(DbusMgr& m_dbus, const char* ifaceName, deviceInfo& properties)
        {
            GError *error = NULL;
            GVariant *devicesVar = NULL;
            bool ret = false;

            GDBusProxy* nmProxy  = m_dbus.getNetworkManagerProxy();
            if(nmProxy == NULL)
                return false;

            devicesVar = g_dbus_proxy_call_sync(nmProxy, "GetDevices", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
            if (error) {
                NMLOG_ERROR("Error calling GetDevices method: %s", error->message);
                g_error_free(error);
                g_object_unref(nmProxy);
                return false;
            }

            GVariantIter* iter = NULL;
            gchar* devicePath = NULL;
            g_variant_get(devicesVar, "(ao)", &iter);
            while (iter != NULL && g_variant_iter_loop(iter, "o", &devicePath))
            {
                if(devicePath == NULL )
                    continue;

                if(getDevicePropertiesByPath(m_dbus, devicePath, properties) && properties.interface == ifaceName)
                {
                    properties.path = devicePath;
                    g_free(devicePath);
                    ret = true;
                    break;
                }
            }

            g_variant_iter_free(iter);
            if(!ret)
                NMLOG_ERROR("'%s' interface not found", ifaceName);
            if(devicesVar)
                g_variant_unref(devicesVar);
            if(nmProxy)
                g_object_unref(nmProxy);
            return ret;
        }

        bool GnomeUtils::getDeviceByIpIface(DbusMgr& m_dbus, const gchar *iface_name, std::string& path)
        {
            // TODO Fix Error calling method: 
            // GDBus.Error:org.freedesktop.NetworkManager.UnknownDevice: No device found for the requested iface 
            // in wsl linux
            GError *error = NULL;
            GVariant *result;
            gchar *device_path = NULL;
            bool ret = false;

            result = g_dbus_connection_call_sync(
                m_dbus.getConnection(),
                "org.freedesktop.NetworkManager",               // D-Bus name
                "/org/freedesktop/NetworkManager",              // Object path
                "org.freedesktop.NetworkManager",               // Interface
                "GetDeviceByIpIface",                           // Method name
                g_variant_new("(s)", iface_name),               // Input parameter (the interface name)
                G_VARIANT_TYPE("(o)"),                          // Expected return type (object path)
                G_DBUS_CALL_FLAGS_NONE,
                -1,                                             // Default timeout
                NULL,
                &error
            );

            if (error != NULL) {
                NMLOG_ERROR("calling GetDeviceByIpIface: %s", error->message);
                g_error_free(error);
                return ret;
            }

            if (g_variant_is_of_type (result, (const GVariantType *) "(o)"))
            {
                g_variant_get(result, "(o)", &device_path);
                if(device_path != NULL)
                {
                    path = std::string(device_path);
                    ret = true;
                    g_free(device_path);
                }
            }
            //NMLOG_DEBUG("%s device path %s", iface_name, path.c_str());
            g_variant_unref(result);
            return ret;
        }

        bool GnomeUtils::getApDetails(DbusMgr& m_dbus, const char* apPath, Exchange::INetworkManager::WiFiSSIDInfo& wifiInfo)
        {
            guint32 flags= 0, wpaFlags= 0, rsnFlags= 0, freq= 0, bitrate= 0;
            uint8_t strength = 0;
            NM80211Mode mode = NM_802_11_MODE_UNKNOWN;
            bool ret = false;
            GVariant* ssidVariant = NULL;

            GDBusProxy* proxy = m_dbus.getNetworkManagerAccessPointProxy(apPath);
            if (proxy == NULL) {
                return false;
            }

            gsize ssid_length = 0;
            ssidVariant = g_dbus_proxy_get_cached_property(proxy,"Ssid");
            if (!ssidVariant) {
                NMLOG_ERROR("Failed to get AP properties.");
                g_object_unref(proxy);
                return false;
            }

            const guchar *ssid_data = static_cast<const guchar*>(g_variant_get_fixed_array(ssidVariant, &ssid_length, sizeof(guchar)));
            if (ssid_data && ssid_length > 0 && ssid_length <= 32)
            {
                GVariant* result = NULL;
                gchar *_bssid = NULL;
                wifiInfo.ssid.assign(reinterpret_cast<const char*>(ssid_data), ssid_length);
                result = g_dbus_proxy_get_cached_property(proxy,"HwAddress");
                if (!result) {
                    NMLOG_ERROR("Failed to get AP properties.");
                    g_object_unref(proxy);
                    return false;
                }
                g_variant_get(result, "s", &_bssid);
                if(_bssid != NULL) {
                    wifiInfo.bssid = _bssid;
                    g_free(_bssid);
                }
                g_variant_unref(result);
                result = NULL;
                result = g_dbus_proxy_get_cached_property(proxy,"Strength");
                if (!result) {
                    NMLOG_ERROR("Failed to get AP properties.");
                    g_object_unref(proxy);
                    return false;
                }
                g_variant_get(result, "y", &strength);
                wifiInfo.strength = GnomeUtils::convertPercentageToSignalStrengtStr((int)strength);
                g_variant_unref(result);

                GnomeUtils::getCachedPropertyU(proxy, "Flags", &flags);
                GnomeUtils::getCachedPropertyU(proxy, "WpaFlags", &wpaFlags);
                GnomeUtils::getCachedPropertyU(proxy, "RsnFlags", &rsnFlags);
                GnomeUtils::getCachedPropertyU(proxy, "Mode", (guint32*)&mode);
                GnomeUtils::getCachedPropertyU(proxy, "Frequency", &freq);
                GnomeUtils::getCachedPropertyU(proxy, "MaxBitrate", &bitrate);

                wifiInfo.frequency = std::to_string((double)freq/1000);
                wifiInfo.rate = std::to_string(bitrate);
                wifiInfo.security = static_cast<Exchange::INetworkManager::WIFISecurityMode>(wifiSecurityModeFromApFlags(flags, wpaFlags, rsnFlags));

                // NMLOG_DEBUG("SSID: %s", wifiInfo.m_ssid.c_str());
                // NMLOG_DEBUG("bssid %s", wifiInfo.m_bssid.c_str());
                // NMLOG_DEBUG("strength : %s dbm (%d%%)", wifiInfo.m_signalStrength.c_str(), strength);
                // NMLOG_DEBUG("frequency : %f MHz", wifiInfo.m_frequency);
                // NMLOG_DEBUG("bitrate : %s kbit/s", wifiInfo.m_rate.c_str());
                // NMLOG_DEBUG("securityMode : %d", wifiInfo.m_securityMode);
 
                // TODO add noice
                ret = true;
            }
            else {
               // NMLOG_DEBUG("Invalid/hidden SSID: %zu (maximum is 32)", ssid_length);
                ret = false;
            }

            if(ssidVariant)
                g_variant_unref(ssidVariant);
            if(proxy)
                g_object_unref(proxy);

            return ret;
        }

        // Function to convert percentage (0-100) to dBm string
        const char* GnomeUtils::convertPercentageToSignalStrengtStr(int percentage) {

            if (percentage <= 0 || percentage > 100) {
                return "";
            }

           /*
            * -30 dBm to -50 dBm: Excellent signal strength.
            * -50 dBm to -60 dBm: Very good signal strength.
            * -60 dBm to -70 dBm: Good signal strength; acceptable for basic internet browsing.
            * -70 dBm to -80 dBm: Weak signal; performance may degrade, slower speeds, and possible dropouts.
            * -80 dBm to -90 dBm: Very poor signal; likely unusable or highly unreliable.
            *  Below -90 dBm: Disconnected or too weak to establish a stable connection.
            */

            // dBm range: -30 dBm (strong) to -90 dBm (weak)
            const int max_dBm = -30;
            const int min_dBm = -90;
            int dBm_value = max_dBm + ((min_dBm - max_dBm) * (100 - percentage)) / 100;
            static char result[8]={0};
            snprintf(result, sizeof(result), "%d", dBm_value);
            return result;
        }

        bool GnomeUtils::getWifiConnectionPaths(DbusMgr& m_dbus, const char* devicePath, std::list<std::string>& paths)
        {
            GDBusProxy* nmProxy = NULL;
            GVariant* result = NULL;
            bool ret = false;
            nmProxy = m_dbus.getNetworkManagerDeviceProxy(devicePath);
            if(nmProxy == NULL)
                return false;

            result = g_dbus_proxy_get_cached_property(nmProxy, "AvailableConnections");
            if (!result) {
                NMLOG_ERROR("Failed to get AvailableConnections property.");
                g_object_unref(nmProxy);
                return ret;
            }

            GVariantIter* iter = NULL;
            gchar* connPath = NULL;
            g_variant_get(result, "ao", &iter);
            while (g_variant_iter_loop(iter, "o", &connPath))
            {
                if(connPath == NULL)
                    continue;
                if(g_strcmp0(connPath, "/") != 0)
                {
                    paths.push_back(connPath);
                    // NMLOG_DEBUG("AvailableConnections Path: %s", connPath);
                    ret = true;
                }
            }

            g_variant_iter_free(iter);
            g_variant_unref(result);
            g_object_unref(nmProxy);
            return ret;
        }

        bool GnomeUtils::getConnectionPaths(DbusMgr& m_dbus, std::list<std::string>& pathsList)
        {
            GDBusProxy *sProxy= NULL;
            GError *error= NULL;
            GVariant *listProxy = NULL;
            char **paths = NULL;

            sProxy = m_dbus.getNetworkManagerSettingsProxy();
            if(sProxy == NULL)
                return false;

            listProxy = g_dbus_proxy_call_sync(sProxy,
                                        "ListConnections",
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
            if(listProxy == NULL)
            {
                if (!error) {
                    NMLOG_ERROR("ListConnections failed: %s", error->message);
                    g_error_free(error);
                    g_object_unref(sProxy);
                    return false;
                }
                else
                    NMLOG_ERROR("ListConnections proxy failed no error message");
            }

            g_variant_get(listProxy, "(^ao)", &paths);
            g_variant_unref(listProxy);

            if(paths == NULL)
            {
                NMLOG_WARNING("no connection path available");
                g_object_unref(sProxy);
                return false;
            }

            for (int i = 0; paths[i]; i++) {
                //NMLOG_DEBUG("Connection path: %s", paths[i]);
                pathsList.push_back(paths[i]);
            }

            g_strfreev(paths);
            g_object_unref(sProxy);
            if(pathsList.empty())
                return false;

            return true;
        }

    } // Plugin
} // WPEFramework


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

#include <glib.h>
#include <gio/gio.h>
#include <string>
#include <cstring>
#include <nm-dbus-interface.h>

#include "NetworkManagerLogger.h"
#include "NetworkManagerGdbusUtils.h"
#include "NetworkManagerGdbusMgr.h"
#include "NetworkManagerImplementation.h"
#include <arpa/inet.h>
#include <netinet/in.h> // for struct in_addr

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
            else if((wpaFlags & NM_802_11_AP_SEC_PAIR_TKIP) || (rsnFlags & NM_802_11_AP_SEC_PAIR_TKIP))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK;
            }
            else if((wpaFlags & NM_802_11_AP_SEC_PAIR_CCMP) || (rsnFlags & NM_802_11_AP_SEC_PAIR_CCMP))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK;
            }
            else if ((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK) && (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_802_1X))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_EAP;
            }
            else if(rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK;
            }
            else if((wpaFlags & NM_802_11_AP_SEC_GROUP_CCMP) || (rsnFlags & NM_802_11_AP_SEC_GROUP_CCMP))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK;
            }
            else if((wpaFlags & NM_802_11_AP_SEC_GROUP_TKIP) || (rsnFlags & NM_802_11_AP_SEC_GROUP_TKIP))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK;
            }
            else if((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_OWE) || (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM))
            {
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE;
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

        bool GnomeUtils::getCachedPropertyBoolean(GDBusProxy* proxy, const char* property, bool *value)
        {
            GVariant* result = nullptr;
            result = g_dbus_proxy_get_cached_property(proxy, property);
            if (result == nullptr) {
                NMLOG_ERROR("Failed to get '%s' properties", property);
                return false;
            }

            if (g_variant_is_of_type(result, G_VARIANT_TYPE_BOOLEAN)) {
                *value = g_variant_get_boolean(result);
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
            u_int32_t value = 0;
            bool managedValue;

            nmProxy = m_dbus.getNetworkManagerDeviceProxy(devicePath);
            if(nmProxy == NULL)
                return false;

            if(GnomeUtils::getCachedPropertyU(nmProxy, "DeviceType", &value))
                properties.deviceType = static_cast<NMDeviceType>(value);
            else
                NMLOG_ERROR("'DeviceType' property failed");

            if(GnomeUtils::getCachedPropertyBoolean(nmProxy, "Managed", &managedValue))
                properties.managed = managedValue;
            else
                NMLOG_ERROR("'Managed' property failed");
            devicesVar = g_dbus_proxy_get_cached_property(nmProxy, "HwAddress");
            if (devicesVar) {
                const gchar *mac = g_variant_get_string(devicesVar, NULL);
                if(mac != NULL)
                {
                    properties.MAC = mac;
                }
                g_variant_unref(devicesVar);
            }
            else
                NMLOG_ERROR("'mac' property failed");

            devicesVar = g_dbus_proxy_get_cached_property(nmProxy, "Interface");
            if (devicesVar) {
                const gchar *iface = g_variant_get_string(devicesVar, NULL);
                if(iface != NULL)
                {
                    properties.interface = iface;
                }
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

        bool GnomeUtils::getDeviceByIpIface(DbusMgr& m_dbus, const gchar *ifaceName, std::string& path)
        {
            GError *error = nullptr;
            GDBusProxy* nmProxy  = m_dbus.getNetworkManagerProxy();
            if(nmProxy == NULL)
                return false;

            GVariant *result = g_dbus_proxy_call_sync(
                    nmProxy,
                    "GetDeviceByIpIface",
                    g_variant_new("(s)", ifaceName),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (result == nullptr) {
                NMLOG_ERROR("Error calling GetDeviceByIpIface: %s", error->message);
                g_clear_error(&error);
                g_object_unref(nmProxy);
                return false;
            }

            gchar *devicePath;
            g_variant_get(result, "(o)", &devicePath);

            g_variant_unref(result);
            g_object_unref(nmProxy);

            path = std::string(devicePath);
            g_free(devicePath);

            return true;
        }

        bool GnomeUtils::getApDetails(DbusMgr& m_dbus, const char* apPath, Exchange::INetworkManager::WiFiSSIDInfo& wifiInfo)
        {
            guint32 flags= 0, wpaFlags= 0, rsnFlags= 0, freq= 0, bitrate= 0;
            uint8_t strength = 0;
            gint16  noise = 0;
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
                if(noise <= 0 || noise >= m_defaultNoise)
                    wifiInfo.noise = std::to_string(noise);
                else
                    wifiInfo.noise = std::to_string(m_defaultNoise);

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

        bool GnomeUtils::activateConnection(DbusMgr& m_dbus, const std::string& connectionProfile, const std::string& devicePath)
        {
            GError* error = nullptr;
            GDBusProxy* nmProxy  = m_dbus.getNetworkManagerProxy();
            if(nmProxy == NULL)
                return false;

            GVariant* result = g_dbus_proxy_call_sync(
                    nmProxy,
                    "ActivateConnection",
                    g_variant_new("(ooo)", connectionProfile.c_str(), devicePath.c_str(), "/"),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error
                    );

            if (error) {
                NMLOG_ERROR("ActivateConnection Error: %s", error->message);
                g_error_free(error);
            } else if (result != nullptr) {
                g_variant_unref(result);
            }
            return true;
        }

        bool GnomeUtils::getSettingsConnectionPath(DbusMgr &m_dbus, std::string& connectionPath, const std::string& interface)
        {
            GDBusProxy *nmProxy = nullptr;
            bool found = false;
            nmProxy = m_dbus.getNetworkManagerProxy();
            if(nmProxy == nullptr)
                return false;
            GVariant *activeConnections = g_dbus_proxy_get_cached_property(nmProxy, "ActiveConnections");
            if (activeConnections == nullptr) {
                NMLOG_ERROR("Error retrieving active connections");
                g_object_unref(nmProxy);
                return false;
            }

            GVariantIter iter;
            g_variant_iter_init(&iter, activeConnections);
            gchar *activeConnectionPath = nullptr;
            while (g_variant_iter_loop(&iter, "o", &activeConnectionPath)) {
                GDBusProxy *activeConnectionProxy = m_dbus.getNetworkManagerActiveConnProxy(activeConnectionPath);

                if (activeConnectionProxy == nullptr) {
                    NMLOG_ERROR("Error creating active connection proxy");
                    continue;
                }

                GVariant *devicesVar = g_dbus_proxy_get_cached_property(activeConnectionProxy, "Devices");
                if (devicesVar == nullptr) {
                    NMLOG_ERROR("Error retrieving devices property");
                    g_object_unref(activeConnectionProxy);
                    continue;
                }

                GVariantIter devicesIter;
                g_variant_iter_init(&devicesIter, devicesVar);
                gchar *devicePath = nullptr;

                while (g_variant_iter_loop(&devicesIter, "o", &devicePath)) {
                    GDBusProxy *deviceProxy = m_dbus.getNetworkManagerDeviceProxy(devicePath);

                    if (deviceProxy == nullptr) {
                        NMLOG_ERROR("Error creating device proxy");
                        continue;
                    }

                    GVariant *ifaceProperty = g_dbus_proxy_get_cached_property(deviceProxy, "Interface");
                    if (ifaceProperty) {
                        const gchar *iface = g_variant_get_string(ifaceProperty, nullptr);
                        if (interface == iface) {
                            found = true;
                            GVariant *connectionProperty = g_dbus_proxy_get_cached_property(activeConnectionProxy, "Connection");
                            if (connectionProperty) {
                                connectionPath = g_variant_get_string(connectionProperty, nullptr);
                                g_variant_unref(connectionProperty);
                            } else {
                                NMLOG_ERROR("Error retrieving connection property");
                            }

                            g_variant_unref(ifaceProperty);
                            g_object_unref(deviceProxy);
                            break;
                        }
                        g_variant_unref(ifaceProperty);
                    }

                    g_object_unref(deviceProxy);
                }
                g_variant_unref(devicesVar);
                g_object_unref(activeConnectionProxy);

                if (found) {
                    break;
                }
            }

            g_variant_unref(activeConnections);
            g_object_unref(nmProxy);
            return true;
        }

        // Convert IPv4 string to network byte order (NBO)
        uint32_t GnomeUtils::ip4StrToNBO(const std::string &ipAddress)
        {
            struct in_addr addr;
            if (inet_pton(AF_INET, ipAddress.c_str(), &addr) != 1) {
                NMLOG_ERROR("Invalid IPv4 address format: %s", ipAddress.c_str());
            }
            return addr.s_addr;
        }

        // Convert an IPv6 string address to an array of bytes
        std::array<guint8, 16> GnomeUtils::ip6StrToNBO(const std::string &ipAddress)
        {
            struct in6_addr addr6;
            inet_pton(AF_INET6, ipAddress.c_str(), &addr6);
            std::array<guint8, 16> ip6{};
            std::memcpy(ip6.data(), &addr6, 16);
            return ip6;
        }

        // Helper function to convert a raw IPv4 address to human-readable format
        std::string GnomeUtils::ip4ToString(uint32_t ip) {
            ip = ntohl(ip); // Convert from network to host byte order
            char buf[INET_ADDRSTRLEN] = {0};
            snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                    (ip >> 24) & 0xFF,
                    (ip >> 16) & 0xFF,
                    (ip >>  8) & 0xFF,
                    ip & 0xFF);
            return std::string(buf);
        }

        // Helper function to convert a raw IPv6 address to human-readable format
        std::string GnomeUtils::ip6ToString(const uint8_t *ipv6) {
            char buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, ipv6, buf, sizeof(buf));
            return std::string(buf);
        }

        // Helper function to add elements from GVariant to GVariantBuilder
        void GnomeUtils::addGvariantToBuilder(GVariant *variant, GVariantBuilder *builder, gboolean excludeRouteMetric) {
            GVariantIter iter;

            const gchar *key;
            GVariant *value;
            g_variant_iter_init(&iter, variant);

            // Iterate through key-value pairs in the dictionary
            while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
                if (excludeRouteMetric && strcmp(key, "route-metric") == 0) {
                    // Skip adding this key-value pair if exclude_route_metric is TRUE and key is "route-metric"
                    continue;
                }
                g_variant_builder_add(builder, "{sv}", key, value);
            }
        }
    } // Plugin
} // WPEFramework


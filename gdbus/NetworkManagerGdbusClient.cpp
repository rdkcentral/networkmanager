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
#include <list>
#include <uuid/uuid.h>
#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include "NetworkManagerLogger.h"
#include "NetworkManagerGdbusClient.h"
#include "NetworkManagerGdbusUtils.h"

namespace WPEFramework
{
    namespace Plugin
    {

        NetworkManagerClient::NetworkManagerClient() {
            NMLOG_INFO("NetworkManagerClient");
        }

        NetworkManagerClient::~NetworkManagerClient() {
            NMLOG_INFO("~NetworkManagerClient");
        }

        static bool getSSIDFromConnection(DbusMgr &m_dbus, const std::string connPath, std::string& ssid)
        {
            GError *error = NULL;
            GDBusProxy *ConnProxy = NULL;
            GVariant *settingsProxy= NULL, *connection= NULL, *gVarConn= NULL;
            bool ret = false;

            ConnProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(connPath.c_str());
            if(ConnProxy == NULL)
                return false;
        
            settingsProxy = g_dbus_proxy_call_sync(ConnProxy,
                                    "GetSettings",
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);
            if (!settingsProxy) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_ERROR("Failed to get connection settings: %s", error->message);
                g_error_free(error);
                g_object_unref(ConnProxy);
                return false;
            }

            g_variant_get(settingsProxy, "(@a{sa{sv}})", &connection);
            gVarConn = g_variant_lookup_value(connection, "connection", NULL);

            if(gVarConn == NULL) {
                NMLOG_ERROR("connection Gvarient Error");
                g_variant_unref(settingsProxy);
                g_object_unref(ConnProxy);
                return false;
            }

            if(gVarConn)
            {
                const char *connTyp = NULL;
                const char *interfaceName = NULL;
                G_VARIANT_LOOKUP(gVarConn, "type", "&s", &connTyp);
                G_VARIANT_LOOKUP(gVarConn, "interface-name", "&s", &interfaceName);
                if((strcmp(connTyp, "802-11-wireless") == 0) && strcmp(interfaceName, GnomeUtils::getWifiIfname()) == 0 )
                {
                    // 802-11-wireless.ssid: <ssid>
                    GVariant *setting = NULL;
                    setting = g_variant_lookup_value(connection, "802-11-wireless", NULL);
                    if(setting)
                    {
                        GVariantIter iter;
                        GVariant *value = NULL;
                        const char  *propertyName;
                        g_variant_iter_init(&iter, setting);
                        while (g_variant_iter_next(&iter, "{&sv}", &propertyName, &value)) {
                            if (strcmp(propertyName, "ssid") == 0) {
                                // Decode SSID from GVariant of type "ay"
                                gsize ssid_length = 0;
                                const guchar *ssid_data = static_cast<const guchar*>(g_variant_get_fixed_array(value, &ssid_length, sizeof(guchar)));
                                if (ssid_data && ssid_length > 0 && ssid_length <= 32) {
                                    ssid.assign(reinterpret_cast<const char*>(ssid_data), ssid_length);
                                    NMLOG_DEBUG("SSID: %s", ssid.c_str());
                                } else {
                                    NMLOG_ERROR("Invalid SSID length: %zu (maximum is 32)", ssid_length);
                                    ssid.empty();
                                }
                            }
                            if(value) {
                                g_variant_unref(value);
                                value = NULL;
                            }
                        }
                        g_variant_unref(setting);
                        setting = NULL;
                    }

                    if(!ssid.empty())
                    {
                        // 802-11-wireless-security.key-mgmt: wpa-psk
                        const char* keyMgmt = NULL;
                        setting = g_variant_lookup_value(connection, "802-11-wireless-security", NULL);
                        if(setting != NULL)
                        {
                            G_VARIANT_LOOKUP(setting, "key-mgmt", "&s", &keyMgmt);
                            if(keyMgmt != NULL)
                                NMLOG_DEBUG("802-11-wireless-security.key-mgmt: %s", keyMgmt);
                            g_variant_unref(setting);
                            setting = NULL;
                        }
                        ret = true;
                    }
                }
                else
                    ret = false;
                g_variant_unref(gVarConn);
            }

            if (connection)
                g_variant_unref(connection);
            if (settingsProxy)
                g_variant_unref(settingsProxy);
            g_object_unref(ConnProxy);

            return ret;
        }

        static bool deleteConnection(DbusMgr m_dbus, const std::string& path, std::string& ssid)
        {
            GError *error = NULL;
            GDBusProxy *ConnProxy = NULL;
            GVariant *deleteVar= NULL;

            ConnProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(path.c_str());
            if(ConnProxy == NULL)
                return false;

            deleteVar = g_dbus_proxy_call_sync(ConnProxy,
                                    "Delete",
                                    NULL,
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);
            if (!deleteVar) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_ERROR("Failed to get connection settings: %s", error->message);
                g_error_free(error);
                g_object_unref(ConnProxy);
                return false;
            }
            else
               NMLOG_INFO("connection '%s' Removed path: %s", ssid.c_str(), path.c_str()); 

            if (deleteVar)
                g_variant_unref(deleteVar);
            g_object_unref(ConnProxy);

            return true;
        }

        bool NetworkManagerClient::getKnownSSIDs(std::list<std::string>& ssids)
        {
            std::list<std::string> paths;
            if(!GnomeUtils::getConnectionPaths(m_dbus, paths))
            {
                NMLOG_ERROR("Connection path fetch failed");
                return false;
            }

            for (const std::string& path : paths) {
               // NMLOG_DEBUG("connection path %s", path.c_str());
                std::string ssid;
                if(getSSIDFromConnection(m_dbus, path, ssid) && !ssid.empty())
                    ssids.push_back(ssid);
            }

            if(ssids.empty())
            {
                NMLOG_WARNING("no Known SSID list empty");
                return false;
            }
            return true;
        }

        bool NetworkManagerClient::getConnectedSSID(Exchange::INetworkManager::WiFiSSIDInfo &ssidinfo)
        {
            GDBusProxy* wProxy = NULL;
            GVariant* result = NULL;
            gchar *activeApPath = NULL;
            bool ret = false;
            deviceInfo devInfo{};

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), devInfo))
                return false;

            if(devInfo.path.empty() || devInfo.state < NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_ERROR("access point state not valid %d", devInfo.state);
                return false;
            }

            wProxy = m_dbus.getNetworkManagerWirelessProxy(devInfo.path.c_str());
            if(wProxy == NULL)
                return false;

            result = g_dbus_proxy_get_cached_property(wProxy, "ActiveAccessPoint");
            if (!result) {
                NMLOG_ERROR("Failed to get ActiveAccessPoint property.");
                g_object_unref(wProxy);
                return false;
            }

            g_variant_get(result, "o", &activeApPath);

            if(activeApPath != NULL && g_strcmp0(activeApPath, "/") != 0)
            {
                //NMLOG_DEBUG("ActiveAccessPoint property path %s", activeApPath);
                if(GnomeUtils::getApDetails(m_dbus, activeApPath, ssidinfo))
                {
                    ret = true;
                }
            }
            else
                NMLOG_WARNING("active access point not found");

            if(activeApPath != NULL)
                g_free(activeApPath);
            g_variant_unref(result);
            g_object_unref(wProxy);
            return ret;
        }

        bool NetworkManagerClient::getAvailableSSIDs(std::list<std::string>& ssids)
        {
            GError* error = nullptr;
            GDBusProxy* wProxy = nullptr;
            deviceInfo devProperty{};

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), devProperty))
                return false;

            if(devProperty.path.empty() || devProperty.state < NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_ERROR("no wifi device found");
                return false;
            }

            wProxy = m_dbus.getNetworkManagerWirelessProxy(devProperty.path.c_str());
            if (wProxy == NULL)
                return false;

            GVariant* result = g_dbus_proxy_call_sync(wProxy, "GetAllAccessPoints", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
            if (error) {
                NMLOG_ERROR("Error creating proxy: %s", error->message);
                g_error_free(error);
                g_object_unref(wProxy);
                return false;
            }

            GVariantIter* iter = NULL;
            const gchar* apPath = NULL;
            g_variant_get(result, "(ao)", &iter);

            while (g_variant_iter_loop(iter, "o", &apPath)) {
                Exchange::INetworkManager::WiFiSSIDInfo wifiInfo{};
                NMLOG_DEBUG("Access Point Path: %s", apPath);
                if(GnomeUtils::getApDetails(m_dbus, apPath, wifiInfo))
                    ssids.push_back(wifiInfo.ssid);
            }

            g_variant_iter_free(iter);
            g_variant_unref(result);
            g_object_unref(wProxy);

            return true;
        }

        bool NetworkManagerClient::startWifiScan(const std::string ssid)
        {
            GError* error = NULL;
            GDBusProxy* wProxy = NULL;
            deviceInfo devInfo;

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), devInfo))
                return false;

            if(devInfo.path.empty() || devInfo.state < NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_WARNING("access point not active");
                return false;
            }

            wProxy = m_dbus.getNetworkManagerWirelessProxy(devInfo.path.c_str());
            if(wProxy == NULL)
                return false;

            GVariant* timestampVariant = NULL;
            timestampVariant = g_dbus_proxy_get_cached_property(wProxy, "LastScan");
            if (!timestampVariant) {
                NMLOG_ERROR("Failed to get LastScan properties");
                g_object_unref(wProxy);
                return false;
            }

            if (g_variant_is_of_type (timestampVariant, G_VARIANT_TYPE_INT64)) {
                gint64 timestamp;
                timestamp = g_variant_get_int64 (timestampVariant);
                NMLOG_DEBUG("Last scan timestamp: %lld", static_cast<long long int>(timestamp));
            } else {
                g_warning("Unexpected variant type: %s", g_variant_get_type_string (timestampVariant));
            }

            if (!ssid.empty()) 
            {
                GVariantBuilder builder;
                NMLOG_INFO("Starting WiFi scanning with SSID: %s", ssid.c_str());
                GVariantBuilder ssid_array_builder;
                g_variant_builder_init(&ssid_array_builder, G_VARIANT_TYPE("aay"));
                g_variant_builder_add_value(&ssid_array_builder, 
                            g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, (const guint8 *)ssid.c_str(), ssid.length(),1));
                /* ssid GVariant = [['S', 'S', 'I', 'D']] */
                g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
                g_variant_builder_add(&builder, "{sv}", "ssids", g_variant_builder_end(&ssid_array_builder));
                g_dbus_proxy_call_sync(wProxy, "RequestScan", g_variant_new("(a{sv})", builder),
                                                             G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
            }

            else {
                g_dbus_proxy_call_sync(wProxy, "RequestScan", g_variant_new("(a{sv})", NULL),
                                                             G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
            }

            if (error)
            {
                NMLOG_ERROR("Error RequestScan: %s", error->message);
                g_error_free(error);
                g_object_unref(wProxy);
                return false;
            }

            g_object_unref(wProxy);
            return true;
        }

        static char * nm_utils_uuid_generate (void)
        {
            uuid_t uuid;
            char *buf;

            buf = static_cast<char*>(g_malloc0(37));
            uuid_generate_random (uuid);
            uuid_unparse_lower (uuid, &buf[0]);
            return buf;
        }

        bool updateConnctionAndactivate(DbusMgr& m_dbus, GVariantBuilder& connBuilder, const char* devicePath, const char* connPath)
        {
            GDBusProxy* proxy = nullptr;
            GError* error = nullptr;
            GVariant* result = nullptr;

            proxy = m_dbus.getNetworkManagerSettingsConnectionProxy(connPath);
            if(proxy == NULL)
                return false;

            result = g_dbus_proxy_call_sync (proxy, "Update",
                g_variant_new("(a{sa{sv}})", connBuilder),
                G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

            if (error) {
                NMLOG_ERROR("Failed to call Update : %s", error->message);
                g_clear_error(&error);
                g_object_unref(proxy);
                return false;
            }

            if(result)
                g_variant_unref(result);
            g_object_unref(proxy);

            proxy = m_dbus.getNetworkManagerProxy();
            if(proxy == NULL)
                return false;

            if (error) {
                NMLOG_ERROR( "Failed to create proxy: %s", error->message);
                g_clear_error(&error);
                return false;
            }
            const char* specificObject = "/";

            result = g_dbus_proxy_call_sync (proxy, "ActivateConnection",
                g_variant_new("(ooo)", connPath, devicePath, specificObject),
                G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

            if (error) {
                NMLOG_ERROR("Failed to call ActivateConnection: %s", error->message);
                g_clear_error(&error);
                g_object_unref(proxy);
                return false;
            }

            if(result) {
                GVariant* activeConnVariant;
                g_variant_get(result, "(@o)", &activeConnVariant);
                const char* activeConnPath = g_variant_get_string(activeConnVariant, nullptr);
                NMLOG_DEBUG("connPath %s; activeconn %s", connPath, activeConnPath);
                g_variant_unref(activeConnVariant);
            }

            if(result)
                g_variant_unref(result);
            if(proxy)
                g_object_unref(proxy);

            return true;
        }

        bool addNewConnctionAndactivate(DbusMgr& m_dbus, GVariantBuilder connBuilder, const char* devicePath, bool persist)
        {
            GDBusProxy* proxy = nullptr;
            GError* error = nullptr;
            GVariant* result = nullptr;

            proxy = m_dbus.getNetworkManagerProxy();
            if(proxy == NULL)
                return false;

            const char* specific_object = "/";

            GVariantBuilder optionsBuilder;
            g_variant_builder_init (&optionsBuilder, G_VARIANT_TYPE ("a{sv}"));
            if(!persist) // by default it will be in disk mode
            {
                NMLOG_WARNING("wifi connection will not persist to the disk");
                g_variant_builder_add(&optionsBuilder, "{sv}", "persist", g_variant_new_string("volatile"));
            }
            //else
                //g_variant_builder_add(&optionsBuilder, "{sv}", "persist", g_variant_new_string("disk"));

            result = g_dbus_proxy_call_sync (proxy, "AddAndActivateConnection2",
                g_variant_new("(a{sa{sv}}ooa{sv})", connBuilder, devicePath, specific_object, optionsBuilder),
                G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

            if (error) {
                NMLOG_ERROR("Failed to call AddAndActivateConnection2: %s", error->message);
                g_clear_error(&error);
                g_object_unref(proxy);
                return false;
            }

            GVariant* pathVariant;
            GVariant* activeConnVariant;
            GVariant* resultDict;

            g_variant_get(result, "(@o@o@a{sv})", &pathVariant, &activeConnVariant, &resultDict);

            // Extract connection and active connection paths
            const char* newConnPath = g_variant_get_string(pathVariant, nullptr);
            const char* activeConnPath = g_variant_get_string(activeConnVariant, nullptr);

            NMLOG_INFO("newconn %s; activeconn %s",newConnPath, activeConnPath);
            if(pathVariant)
                g_variant_unref(pathVariant);
            if(activeConnVariant)
                g_variant_unref(activeConnVariant);
            if(resultDict)
                g_variant_unref(resultDict);
            if(result)
                g_variant_unref(result);
            if(proxy)
                g_object_unref(proxy);
            return true;
        }

        static bool gVariantConnectionBuilder(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo, GVariantBuilder& connBuilder)
        {
            char *uuid = NULL;
            GVariantBuilder settingsBuilder;

            if(ssidinfo.ssid.empty() || ssidinfo.ssid.length() > 32)
            {
                NMLOG_WARNING("ssid name is missing or invalied");
                return false;
            }

            NMLOG_INFO("ssid %s, security %d, persist %d", ssidinfo.ssid.c_str(), (int)ssidinfo.security, (int)ssidinfo.persist);
            /* Build up the complete connection */
            g_variant_builder_init (&connBuilder, G_VARIANT_TYPE ("a{sa{sv}}"));

            /* Build up the 'connection' Setting */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));

            uuid = nm_utils_uuid_generate();
            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_CONNECTION_UUID,
                                g_variant_new_string (uuid));
            g_free (uuid);

            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_CONNECTION_ID,
                                g_variant_new_string (ssidinfo.ssid.c_str())); // connection id = ssid name

            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_CONNECTION_INTERFACE_NAME,
                                g_variant_new_string (GnomeUtils::getWifiIfname()));

            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_CONNECTION_TYPE,
                                g_variant_new_string (NM_SETTING_WIRELESS_SETTING_NAME));

            g_variant_builder_add (&connBuilder, "{sa{sv}}",
                                NM_SETTING_CONNECTION_SETTING_NAME,
                                &settingsBuilder);

            /* Add the '802-11-wireless' Setting */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));
            GVariant *ssid_array = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, (const guint8 *)ssidinfo.ssid.c_str(), ssidinfo.ssid.length(), 1);
            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_WIRELESS_SSID,
                                ssid_array);

            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_WIRELESS_MODE,
                                g_variant_new_string(NM_SETTING_WIRELESS_MODE_INFRA));

            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_WIRELESS_HIDDEN,
                                g_variant_new_boolean(true)); // set hidden: yes

            g_variant_builder_add (&connBuilder, "{sa{sv}}",
                                NM_SETTING_WIRELESS_SETTING_NAME,
                                &settingsBuilder);

             /* Build up the '802-11-wireless-security' settings */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));

            switch(ssidinfo.security)
            {
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK_TKIP:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK_AES:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_PSK_TKIP:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_PSK_AES:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_WPA2_PSK:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA3_PSK_AES:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA3_SAE:
                {
                    if(ssidinfo.passphrase.empty() || ssidinfo.passphrase.length() < 8)
                    {
                        NMLOG_WARNING("wifi securtity type password erro length > 8");
                        return false;
                    }

                    if(Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA3_SAE == ssidinfo.security)
                    {
                        NMLOG_DEBUG("802-11-wireless-security key-mgmt: 'sae'");
                        g_variant_builder_add (&settingsBuilder, "{sv}", NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
                                                                        g_variant_new_string("sae"));
                    }
                    else
                    {
                        NMLOG_DEBUG("802-11-wireless-security key-mgmt: 'wpa-psk'");
                        g_variant_builder_add (&settingsBuilder, "{sv}", NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
                                                                        g_variant_new_string("wpa-psk"));  // WPA + WPA2 + WPA3 personal
                    }
                    g_variant_builder_add (&settingsBuilder, "{sv}", NM_SETTING_WIRELESS_SECURITY_PSK,
                                                            g_variant_new_string(ssidinfo.passphrase.c_str()));
                    break;
                }

                case Exchange::INetworkManager::WIFI_SECURITY_NONE:
                {
                    NMLOG_DEBUG("802-11-wireless-security key-mgmt: 'none'");
                    g_variant_builder_add (&settingsBuilder, "{sv}", NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
                                                            g_variant_new_string("none"));  // no password protection
                    break;
                }
                default:
                {
                    NMLOG_WARNING("connection wifi securtity type not supported %d", ssidinfo.security);
                    return false;
                }
            }

            g_variant_builder_add (&connBuilder, "{sa{sv}}",
                        NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                        &settingsBuilder);

            /* Build up the 'ipv4' Setting */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));
            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_IP_CONFIG_METHOD,
                                g_variant_new_string (NM_SETTING_IP4_CONFIG_METHOD_AUTO));

            g_variant_builder_add (&connBuilder, "{sa{sv}}",
                                NM_SETTING_IP4_CONFIG_SETTING_NAME,
                                &settingsBuilder);

            /* Build up the 'ipv6' Setting */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));
            g_variant_builder_add (&settingsBuilder, "{sv}",
                                NM_SETTING_IP_CONFIG_METHOD,
                                g_variant_new_string (NM_SETTING_IP6_CONFIG_METHOD_AUTO));

            g_variant_builder_add (&connBuilder, "{sa{sv}}",
                                NM_SETTING_IP6_CONFIG_SETTING_NAME,
                                &settingsBuilder);
            NMLOG_DEBUG("connection builder success...");
            return true;
        }

        bool NetworkManagerClient::addToKnownSSIDs(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo)
        {
            // TODO check argument length
            GVariantBuilder connBuilder;
            bool ret = false, reuseConnection = false;
            GVariant *result = NULL;
            GError* error = NULL;
            const char *newConPath  = NULL;
            std::string exsistingConn;
            deviceInfo deviceProp;

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), deviceProp))
                return false;

            if(deviceProp.path.empty() || deviceProp.state == NM_DEVICE_STATE_UNKNOWN)
            {
                NMLOG_WARNING("wlan0 interface not active");
                return false;
            }

            std::list<std::string> paths{};
            if(GnomeUtils::getWifiConnectionPaths(m_dbus, deviceProp.path.c_str(), paths))
            {
                for (const std::string& path : paths)
                {
                    std::string ssid{};
                    if(getSSIDFromConnection(m_dbus, path, ssid) && ssid == ssidinfo.ssid)
                    {
                        exsistingConn = path;
                        NMLOG_DEBUG("same connection exsist (%s) updating ...", exsistingConn.c_str());
                        reuseConnection = true;
                        break; // only one connection will be activate (same property only)
                    }
                }
            }

            if(!gVariantConnectionBuilder(ssidinfo, connBuilder))
            {
                NMLOG_WARNING("connection builder failed");
                return false;
            }

            if(reuseConnection)
            {
                GDBusProxy* proxy = m_dbus.getNetworkManagerSettingsConnectionProxy(exsistingConn.c_str());

                if(proxy != nullptr)
                {
                    result = g_dbus_proxy_call_sync (proxy, "Update",
                                g_variant_new("(a{sa{sv}})", connBuilder),
                                G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

                    if (error == nullptr) {
                        NMLOG_DEBUG("same connection updated success : %s", exsistingConn.c_str());
                        ret = true;
                    }
                    else {
                        NMLOG_ERROR("Failed to call Update : %s", error->message);
                        g_clear_error(&error);
                    }
                    if(result)
                        g_variant_unref (result);
                    g_object_unref(proxy);
                }
            }
            else
            {
                GDBusProxy *proxy = NULL;
                proxy = m_dbus.getNetworkManagerSettingsProxy();
                if (proxy != nullptr)
                {
                    result = g_dbus_proxy_call_sync (proxy,
                                                "AddConnection",
                                                g_variant_new ("(a{sa{sv}})", &connBuilder),
                                                G_DBUS_CALL_FLAGS_NONE, -1,
                                                NULL, &error);

                    if (error != nullptr) {
                        g_dbus_error_strip_remote_error (error);
                        NMLOG_ERROR ("Could not create NetworkManager/Settings proxy: %s", error->message);
                        g_error_free (error);
                        g_object_unref(proxy);
                        return false;
                    }

                    if (result) {
                        g_variant_get (result, "(&o)", &newConPath);
                        NMLOG_INFO("Added a new connection: %s", newConPath);
                        ret = true;
                        g_variant_unref (result);
                    } else {
                        g_dbus_error_strip_remote_error (error);
                        NMLOG_ERROR("Error adding connection: %s", error->message);
                        g_clear_error (&error);
                    }
                    g_object_unref(proxy);
                }
            }

            return ret;
        }

        bool NetworkManagerClient::removeKnownSSIDs(const std::string& ssid)
        {
            bool ret = false;

            if(ssid.empty())
            {
                NMLOG_ERROR("ssid name is empty");
                return false;
            }

            std::list<std::string> paths;
            if(!GnomeUtils::getConnectionPaths(m_dbus, paths))
            {
                NMLOG_ERROR("remove connection path fetch failed");
                return false;
            }

            for (const std::string& path : paths) {
                // NMLOG_DEBUG("remove connection path %s", path.c_str());
                std::string connSsid;
                if(getSSIDFromConnection(m_dbus, path, connSsid) && connSsid == ssid) {
                    ret = deleteConnection(m_dbus, path, connSsid);
                }
            }

            if(!ret)
                NMLOG_ERROR("ssid '%s' Connection remove failed", ssid.c_str());

            return ret;
        }

        bool NetworkManagerClient::getWiFiSignalStrength(string& ssid, string& signalStrength, Exchange::INetworkManager::WiFiSignalQuality& quality)
        {
            Exchange::INetworkManager::WiFiSSIDInfo ssidInfo;
            const float signalStrengthThresholdExcellent = -50.0f;
            const float signalStrengthThresholdGood = -60.0f;
            const float signalStrengthThresholdFair = -67.0f;

            if(!getConnectedSSID(ssidInfo))
            {
                NMLOG_ERROR("no wifi connected");
                return false;
            }
            else
            {
                ssid = ssidInfo.ssid;
                signalStrength = ssidInfo.strength;

	            float signalStrengthFloat = 0.0f;
                if(!signalStrength.empty())
                    signalStrengthFloat = std::stof(signalStrength.c_str());

                if (signalStrengthFloat == 0)
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED;
                else if (signalStrengthFloat >= signalStrengthThresholdExcellent && signalStrengthFloat < 0)
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_EXCELLENT;
                else if (signalStrengthFloat >= signalStrengthThresholdGood && signalStrengthFloat < signalStrengthThresholdExcellent)
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_GOOD;
                else if (signalStrengthFloat >= signalStrengthThresholdFair && signalStrengthFloat < signalStrengthThresholdGood)
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_FAIR;
                else
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_WEAK;
                NMLOG_INFO("strength success %s dBm", signalStrength.c_str());
            }

            return true;
        }

        bool NetworkManagerClient::getWifiState(Exchange::INetworkManager::WiFiState &state)
        {
            deviceInfo deviceProp;
            state = Exchange::INetworkManager::WiFiState::WIFI_STATE_UNINSTALLED;

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), deviceProp))
                return false;

            // Todo check NMDeviceStateReason for more information
            switch(deviceProp.state)
            {
                case NM_DEVICE_STATE_ACTIVATED: 
                    state = Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTED;
                    break;
                case NM_DEVICE_STATE_PREPARE:
                case NM_DEVICE_STATE_CONFIG:
                    state = Exchange::INetworkManager::WiFiState::WIFI_STATE_PAIRING;
                    break;
                case NM_DEVICE_STATE_NEED_AUTH:
                case NM_DEVICE_STATE_IP_CONFIG:
                case NM_DEVICE_STATE_IP_CHECK:
                case NM_DEVICE_STATE_SECONDARIES:
                    state = Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTING;
                    break;
                case NM_DEVICE_STATE_DEACTIVATING:
                case NM_DEVICE_STATE_FAILED:
                case NM_DEVICE_STATE_DISCONNECTED:
                    state = Exchange::INetworkManager::WiFiState::WIFI_STATE_DISCONNECTED;
                    break;
                default:
                    state = Exchange::INetworkManager::WiFiState::WIFI_STATE_DISABLED;
            }
            NMLOG_DEBUG("networkmanager wifi state (%d) mapped state (%d) ", (int)deviceProp.state, (int)state);
            return true;
        }

        bool NetworkManagerClient::wifiConnect(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo)
        {
            // TODO check sudo nmcli device wifi connect HomeNet password rafi@123
            //            Error: Connection activation failed: Device disconnected by user or client error ?
            GVariantBuilder connBuilder;
            bool ret = false, reuseConnection = false;
            deviceInfo deviceProp;
            std::string exsistingConn;

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), deviceProp))
                return false;

            if(deviceProp.path.empty() || deviceProp.state < NM_DEVICE_STATE_UNAVAILABLE)
            {
                NMLOG_ERROR("wlan0 interface not active");
                return false;
            }

            std::list<std::string> paths;
            if(GnomeUtils::getWifiConnectionPaths(m_dbus, deviceProp.path.c_str(), paths))
            {
                for (const std::string& path : paths) {
                    std::string ssid;
                    if(getSSIDFromConnection(m_dbus, path, ssid) && ssid == ssidinfo.ssid)
                    {
                        exsistingConn = path;
                        NMLOG_WARNING("same connection exsist (%s)", exsistingConn.c_str());
                        reuseConnection = true;
                        break; // only one connection will be activate (same property only)
                    }
                }
            }

            if(reuseConnection)
            {
                NMLOG_INFO("activating connection...");
                ret = gVariantConnectionBuilder(ssidinfo, connBuilder);
                if(!ret) {
                    NMLOG_WARNING("connection builder failed");
                    return false;
                }

                if(updateConnctionAndactivate(m_dbus, connBuilder, deviceProp.path.c_str(), exsistingConn.c_str()))
                    NMLOG_INFO("updated connection request success");
                else
                    NMLOG_ERROR("wifi connect request failed");
            }
            else 
            {
                ret = gVariantConnectionBuilder(ssidinfo, connBuilder);
                if(!ret) {
                    NMLOG_WARNING("connection builder failed");
                    return false;
                }
                if(addNewConnctionAndactivate(m_dbus, connBuilder, deviceProp.path.c_str(), ssidinfo.persist))
                    NMLOG_INFO("wifi connect request success");
                else
                    NMLOG_ERROR("wifi connect request failed");
            }

            return true;
        }

        bool NetworkManagerClient::wifiDisconnect()
        {
            GError* error = NULL;
            GDBusProxy* wProxy = NULL;
            deviceInfo devInfo;

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), devInfo) || devInfo.path.empty())
                return false;

            if(devInfo.state <= NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_WARNING("wifi in disconnected state");
                return true;
            }

            wProxy = m_dbus.getNetworkManagerDeviceProxy(devInfo.path.c_str());
            if(wProxy == NULL)
                return false;

            g_dbus_proxy_call_sync(wProxy, "Disconnect", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
            if (error) {
                NMLOG_ERROR("Error calling Disconnect method: %s", error->message);
                g_error_free(error);
                g_object_unref(wProxy);
                return false;
            }
            else
               NMLOG_INFO("wifi disconnected success"); 

            return true;
        }

        bool NetworkManagerClient::startWPS()
        {
            return false;
        }

        bool NetworkManagerClient::stopWPS()
        {
            return false;
        }

    } // WPEFramework
} // Plugin

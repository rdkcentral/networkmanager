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
#include <list>
#include <uuid/uuid.h>
#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include "NetworkManagerLogger.h"
#include "NetworkManagerGdbusClient.h"
#include "NetworkManagerGdbusUtils.h"
#include "../NetworkManagerGnomeUtils.h"

namespace WPEFramework
{
    namespace Plugin
    {
        extern NetworkManagerImplementation* _instance;
        NetworkManagerClient::NetworkManagerClient() {
            NMLOG_INFO("NetworkManagerClient");
            m_wpsProcessRun = false;
            m_wpsActionTriggered = false;
        }

        NetworkManagerClient::~NetworkManagerClient() {
            NMLOG_INFO("~NetworkManagerClient");
        }

        bool updateRouteMetric(DbusMgr& m_dbus, const std::string& connectionPath, gint64 route_metric, const gchar* interface, const std::string& activeConnectionPath)
        {
            GError *error = nullptr;
            deviceInfo devInfo{};
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, interface, devInfo))
                return false;
            GDBusProxy *settingsProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(connectionPath.c_str());

            if (settingsProxy == nullptr) {
                NMLOG_ERROR("Error creating connection settings proxy");
                return false;
            }

            GVariant *connectionSettings = g_dbus_proxy_call_sync(
                    settingsProxy,
                    "GetSettings",
                    nullptr,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (connectionSettings == nullptr) {
                NMLOG_ERROR("Error retrieving connection settings: %s", error->message);
                g_error_free(error);
                g_object_unref(settingsProxy);
                return false;
            }

            GVariantIter *iterator;
            GVariant *settingsDict;
            const gchar *settingsKey;
            const gchar *existingId = nullptr;
            const gchar *existingType = nullptr;
            const gchar *existingIPv4Method = nullptr;
            const gchar *existingIPv6Method = nullptr;
            const gchar *existingInterfaceName = nullptr;

            const gchar *existingKeyMgmt = nullptr;
            const gchar *existingPSK = nullptr;
            const gchar *existingSSID = nullptr;
            GVariant *existingIPv4Settings = nullptr;
            GVariant *existingIPv6Settings = nullptr;

            g_variant_get(connectionSettings, "(a{sa{sv}})", &iterator);
            while (g_variant_iter_loop(iterator, "{&s@a{sv}}", &settingsKey, &settingsDict)) {
                GVariantIter settingsIter;
                const gchar *key;
                GVariant *value;

                g_variant_iter_init(&settingsIter, settingsDict);
                while (g_variant_iter_loop(&settingsIter, "{&sv}", &key, &value)) {
                    if (g_strcmp0(key, "id") == 0) {
                        existingId = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("Connection ID: %s\n", existingId);
                    } else if (g_strcmp0(key, "type") == 0) {
                        existingType = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("Connection Type: %s\n", existingType);
                    } else if (g_strcmp0(key, "interface-name") == 0) {
                        existingInterfaceName = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("Interface Name: %s\n", existingInterfaceName);
                    } else if (g_strcmp0(key, "method") == 0) {
                        if(g_strcmp0(settingsKey, "ipv4") == 0)
                        {
                            existingIPv4Method = g_variant_get_string(value, NULL);
                            NMLOG_DEBUG("IPV4 Method: %s\n", existingIPv4Method);
                        }
                        else if(g_strcmp0(settingsKey, "ipv6") == 0)
                        {
                            existingIPv6Method = g_variant_get_string(value, NULL);
                            NMLOG_DEBUG("IPV6 Method: %s\n", existingIPv6Method);
                        }
                    } else if (g_strcmp0(key, "ssid") == 0) {
                        gsize size;
                        const guint8 *ssid = (const guint8 *) g_variant_get_fixed_array(value, &size, sizeof(guint8));
                        // Convert the ssid to a null-terminated string
                        existingSSID = g_strndup((const gchar *)ssid, size);
                    } else if (g_strcmp0(key, "key-mgmt") == 0) {
                        existingKeyMgmt = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("Key Management: %s\n", existingKeyMgmt);
                    } else if (g_strcmp0(key, "psk") == 0) {
                        existingPSK = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("psk: %s\n", existingPSK);
                    } else if (g_strcmp0(settingsKey, "ipv4") == 0) {
                        existingIPv4Settings = g_variant_ref(settingsDict);
                    } else if (g_strcmp0(settingsKey, "ipv6") == 0) {
                        existingIPv6Settings = g_variant_ref(settingsDict);
                    }

                }
            }
            g_variant_iter_free(iterator);

            GVariantBuilder connectionBuilder;

            GVariantBuilder wifiBuilder;
            GVariantBuilder settingsBuilder;
            GVariantBuilder wifiSecurityBuilder;
            g_variant_builder_init(&settingsBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
            // Define the 'connection' dictionary with connection details
            g_variant_builder_init(&connectionBuilder, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&connectionBuilder, "{sv}", "id", g_variant_new_string(existingId));
            g_variant_builder_add(&connectionBuilder, "{sv}", "type", g_variant_new_string(existingType));
            g_variant_builder_add(&connectionBuilder, "{sv}", "interface-name", g_variant_new_string(existingInterfaceName));
            g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "connection", &connectionBuilder);

            if (g_strcmp0(interface, GnomeUtils::getWifiIfname()) == 0) {
                // Define the '802-11-wireless' dictionary with Wi-Fi specific details
                g_variant_builder_init(&wifiBuilder, G_VARIANT_TYPE("a{sv}"));
                GVariantBuilder ssidBuilder;
                g_variant_builder_init(&ssidBuilder, G_VARIANT_TYPE("ay"));
                while (*existingSSID) {
                    g_variant_builder_add(&ssidBuilder, "y", *(existingSSID++));
                }
                g_variant_builder_add(&wifiBuilder, "{sv}", "ssid", g_variant_builder_end(&ssidBuilder));
                g_variant_builder_add(&wifiBuilder, "{sv}", "mode", g_variant_new_string("infrastructure")); // Set Wi-Fi mode
                g_variant_builder_add(&wifiBuilder, "{sv}", "security", g_variant_new_string("802-11-wireless-security")); // Security type

                g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "802-11-wireless", &wifiBuilder);

                // Define the '802-11-wireless-security' dictionary with security details
                g_variant_builder_init(&wifiSecurityBuilder, G_VARIANT_TYPE("a{sv}"));
                g_variant_builder_add(&wifiSecurityBuilder, "{sv}", "key-mgmt", g_variant_new_string(existingKeyMgmt)); // Key management
                g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "802-11-wireless-security", &wifiSecurityBuilder);
            }


            GVariantBuilder ipv4Builder;
            g_variant_builder_init(&ipv4Builder, G_VARIANT_TYPE("a{sv}"));
            GVariantBuilder ipv6Builder;
            g_variant_builder_init(&ipv6Builder, G_VARIANT_TYPE("a{sv}"));

            GnomeUtils::addGvariantToBuilder(existingIPv4Settings, &ipv4Builder, true);
            GnomeUtils::addGvariantToBuilder(existingIPv6Settings, &ipv6Builder, true);
            /*g_variant_builder_add(&ipv4Builder, "{sv}", "method", g_variant_new_string(existingIPv4Method));
            g_variant_builder_add(&ipv6Builder, "{sv}", "method", g_variant_new_string(existingIPv6Method));*/
            g_variant_builder_add(&ipv4Builder, "{sv}", "route-metric", g_variant_new_int64(route_metric));
            g_variant_builder_add(&ipv6Builder, "{sv}", "route-metric", g_variant_new_int64(route_metric));

            g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "ipv4", &ipv4Builder);
            g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "ipv6", &ipv6Builder);

            g_dbus_proxy_call_sync(
                    settingsProxy,
                    "Update",
                    g_variant_new("(a{sa{sv}})", &settingsBuilder),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (error) {
                NMLOG_ERROR("Error updating connection settings: %s", error->message);
                g_error_free(error);
            } else {
                NMLOG_DEBUG("Successfully updated IPv4 settings for %s interface", interface);
            }
            if(!GnomeUtils::activateConnection(m_dbus, connectionPath, devInfo.path))
            {
                NMLOG_INFO("activateConnection not successful");
                return false;
            }
            else
                NMLOG_INFO("activateConnection successful");

            g_variant_unref(connectionSettings);
            g_object_unref(settingsProxy);
            return true;
        }

        bool updateIPSettings(DbusMgr& m_dbus, const std::string& connectionPath, const Exchange::INetworkManager::IPAddress& address, const std::string& interface)
        {
            GError *error = nullptr;
            deviceInfo devInfo{};
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, interface.c_str(), devInfo))
                return false;
            GDBusProxy *settingsProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(connectionPath.c_str());

            if (settingsProxy == nullptr) {
                NMLOG_ERROR("Error creating connection settings proxy");
                return false;
            }

            GVariant *connectionSettings = g_dbus_proxy_call_sync(
                    settingsProxy,
                    "GetSettings",
                    nullptr,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (connectionSettings == nullptr) {
                NMLOG_ERROR("Error retrieving connection settings: %s", error->message);
                g_error_free(error);
                g_object_unref(settingsProxy);
                return false;
            }

            GVariantIter *iterator;
            GVariant *settingsDict;
            const gchar *settingsKey;
            const gchar *existingId = nullptr;
            const gchar *existingType = nullptr;
            const gchar *existingInterfaceName = nullptr;

            const gchar *existingKeyMgmt = nullptr;
            const gchar *existingPSK = nullptr;
            const gchar *existingSSID = nullptr;
            GVariant *existingIPv4Settings = nullptr;
            GVariant *existingIPv6Settings = nullptr;


            g_variant_get(connectionSettings, "(a{sa{sv}})", &iterator);
            while (g_variant_iter_loop(iterator, "{&s@a{sv}}", &settingsKey, &settingsDict)) {
                GVariantIter settingsIter;
                const gchar *key;
                GVariant *value;

                g_variant_iter_init(&settingsIter, settingsDict);
                while (g_variant_iter_loop(&settingsIter, "{&sv}", &key, &value)) {
                    if (g_strcmp0(key, "id") == 0) {
                        existingId = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("Connection ID: %s\n", existingId);
                    } else if (g_strcmp0(key, "type") == 0) {
                        existingType = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("Connection Type: %s\n", existingType);
                    } else if (g_strcmp0(key, "interface-name") == 0) {
                        existingInterfaceName = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("Interface Name: %s\n", existingInterfaceName);
                    } else if (g_strcmp0(key, "ssid") == 0) {
                        gsize size;
                        const guint8 *ssid = (const guint8 *) g_variant_get_fixed_array(value, &size, sizeof(guint8));
                        // Convert the ssid to a null-terminated string
                        existingSSID = g_strndup((const gchar *)ssid, size);
                    } else if (g_strcmp0(key, "key-mgmt") == 0) {
                        existingKeyMgmt = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("Key Management: %s\n", existingKeyMgmt);
                    } else if (g_strcmp0(key, "psk") == 0) {
                        existingPSK = g_variant_get_string(value, NULL);
                        NMLOG_DEBUG("psk: %s\n", existingPSK);
                    } else if (g_strcmp0(settingsKey, "ipv4") == 0) {
                        existingIPv4Settings = g_variant_ref(settingsDict);
                    } else if (g_strcmp0(settingsKey, "ipv6") == 0) {
                        existingIPv6Settings = g_variant_ref(settingsDict);
                    }
                }
            }
            g_variant_iter_free(iterator);

            GVariantBuilder connectionBuilder;

            GVariantBuilder wifiBuilder;
            GVariantBuilder settingsBuilder;
            GVariantBuilder wifiSecurityBuilder;
            GVariantBuilder addressesBuilder;
            GVariantBuilder addressEntryBuilder;
            GVariantBuilder dnsBuilder;
            g_variant_builder_init(&settingsBuilder, G_VARIANT_TYPE("a{sa{sv}}"));
            // Define the 'connection' dictionary with connection details
            g_variant_builder_init(&connectionBuilder, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&connectionBuilder, "{sv}", "id", g_variant_new_string(existingId));
            g_variant_builder_add(&connectionBuilder, "{sv}", "type", g_variant_new_string(existingType));
            g_variant_builder_add(&connectionBuilder, "{sv}", "interface-name", g_variant_new_string(existingInterfaceName));
            g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "connection", &connectionBuilder);

            if (g_strcmp0(interface.c_str(), GnomeUtils::getWifiIfname()) == 0) {
                // Define the '802-11-wireless' dictionary with Wi-Fi specific details
                g_variant_builder_init(&wifiBuilder, G_VARIANT_TYPE("a{sv}"));
                GVariantBuilder ssidBuilder;
                g_variant_builder_init(&ssidBuilder, G_VARIANT_TYPE("ay"));
                while (*existingSSID) {
                    g_variant_builder_add(&ssidBuilder, "y", *(existingSSID++));
                }
                g_variant_builder_add(&wifiBuilder, "{sv}", "ssid", g_variant_builder_end(&ssidBuilder));
                g_variant_builder_add(&wifiBuilder, "{sv}", "mode", g_variant_new_string("infrastructure")); // Set Wi-Fi mode
                g_variant_builder_add(&wifiBuilder, "{sv}", "security", g_variant_new_string("802-11-wireless-security")); // Security type

                g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "802-11-wireless", &wifiBuilder);

                // Define the '802-11-wireless-security' dictionary with security details
                g_variant_builder_init(&wifiSecurityBuilder, G_VARIANT_TYPE("a{sv}"));
                g_variant_builder_add(&wifiSecurityBuilder, "{sv}", "key-mgmt", g_variant_new_string(existingKeyMgmt)); // Key management
                g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "802-11-wireless-security", &wifiSecurityBuilder);
            }

            GVariantBuilder ipv4Builder;
            GVariantBuilder ipv6Builder;

            g_variant_builder_init(&ipv4Builder, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_init(&ipv6Builder, G_VARIANT_TYPE("a{sv}"));
            if (g_strcmp0(address.ipversion.c_str(), "IPv4") == 0)
            {
                g_variant_builder_add(&ipv4Builder, "{sv}", "method", g_variant_new_string(address.autoconfig ? "auto" : "manual"));
                if(!address.autoconfig)
                {
                    // addresses
                    g_variant_builder_init(&addressesBuilder, G_VARIANT_TYPE("aau"));

                    g_variant_builder_init(&addressEntryBuilder, G_VARIANT_TYPE("au"));

                    g_variant_builder_add(&addressEntryBuilder, "u", GnomeUtils::ip4StrToNBO(address.ipaddress));
                    g_variant_builder_add(&addressEntryBuilder, "u", address.prefix);
                    g_variant_builder_add(&addressEntryBuilder, "u", GnomeUtils::ip4StrToNBO(address.gateway));
                    g_variant_builder_add_value(&addressesBuilder, g_variant_builder_end(&addressEntryBuilder));

                    g_variant_builder_add(&ipv4Builder, "{sv}", "addresses", g_variant_builder_end(&addressesBuilder));

                    // dns
                    g_variant_builder_init(&dnsBuilder, G_VARIANT_TYPE("au"));
                    g_variant_builder_add(&dnsBuilder, "u", GnomeUtils::ip4StrToNBO(address.primarydns));
                    g_variant_builder_add(&dnsBuilder, "u", GnomeUtils::ip4StrToNBO(address.secondarydns));

                    g_variant_builder_add(&ipv4Builder, "{sv}", "dns", g_variant_builder_end(&dnsBuilder));
                    // Add gateway
                    g_variant_builder_add(&ipv4Builder, "{sv}", "gateway", g_variant_new_string(address.gateway.c_str()));
                    g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "ipv4", &ipv4Builder);
                }
                if (existingIPv6Settings) {
                    GnomeUtils::addGvariantToBuilder(existingIPv6Settings, &ipv6Builder, false);
                    g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "ipv6", &ipv6Builder);
                    g_variant_unref(existingIPv6Settings);
                }
            }
            else if (g_strcmp0(address.ipversion.c_str(), "IPv6") == 0)
            {
                g_variant_builder_add(&ipv6Builder, "{sv}", "method", g_variant_new_string(address.autoconfig ? "auto" : "manual"));
                if(!address.autoconfig)
                {
                    // addresses
                    g_variant_builder_init(&addressesBuilder, G_VARIANT_TYPE("a(ayuay)"));

                    g_variant_builder_init(&addressEntryBuilder, G_VARIANT_TYPE("(ayuay)"));
                    std::array<guint8, 16> ip6 = GnomeUtils::ip6StrToNBO(address.ipaddress);
                    std::array<guint8, 16> gateway6 = GnomeUtils::ip6StrToNBO(address.gateway);
                    g_variant_builder_add_value(&addressEntryBuilder, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, ip6.data(), ip6.size(), sizeof(guint8)));
                    g_variant_builder_add(&addressEntryBuilder, "u", address.prefix);
                    g_variant_builder_add_value(&addressEntryBuilder, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, gateway6.data(), gateway6.size(), sizeof(guint8)));
                    g_variant_builder_add_value(&addressesBuilder, g_variant_builder_end(&addressEntryBuilder));
                    g_variant_builder_add(&ipv6Builder, "{sv}", "addresses", g_variant_builder_end(&addressesBuilder));

                    //DNS
                    g_variant_builder_init(&dnsBuilder, G_VARIANT_TYPE("aay"));
                    std::array<guint8, 16> primaryDns6 = GnomeUtils::ip6StrToNBO(address.primarydns);
                    std::array<guint8, 16> secondaryDns6 = GnomeUtils::ip6StrToNBO(address.secondarydns);
                    g_variant_builder_add_value(&dnsBuilder, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, primaryDns6.data(), primaryDns6.size(), sizeof(guint8)));
                    g_variant_builder_add_value(&dnsBuilder, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, secondaryDns6.data(), secondaryDns6.size(), sizeof(guint8)));
                    g_variant_builder_add(&ipv6Builder, "{sv}", "dns", g_variant_builder_end(&dnsBuilder));
                    // Add gateway
                    g_variant_builder_add(&ipv6Builder, "{sv}", "gateway", g_variant_new_string(address.gateway.c_str()));

                    g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "ipv6", &ipv6Builder);
                }
                if (existingIPv4Settings) {
                    GnomeUtils::addGvariantToBuilder(existingIPv4Settings, &ipv4Builder, false);
                    g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "ipv4", &ipv4Builder);
                    g_variant_unref(existingIPv4Settings);
                }
            }

            g_dbus_proxy_call_sync(
                    settingsProxy,
                    "Update",
                    g_variant_new("(a{sa{sv}})", &settingsBuilder),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (error) {
                NMLOG_ERROR("Error updating connection settings: %s", error->message);
                g_error_free(error);
            } else {
                NMLOG_DEBUG("Successfully updated IPv4 settings for %s interface", interface.c_str());
            }
            if(!GnomeUtils::activateConnection(m_dbus, connectionPath, devInfo.path))
            {
                NMLOG_INFO("activateConnection not successful");
                return false;
            }
            else
                NMLOG_INFO("activateConnection successful");

            g_variant_unref(connectionSettings);
            g_object_unref(settingsProxy);
            return true;
        }

        bool updateHostnameSettings(DbusMgr& m_dbus, const std::string& connectionPath, const std::string& hostname, const std::string& interface)
        {
            GError *error = nullptr;
            deviceInfo devInfo{};
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, interface.c_str(), devInfo))
                return false;

            GDBusProxy *settingsProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(connectionPath.c_str());
            if (settingsProxy == nullptr) {
                NMLOG_ERROR("Error creating connection settings proxy");
                return false;
            }

            GVariant *connectionSettings = g_dbus_proxy_call_sync(
                    settingsProxy,
                    "GetSettings",
                    nullptr,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (connectionSettings == nullptr) {
                NMLOG_ERROR("Error retrieving connection settings: %s", error->message);
                g_error_free(error);
                g_object_unref(settingsProxy);
                return false;
            }

            GVariantIter *iterator;
            GVariant *settingsDict;
            const gchar *settingsKey;
            const gchar *existingId = nullptr;
            const gchar *existingType = nullptr;
            const gchar *existingInterfaceName = nullptr;
            const gchar *existingKeyMgmt = nullptr;
            const gchar *existingSSID = nullptr;
            GVariant *existingIPv4Settings = nullptr;
            GVariant *existingIPv6Settings = nullptr;

            g_variant_get(connectionSettings, "(a{sa{sv}})", &iterator);
            while (g_variant_iter_loop(iterator, "{&s@a{sv}}", &settingsKey, &settingsDict)) {
                GVariantIter settingsIter;
                const gchar *key;
                GVariant *value;

                g_variant_iter_init(&settingsIter, settingsDict);
                while (g_variant_iter_loop(&settingsIter, "{&sv}", &key, &value)) {
                    if (g_strcmp0(key, "id") == 0) {
                        existingId = g_variant_get_string(value, NULL);
                    } else if (g_strcmp0(key, "type") == 0) {
                        existingType = g_variant_get_string(value, NULL);
                    } else if (g_strcmp0(key, "interface-name") == 0) {
                        existingInterfaceName = g_variant_get_string(value, NULL);
                    } else if (g_strcmp0(key, "ssid") == 0) {
                        gsize size;
                        const guint8 *ssid = (const guint8 *) g_variant_get_fixed_array(value, &size, sizeof(guint8));
                        existingSSID = g_strndup((const gchar *)ssid, size);
                    } else if (g_strcmp0(key, "key-mgmt") == 0) {
                        existingKeyMgmt = g_variant_get_string(value, NULL);
                    } else if (g_strcmp0(settingsKey, "ipv4") == 0) {
                        existingIPv4Settings = g_variant_ref(settingsDict);
                    } else if (g_strcmp0(settingsKey, "ipv6") == 0) {
                        existingIPv6Settings = g_variant_ref(settingsDict);
                    }
                }
            }
            g_variant_iter_free(iterator);

            GVariantBuilder connectionBuilder;
            GVariantBuilder wifiBuilder;
            GVariantBuilder settingsBuilder;
            GVariantBuilder wifiSecurityBuilder;
            g_variant_builder_init(&settingsBuilder, G_VARIANT_TYPE("a{sa{sv}}"));

            // Define the 'connection' dictionary with connection details
            g_variant_builder_init(&connectionBuilder, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&connectionBuilder, "{sv}", "id", g_variant_new_string(existingId));
            g_variant_builder_add(&connectionBuilder, "{sv}", "type", g_variant_new_string(existingType));
            g_variant_builder_add(&connectionBuilder, "{sv}", "interface-name", g_variant_new_string(existingInterfaceName));
            g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "connection", &connectionBuilder);

            if (g_strcmp0(interface.c_str(), GnomeUtils::getWifiIfname()) == 0) {
                // Define the '802-11-wireless' dictionary with Wi-Fi specific details
                g_variant_builder_init(&wifiBuilder, G_VARIANT_TYPE("a{sv}"));
                GVariantBuilder ssidBuilder;
                g_variant_builder_init(&ssidBuilder, G_VARIANT_TYPE("ay"));
                while (*existingSSID) {
                    g_variant_builder_add(&ssidBuilder, "y", *(existingSSID++));
                }
                g_variant_builder_add(&wifiBuilder, "{sv}", "ssid", g_variant_builder_end(&ssidBuilder));
                g_variant_builder_add(&wifiBuilder, "{sv}", "mode", g_variant_new_string("infrastructure"));
                g_variant_builder_add(&wifiBuilder, "{sv}", "security", g_variant_new_string("802-11-wireless-security"));
                g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "802-11-wireless", &wifiBuilder);

                // Define the '802-11-wireless-security' dictionary with security details
                g_variant_builder_init(&wifiSecurityBuilder, G_VARIANT_TYPE("a{sv}"));
                g_variant_builder_add(&wifiSecurityBuilder, "{sv}", "key-mgmt", g_variant_new_string(existingKeyMgmt));
                g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "802-11-wireless-security", &wifiSecurityBuilder);
            }

            GVariantBuilder ipv4Builder;
            g_variant_builder_init(&ipv4Builder, G_VARIANT_TYPE("a{sv}"));
            GVariantBuilder ipv6Builder;
            g_variant_builder_init(&ipv6Builder, G_VARIANT_TYPE("a{sv}"));

            // Add existing IPv4 settings and set hostname
            if (existingIPv4Settings)
            {
                GVariantIter iter;
                const gchar *key;
                GVariant *value;
                g_variant_iter_init(&iter, existingIPv4Settings);
                while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
                    // Skip existing hostname properties to avoid duplicates
                    if (g_strcmp0(key, "dhcp-hostname") != 0 && g_strcmp0(key, "dhcp-send-hostname") != 0) {
                        g_variant_builder_add(&ipv4Builder, "{sv}", key, value);
                    }
                }
            }
            // Now add the new hostname settings
            g_variant_builder_add(&ipv4Builder, "{sv}", "dhcp-hostname", g_variant_new_string(hostname.c_str()));
            g_variant_builder_add(&ipv4Builder, "{sv}", "dhcp-send-hostname", g_variant_new_boolean(TRUE));
            g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "ipv4", &ipv4Builder);

            // Add existing IPv6 settings, excluding hostname-related properties
            if (existingIPv6Settings) {
                GVariantIter iter;
                const gchar *key;
                GVariant *value;
                g_variant_iter_init(&iter, existingIPv6Settings);
                while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
                    // Skip existing hostname properties to avoid duplicates
                    if (g_strcmp0(key, "dhcp-hostname") != 0 && g_strcmp0(key, "dhcp-send-hostname") != 0) {
                        g_variant_builder_add(&ipv6Builder, "{sv}", key, value);
                    }
                }
            }
            // Now add the new hostname settings
            g_variant_builder_add(&ipv6Builder, "{sv}", "dhcp-hostname", g_variant_new_string(hostname.c_str()));
            g_variant_builder_add(&ipv6Builder, "{sv}", "dhcp-send-hostname", g_variant_new_boolean(TRUE));
            g_variant_builder_add(&settingsBuilder, "{sa{sv}}", "ipv6", &ipv6Builder);

            g_dbus_proxy_call_sync(
                    settingsProxy,
                    "Update",
                    g_variant_new("(a{sa{sv}})", &settingsBuilder),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (error) {
                NMLOG_ERROR("Error updating connection settings with hostname: %s", error->message);
                g_error_free(error);
                g_variant_unref(connectionSettings);
                g_object_unref(settingsProxy);
                return false;
            }

            NMLOG_DEBUG("Successfully updated hostname settings for %s interface to: %s", interface.c_str(), hostname.c_str());

            // Activate connection to apply changes
            if(!GnomeUtils::activateConnection(m_dbus, connectionPath, devInfo.path))
            {
                NMLOG_INFO("activateConnection not successful");
            }
            else
                NMLOG_INFO("activateConnection successful");

            if (existingIPv4Settings) g_variant_unref(existingIPv4Settings);
            if (existingIPv6Settings) g_variant_unref(existingIPv6Settings);
            g_variant_unref(connectionSettings);
            g_object_unref(settingsProxy);
            return true;
        }

        bool NetworkManagerClient::getAvailableInterfaces(std::vector<Exchange::INetworkManager::InterfaceDetails>& interfacesList)
        {
            deviceInfo ethDevInfo{};
            deviceInfo wifiDevInfo{};
            Exchange::INetworkManager::InterfaceDetails ethInterface;
            Exchange::INetworkManager::InterfaceDetails wifiInterface;
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getEthIfname(), ethDevInfo))
                return false;
            ethInterface.type = Exchange::INetworkManager::InterfaceType::INTERFACE_TYPE_ETHERNET;
            ethInterface.name = ethDevInfo.interface;
            ethInterface.mac = ethDevInfo.MAC;
            ethInterface.enabled = ethDevInfo.managed?true:false;
            ethInterface.connected = ethDevInfo.state == NM_DEVICE_STATE_ACTIVATED?true:false;
            interfacesList.push_back(ethInterface);
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), wifiDevInfo))
                return false;
            wifiInterface.type = Exchange::INetworkManager::InterfaceType::INTERFACE_TYPE_WIFI;
            wifiInterface.name = wifiDevInfo.interface;
            wifiInterface.mac = wifiDevInfo.MAC;
            wifiInterface.enabled = wifiDevInfo.managed?true:false;
            wifiInterface.connected = wifiDevInfo.state == NM_DEVICE_STATE_ACTIVATED?true:false;
            interfacesList.push_back(wifiInterface);

            return true;
        }

        bool NetworkManagerClient::setPrimaryInterface(const std::string& interface)
        {
            GError *error = nullptr;
            GDBusProxy *nmProxy = nullptr;
            std::string connectionPath;
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
            gint64 routeMetric;
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
                        NMLOG_ERROR("Error creating device proxy: %s", error->message);
                        g_error_free(error);
                        continue;
                    }

                    GVariant *ifaceProperty = g_dbus_proxy_get_cached_property(deviceProxy, "Interface");
                    if (ifaceProperty) {
                        const gchar *iface = g_variant_get_string(ifaceProperty, nullptr);
                        GVariant *connectionProperty = g_dbus_proxy_get_cached_property(activeConnectionProxy, "Connection");
                        if (connectionProperty) {
                            connectionPath = g_variant_get_string(connectionProperty, nullptr);
                            g_variant_unref(connectionProperty);
                        } else {
                            NMLOG_ERROR("Error retrieving connection property");
                        }
                        if (!g_strcmp0(iface, GnomeUtils::getWifiIfname()) || !g_strcmp0(iface, GnomeUtils::getEthIfname())) {
                            if(interface == iface)
                                routeMetric = ROUTE_METRIC_PRIORITY_HIGH;
                            else
                                routeMetric = ROUTE_METRIC_PRIORITY_LOW;
                            if(!updateRouteMetric(m_dbus, connectionPath, routeMetric, iface, activeConnectionPath))
                            {
                                NMLOG_ERROR("Error: Failed to update route metric for Interface %s", iface);
                                return false;
                            }
                        }
                        g_variant_unref(ifaceProperty);
                        g_object_unref(deviceProxy);
                        break;
                    }
                    g_variant_unref(ifaceProperty);
                }

                g_variant_unref(devicesVar);
                g_object_unref(activeConnectionProxy);
            }

            g_variant_unref(activeConnections);
            g_object_unref(nmProxy);

            return true;
        }

        bool NetworkManagerClient::getDefaultInterface(std::string& interface)
        {
            deviceInfo devInfo{};
            std::string wifiname = GnomeUtils::getWifiIfname(), ethname = GnomeUtils::getEthIfname();
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, ethname.c_str(), devInfo))
                return false;
            if(devInfo.state > NM_DEVICE_STATE_DISCONNECTED && devInfo.state < NM_DEVICE_STATE_DEACTIVATING)
                interface = ethname;
            else
                interface = wifiname; // default is wifi
            return true;
        }

        bool NetworkManagerClient::getPrimaryInterface(std::string& interface)
        {
            GError* error = nullptr;
            std::string primaryConnectionPath;
            GDBusProxy *nmProxy = nullptr;

            nmProxy = m_dbus.getNetworkManagerPropertyProxy("/org/freedesktop/NetworkManager");
            if(nmProxy == nullptr)
                return false;

            GVariant* result = g_dbus_proxy_call_sync(
                    nmProxy,
                    "Get",
                    g_variant_new("(ss)", "org.freedesktop.NetworkManager", "PrimaryConnection"),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error
                    );

            if (error) {
                NMLOG_ERROR("Error: Getting primary connection path %s",error->message);
                g_error_free(error);
                return false;
            }

            if (result != NULL) {
                GVariant* value;
                g_variant_get(result, "(v)", &value);
                if (g_variant_is_of_type(value, G_VARIANT_TYPE_OBJECT_PATH)) {
                    primaryConnectionPath = g_variant_get_string(value, nullptr);
                } else {
                    NMLOG_ERROR("Expected object path but got different type");
                }
                g_variant_unref(value);
                g_variant_unref(result);
            }

            GDBusProxy* deviceProxy = m_dbus.getNetworkManagerPropertyProxy(primaryConnectionPath.c_str());
            if(deviceProxy == NULL)
                return false;

            std::string defaultDevicePath;

            result = g_dbus_proxy_call_sync(
                    deviceProxy,
                    "Get",
                    g_variant_new("(ss)", "org.freedesktop.NetworkManager.Connection.Active", "Devices"),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error
            );

            if (error) {
                NMLOG_ERROR("Error: Getting default device path %s", error->message);
                g_error_free(error);
                return false;
            }

            if (result != nullptr) {
                GVariant* value;
                g_variant_get(result, "(v)", &value);
                if (g_variant_is_of_type(value, G_VARIANT_TYPE_ARRAY)) {
                    GVariantIter* iter;
                    const gchar* devicePath;
                    g_variant_get(value, "ao", &iter);

                    if (g_variant_iter_loop(iter, "o", &devicePath)) {
                        defaultDevicePath = devicePath;
                    }
                    g_variant_iter_free(iter);
                } else {
                    NMLOG_ERROR("Expected array of object paths but got different type");
                }
                g_variant_unref(value);
                g_variant_unref(result);
            }
            GDBusProxy* defaultDeviceProxy = m_dbus.getNetworkManagerPropertyProxy(defaultDevicePath.c_str());
            if(defaultDeviceProxy == NULL)
                return false;

            error = nullptr;
            result = g_dbus_proxy_call_sync(
                    defaultDeviceProxy,
                    "Get",
                    g_variant_new("(ss)", "org.freedesktop.NetworkManager.Device", "Interface"),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error
                    );

            if (error) {
                NMLOG_ERROR("Error: Getting primary interface %s", error->message);
                g_error_free(error);
                return false;
            }

            if (result != nullptr) {
                GVariant* value;
                g_variant_get(result, "(v)", &value);
                if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
                    interface = g_variant_get_string(value, nullptr);
                } else {
                    NMLOG_ERROR("Expected string but got different type");
                }
                g_variant_unref(value);
                g_variant_unref(result);
            }

            g_object_unref(defaultDeviceProxy);
            g_object_unref(deviceProxy);
            g_object_unref(nmProxy);
            return true;
        }

        bool NetworkManagerClient::setInterfaceState(const std::string& interface, bool enable)
        {
            deviceInfo devInfo{};
            GError* error = nullptr;

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, interface.c_str(), devInfo))
                return false;

            GDBusProxy* deviceProxy = m_dbus.getNetworkManagerPropertyProxy(devInfo.path.c_str());
            if(deviceProxy == NULL)
                return false;
            NMLOG_INFO("interface %s state: %s", interface.c_str(), enable ? "enabled" : "disabled");

            NMDeviceState deviceState = devInfo.state;

            if (enable) {
                NMLOG_DEBUG("Enabling interface...");
                if (deviceState >= NM_DEVICE_STATE_UNAVAILABLE && devInfo.managed) // already enabled and managed
                {
                    return true;
                }
            } else {
                NMLOG_DEBUG("Disabling interface...");
                if (deviceState < NM_DEVICE_STATE_UNAVAILABLE && !devInfo.managed) // already disabled and unmanaged
                {
                    return true;
                }
                else if (deviceState > NM_DEVICE_STATE_DISCONNECTED) {
                    NMLOG_DEBUG("Disconnecting device...");
                    // Disconnect the device before setting it to unmanaged.
                    // This ensures that NetworkManager cleanly removes any IP addresses, routes,
                    // and DNS configuration associated with the interface. Setting an interface
                    // to unmanaged without disconnecting first may leave residual configuration
                    // that can cause networking issues.
                    GDBusProxy* deviceProxy = m_dbus.getNetworkManagerDeviceProxy(devInfo.path.c_str());
                    if(deviceProxy != NULL) {
                        GVariant* disconnectResult = g_dbus_proxy_call_sync(
                            deviceProxy,
                            "Disconnect",
                            nullptr,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            nullptr,
                            &error
                        );

                        if (error) {
                            NMLOG_WARNING("Error disconnecting device: %s", error->message);
                            g_error_free(error);
                            error = nullptr;
                        } else if (disconnectResult) {
                            g_variant_unref(disconnectResult);
                        }
                        g_object_unref(deviceProxy);
                    }

                    // Wait until device is truly disconnected
                    int retry = 24; // 12 seconds
                    NMDeviceState oldDevState = NM_DEVICE_STATE_UNKNOWN;
                    while (retry-- > 0) {
                        /* Force glib event processing to update state
                         * This below line will create an uncertain time wait. We are taking a fixed time interval of 12 seconds.
                         */
                        // while (g_main_context_iteration(NULL, FALSE));

                        if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, interface.c_str(), devInfo)) {
                            break;
                        }
                        deviceState = devInfo.state;
                        if(oldDevState != deviceState)
                        {
                            oldDevState = deviceState;
                            NMLOG_WARNING("Device state: %d", deviceState);
                        }

                        if (deviceState <= NM_DEVICE_STATE_DISCONNECTED)
                            break;

                        g_usleep(500 * 1000);  // 500ms (much faster response)
                    }
                }
            }

            if(deviceState > NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_WARNING("Device not fully disconnected (state: %d), setting to unmanaged state", deviceState);
            }

            // Set the "Managed" property to enable/disable the device
            /*GDBusProxy* propertyProxy = m_dbus.getNetworkManagerPropertyProxy(devInfo.path.c_str());
            if(propertyProxy == NULL)
                return false;*/

            GVariant* result = g_dbus_proxy_call_sync(
                    deviceProxy,
                    "Set",
                    g_variant_new("(ssv)", "org.freedesktop.NetworkManager.Device", "Managed", g_variant_new_boolean(enable)),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error
                    );

            bool success = (error == nullptr);

            if (error) {
                NMLOG_ERROR("Error %s network interface: %s", (enable ? "enabling" : "disabling"), error->message);
                g_error_free(error);
            } else {
                NMLOG_DEBUG("Network interface %s successfully %s", interface.c_str(), (enable ? "enabled" : "disabled"));
                if (result != nullptr) {
                    g_variant_unref(result);
                }
            }
            g_object_unref(deviceProxy);

            if(success)
            {
                // Set persistent marker file for specific interface
                if(interface == GnomeUtils::getEthIfname())
                    nmUtils::setMarkerFile(EthernetDisableMarker, enable);
                else if(interface == GnomeUtils::getWifiIfname())
                    nmUtils::setMarkerFile(WiFiDisableMarker, enable);

                // Auto-reconnection logic when interface is enabled
                if(enable) {
                    // Wait for 1 sec to change the device state
                    sleep(1);
                    if(interface == GnomeUtils::getWifiIfname() && _instance != nullptr) {
                        NMLOG_INFO("Activating connection '%s' ...", _instance->m_lastConnectedSSID.c_str());
                        activateKnownConnection(GnomeUtils::getWifiIfname(), _instance->m_lastConnectedSSID);
                    }
                    else if(interface == GnomeUtils::getEthIfname()) {
                        NMLOG_INFO("Activating connection 'Wired connection 1' ...");
                        // default wired connection name is 'Wired connection 1'
                        activateKnownConnection(GnomeUtils::getEthIfname(), "Wired connection 1");
                    }
                }
            }

            return success;
        }

        bool NetworkManagerClient::getInterfaceState(const std::string& interface, bool& isEnabled)
        {
            deviceInfo devInfo{};
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, interface.c_str(), devInfo))
                return false;

            GError* error = nullptr;
            GDBusProxy* deviceProxy = m_dbus.getNetworkManagerPropertyProxy(devInfo.path.c_str());
            if(deviceProxy == nullptr)
                return false;

            // Call the "Get" method using the proxy
            GVariant* result = g_dbus_proxy_call_sync(
                    deviceProxy,
                    "Get",
                    g_variant_new("(ss)", "org.freedesktop.NetworkManager.Device", "Managed"),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error
                    );

            if (error != nullptr) {
                NMLOG_ERROR("Error getting network interface state: %s", error->message);
                g_error_free(error);
            } else {
                NMLOG_DEBUG("Network interface state retrieved successfully: %s",devInfo.path.c_str());
                GVariant* managedVariant;
                g_variant_get(result, "(v)", &managedVariant);

                isEnabled = g_variant_get_boolean(managedVariant);
                g_variant_unref(managedVariant);

                NMLOG_DEBUG("Interface %s is %s", interface.c_str(), isEnabled ? "enabled" : "disabled");

                g_variant_unref(result);
            }

            return true;
        }

        bool NetworkManagerClient::setIPSettings(const std::string& interface, const Exchange::INetworkManager::IPAddress& address)
        {
            std::string connectionPath;

            if(interface.empty() || (interface != GnomeUtils::getEthIfname() && interface != GnomeUtils::getWifiIfname()))
            {
                NMLOG_ERROR("Invalid interface name: %s", interface.c_str());
                return false;
            }

            if (!GnomeUtils::getSettingsConnectionPath(m_dbus, connectionPath, interface))
            {
                NMLOG_ERROR("Error: connection path not found for interface %s", interface.c_str());
                return false;
            }
            if (connectionPath.empty()) {
                NMLOG_WARNING("No active connections available to edit for interface %s", interface.c_str());
                return true;
            }
            if(!updateIPSettings(m_dbus, connectionPath, address, interface))
            {
                NMLOG_ERROR("Error: Failed to update route metric for Interface %s", interface.c_str());
                return false;
            }

            return true;
        }

        bool NetworkManagerClient::getIPSettings(std::string& interface, const std::string& ipversion, Exchange::INetworkManager::IPAddress& result)
        {
            std::string devicePath;
            std::string addressStr;
            guint32 prefix = 0;
            const gchar *gatewayIp = nullptr;
            gchar *dhcpServerIp = nullptr;
            gchar **dnsList = nullptr;
            gchar *ip4ConfigPath = nullptr;
            gchar *dhcp4ConfigPath = nullptr;
            gchar *dnsAddresses = nullptr;
            gchar *dhcp6ConfigPath = nullptr;
            gchar *ip6ConfigPath = nullptr;
            GDBusProxy* ipv4Proxy = nullptr;
            GDBusProxy* ipv6Proxy = nullptr;
            const gchar *IPv4Method = nullptr;
            const gchar *IPv6Method = nullptr;
            deviceInfo devInfo{};
            GError *error = nullptr;

            std::string wifiname = GnomeUtils::getWifiIfname(), ethname = GnomeUtils::getEthIfname();

            if(interface.empty())
            {
                if(Core::ERROR_NONE != _instance->GetPrimaryInterface(interface))
                {
                    NMLOG_WARNING("default interface get failed");
                    return true;
                }
                if(interface.empty())
                {
                    NMLOG_DEBUG("default interface return empty default is wlan0");
                    interface = wifiname;
                }
            }
            else if(wifiname != interface && ethname != interface)
            {
                NMLOG_ERROR("interface: %s; not valied", interface.c_str());
                return false;
            }

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, interface.c_str(), devInfo))
            {
                return false;
            }

            if(devInfo.state <= NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_WARNING("Device state is not a valid state: (%d)", devInfo.state);
                return false;
            }
            else if (devInfo.state > NM_DEVICE_STATE_DISCONNECTED && devInfo.state < NM_DEVICE_STATE_IP_CHECK)
            {
                result.autoconfig = true;
                if(ipversion.empty())
                    result.ipversion = "IPv4";
                else
                    result.ipversion = ipversion;
                return true;
            }

            if(!GnomeUtils::getDeviceByIpIface(m_dbus, interface.c_str(), devicePath))
                return false;
            GDBusProxy *deviceProxy = m_dbus.getNetworkManagerDeviceProxy(devicePath.c_str());
            if(deviceProxy == nullptr)
                return false;
            if(ipversion.empty() || (g_strcmp0(ipversion.c_str(), "IPv4") == 0))
            {
                GVariant *ip4Property = g_dbus_proxy_get_cached_property(deviceProxy, "Ip4Config");
                if (ip4Property != nullptr)
                {
                    g_variant_get(ip4Property, "o", &ip4ConfigPath);
                    g_variant_unref(ip4Property);
                }
                else
                {
                    NMLOG_ERROR("Failed to get Ip4Config property");
                    g_object_unref(deviceProxy);
                }

                GVariant *dhcp4Property = g_dbus_proxy_get_cached_property(deviceProxy, "Dhcp4Config");
                if (dhcp4Property != nullptr)
                {
                    g_variant_get(dhcp4Property, "o", &dhcp4ConfigPath);
                    g_variant_unref(dhcp4Property);
                }
                else
                {
                    NMLOG_ERROR("Failed to get Dhcp4Config property");
                    g_object_unref(deviceProxy);
                }

                ipv4Proxy = m_dbus.getNetworkManagerIpv4Proxy(ip4ConfigPath);
                g_free(ip4ConfigPath);
                // Get the 'Addresses' property
                if(ipv4Proxy != nullptr)
                {
                    GVariant *addressesProperty = g_dbus_proxy_get_cached_property(ipv4Proxy, "Addresses");

                    if(addressesProperty != nullptr)
                    {
                        gsize numAddresses = g_variant_n_children(addressesProperty);
                        for (gsize i = 0; i < numAddresses; ++i) {
                            GVariant *addressArray = g_variant_get_child_value(addressesProperty, i);
                            GVariantIter iter;
                            guint32 addr;

                            g_variant_iter_init(&iter, addressArray);

                            if (g_variant_iter_next(&iter, "u", &addr)) {
                                addressStr = GnomeUtils::ip4ToString(addr);
                                NMLOG_DEBUG("IP Address: %s", addressStr.c_str());
                            }
                            if (g_variant_iter_next(&iter, "u", &prefix)) {
                                NMLOG_DEBUG("Prefix: %d", prefix);
                            }

                            g_variant_unref(addressArray);
                        }

                        g_variant_unref(addressesProperty);
                    }
                    else
                    {
                        NMLOG_ERROR("Failed to get Addresses property");
                        g_object_unref(ipv4Proxy);
                    }

                    // Get the 'Gateway' property
                    GVariant *gatewayProperty = g_dbus_proxy_get_cached_property(ipv4Proxy, "Gateway");
                    if (gatewayProperty != nullptr) {
                        // Fetch and print the Gateway IP
                        gatewayIp = g_variant_get_string(gatewayProperty, nullptr);
                        NMLOG_DEBUG("Gateway: %s", gatewayIp);
                        g_variant_unref(gatewayProperty);
                    }
                    else
                    {
                        NMLOG_ERROR("Failed to get Gateway property");
                        g_variant_unref(addressesProperty);
                        g_object_unref(ipv4Proxy);
                    }
                }
                GDBusProxy* dhcpv4Proxy = m_dbus.getNetworkManagerDhcpv4Proxy(dhcp4ConfigPath);
                g_free(dhcp4ConfigPath);

                if(dhcpv4Proxy != nullptr)
                {
                    // Get the 'Options' property
                    GVariant *optionsProperty = g_dbus_proxy_get_cached_property(dhcpv4Proxy, "Options");

                    if (optionsProperty != nullptr) {

                        GVariantIter *iter;
                        gchar *key;
                        GVariant *value;

                        // Print the whole options property for debugging
                        gchar *optionsStr = g_variant_print(optionsProperty, TRUE);
                        NMLOG_DEBUG("Options property: %s", optionsStr);
                        g_free(optionsStr);

                        g_variant_get(optionsProperty, "a{sv}", &iter);

                        while (g_variant_iter_next(iter, "{&sv}", &key, &value)) {
                            if (g_strcmp0(key, "dhcp_server_identifier") == 0) {
                                dhcpServerIp = g_strdup(g_variant_get_string(value, nullptr));
                            } else if (g_strcmp0(key, "domain_name_servers") == 0) {
                                dnsAddresses = g_strdup(g_variant_get_string(value, nullptr));
                            }
                            g_variant_unref(value);

                            if (dhcpServerIp && dnsAddresses) {
                                break;
                            }
                        }

                        g_variant_iter_free(iter);
                        g_variant_unref(optionsProperty);
                        g_object_unref(dhcpv4Proxy);

                        if (dhcpServerIp) {
                            NMLOG_DEBUG("DHCP server IP address: %s", dhcpServerIp);
                        } else {
                            dhcpServerIp = nullptr;
                            NMLOG_DEBUG("Failed to find DHCP server IP address");
                        }
                        if (dnsAddresses) {
                            // DNS addresses are space-separated, we need to split them
                            dnsList = g_strsplit(dnsAddresses, " ", -1);
                            if (dnsList[0] != nullptr) {
                                NMLOG_DEBUG("Primary DNS: %s", dnsList[0]);
                                if (dnsList[1] != nullptr) {
                                    NMLOG_DEBUG("Secondary DNS: %s", dnsList[1]);
                                } else {
                                    NMLOG_DEBUG("Secondary DNS: Not available");
                                }
                            } else {
                                NMLOG_DEBUG("Failed to parse DNS addresses");
                            }
                        } else {
                            NMLOG_DEBUG("Failed to find DNS addresses");
                        }
                    }
                    else
                    {
                        NMLOG_ERROR("Failed to get Options property");
                        g_object_unref(dhcpv4Proxy);
                    }
                }
                result.ipversion = "IPv4";
            }
            if((addressStr.empty() && !(g_strcmp0(ipversion.c_str(), "IPv4") == 0)) || g_strcmp0(ipversion.c_str(), "IPV6") == 0)
            {
                GVariant *ip6Property = g_dbus_proxy_get_cached_property(deviceProxy, "Ip6Config");
                if (ip6Property != nullptr) {
                    g_variant_get(ip6Property, "o", &ip6ConfigPath);
                    g_variant_unref(ip6Property);
                }
                else
                {
                    NMLOG_ERROR("Failed to get Ip6Config property");
                    g_object_unref(deviceProxy);
                }

                GVariant *dhcp6Property = g_dbus_proxy_get_cached_property(deviceProxy, "Dhcp6Config");
                if (dhcp6Property != nullptr) {
                    g_variant_get(dhcp6Property, "o", &dhcp6ConfigPath);
                    g_variant_unref(dhcp6Property);
                }
                else
                {
                    NMLOG_ERROR("Failed to get Dhcp6Config property");
                    g_object_unref(deviceProxy);
                }


                ipv6Proxy = m_dbus.getNetworkManagerIpv6Proxy(ip6ConfigPath);
                g_free(ip6ConfigPath);
                if(ipv6Proxy != nullptr)
                {
                    // Get the 'Addresses' property
                    GVariant *addressesProperty = g_dbus_proxy_get_cached_property(ipv6Proxy, "Addresses");
                    if (addressesProperty != nullptr)
                    {
                        gsize numAddresses = g_variant_n_children(addressesProperty);
                        for (gsize i = 0; i < numAddresses; ++i) {
                            GVariant *addressTuple = g_variant_get_child_value(addressesProperty, i);
                            GVariant *addressArray = g_variant_get_child_value(addressTuple, 0); // Get the first byte array (IPv6 address)
                            GVariant *prefixVariant = g_variant_get_child_value(addressTuple, 1); // Get the prefix
                            prefix = g_variant_get_uint32(prefixVariant);

                            uint8_t ipv6Addr[16];
                            gsize addrLen;
                            gconstpointer addrData = g_variant_get_fixed_array(addressArray, &addrLen, 1);
                            if (addrLen == 16) {
                                memcpy(ipv6Addr, addrData, addrLen);
                                addressStr = GnomeUtils::ip6ToString(ipv6Addr);
                                if ((ipv6Addr[0] & 0xE0) == 0x20) // Check if the first three bits are within the range for global
                                {
                                    break;
                                }
                                NMLOG_DEBUG("IPv6 Address: %s Prefix: %d", addressStr.c_str(), prefix);
                            }

                            g_variant_unref(addressArray);
                            g_variant_unref(prefixVariant);
                            g_variant_unref(addressTuple);
                        }
                    }
                    else
                    {
                        NMLOG_ERROR("Failed to get Addresses property for IPv6");
                        g_object_unref(ipv6Proxy);
                    }
                    g_variant_unref(addressesProperty);

                    // Get the 'Gateway' property
                    GVariant *gatewayProperty = g_dbus_proxy_get_cached_property(ipv6Proxy, "Gateway");
                    if (gatewayProperty != nullptr)
                    {
                        // Fetch and print the Gateway IP
                        gatewayIp = g_variant_get_string(gatewayProperty, nullptr);
                        NMLOG_DEBUG("IPv6 Gateway: %s", gatewayIp);
                        g_variant_unref(gatewayProperty);
                    }
                    else
                    {
                        gatewayIp = nullptr;
                        NMLOG_ERROR("Failed to get Gateway property for IPv6");
                        g_object_unref(ipv6Proxy);
                    }
                }
                GDBusProxy* dhcpv6Proxy = m_dbus.getNetworkManagerDhcpv6Proxy(dhcp6ConfigPath);
                g_free(dhcp6ConfigPath);
                if(dhcpv6Proxy != nullptr)
                {
                    // Get the 'Options' property
                    GVariant *optionsProperty = g_dbus_proxy_get_cached_property(dhcpv6Proxy, "Options");
                    if (optionsProperty != nullptr) {

                        GVariantIter *iter;
                        gchar *key;
                        GVariant *value;
                        gchar *ip6Address = nullptr;

                        // Print the whole options property for debugging
                        gchar *optionsStr = g_variant_print(optionsProperty, TRUE);
                        NMLOG_DEBUG("Options property: %s", optionsStr);
                        g_free(optionsStr);

                        g_variant_get(optionsProperty, "a{sv}", &iter);

                        while (g_variant_iter_next(iter, "{&sv}", &key, &value)) {
                            gchar *valueStr = g_variant_print(value, TRUE);
                            NMLOG_DEBUG("Key: %s Value: %s", key, valueStr);
                            g_free(valueStr);

                            if (g_strcmp0(key, "dhcp6_name_servers") == 0) {
                                dnsAddresses = g_strdup(g_variant_get_string(value, nullptr));
                            } else if (g_strcmp0(key, "ip6_address") == 0) {
                                ip6Address = g_strdup(g_variant_get_string(value, nullptr));
                            }
                            g_variant_unref(value);
                        }

                        g_variant_iter_free(iter);
                        g_variant_unref(optionsProperty);
                        g_object_unref(dhcpv6Proxy);

                        if (ip6Address) {
                            NMLOG_DEBUG("IPv6 Address: %s", ip6Address);
                            g_free(ip6Address);
                        } else {
                            NMLOG_ERROR("Failed to find the IPv6 address");
                        }

                        if (dnsAddresses) {
                            // DNS addresses are space-separated, we need to split them
                            dnsList = g_strsplit(dnsAddresses, " ", -1);
                            if (dnsList[0] != nullptr) {
                                NMLOG_DEBUG("Primary DNS: %s", dnsList[0]);
                                if (dnsList[1] != nullptr) {
                                    NMLOG_DEBUG("Secondary DNS: %s ", dnsList[1]);
                                } else {
                                    NMLOG_DEBUG("Secondary DNS: Not available");
                                }
                            } else {
                                NMLOG_ERROR("Failed to parse DNS addresses");
                            }
                        } else {
                            dnsList = nullptr;
                            std::cerr << "Failed to find DNS addresses" << std::endl;
                        }
                    }
                    else
                    {
                        NMLOG_ERROR("Failed to get Options property for DHCP6");
                        g_object_unref(dhcpv6Proxy);
                    }
                }
                result.ipversion = "IPv6";
            }
            if(addressStr.empty())
            {
                result.autoconfig = true;
                if(ipversion.empty())
                    result.ipversion = "IPv4";
            }
            std::string connectionPath;
            if (!GnomeUtils::getSettingsConnectionPath(m_dbus, connectionPath, interface))
            {
                NMLOG_ERROR("Error: connection path not found for interface %s", interface.c_str());
                return false;
            }

            GDBusProxy *settingsProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(connectionPath.c_str());

            if (settingsProxy == nullptr) {
                NMLOG_ERROR("Error creating connection settings proxy");
                return false;
            }

            GVariant *connectionSettings = g_dbus_proxy_call_sync(
                    settingsProxy,
                    "GetSettings",
                    nullptr,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (connectionSettings == nullptr) {
                NMLOG_ERROR("Error retrieving connection settings: %s", error->message);
                g_error_free(error);
                g_object_unref(settingsProxy);
                return false;
            }

            GVariantIter *iterator;
            GVariant *settingsDict;
            const gchar *settingsKey;

            g_variant_get(connectionSettings, "(a{sa{sv}})", &iterator);
            while (g_variant_iter_loop(iterator, "{&s@a{sv}}", &settingsKey, &settingsDict)) {
                GVariantIter settingsIter;
                const gchar *key;
                GVariant *value;

                g_variant_iter_init(&settingsIter, settingsDict);
                while (g_variant_iter_loop(&settingsIter, "{&sv}", &key, &value)) {
                    if (g_strcmp0(key, "method") == 0) {
                        if(g_strcmp0(settingsKey, "ipv4") == 0)
                        {
                            IPv4Method = g_variant_get_string(value, NULL);
                            NMLOG_DEBUG("IPV4 Method: %s\n", IPv4Method);
                        }
                        else if(g_strcmp0(settingsKey, "ipv6") == 0)
                        {
                            IPv6Method = g_variant_get_string(value, NULL);
                            NMLOG_DEBUG("IPV6 Method: %s\n", IPv6Method);
                        }
                    }
                }
            }

            result.autoconfig = true;
            if(g_strcmp0(ipversion.c_str(), "IPv4") == 0)
                result.autoconfig = (g_strcmp0(IPv4Method, "auto") == 0);
            else if(g_strcmp0(ipversion.c_str(), "IPv6") == 0)
                result.autoconfig = (g_strcmp0(IPv6Method, "auto") == 0);
            result.ipaddress = addressStr;
            result.prefix = prefix;
            result.ula = "";
            result.dhcpserver = (dhcpServerIp != NULL) ? std::string(dhcpServerIp) : "";
            result.gateway = (gatewayIp != NULL) ? std::string(gatewayIp) : "";
            if (dnsList != NULL)
            {
                result.primarydns = (dnsList[0] != NULL) ? std::string(dnsList[0]) : "";
                result.secondarydns = (dnsList[1] != NULL) ? std::string(dnsList[1]) : "";
            }
            else
            {
                result.primarydns = "";
                result.secondarydns = "";
            }
            if(ipv4Proxy != NULL)
                g_object_unref(ipv4Proxy);
            if(ipv6Proxy != NULL)
                g_object_unref(ipv6Proxy);
            g_strfreev(dnsList);
            g_free(dnsAddresses);
            g_free(dhcpServerIp);

            return true;
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
        
            settingsProxy = g_dbus_proxy_call_sync(ConnProxy, "GetSettings", NULL, G_DBUS_CALL_FLAGS_NONE, -1,  NULL, &error);
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
                                    //NMLOG_DEBUG("SSID: %s", ssid.c_str());
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
                                NMLOG_DEBUG("ssid: %s key-mgmt: %s", ssid.c_str(), keyMgmt);
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

        static bool deleteConnection(DbusMgr& m_dbus, const std::string& path, std::string& ssid)
        {
            GError *error = NULL;
            GDBusProxy *ConnProxy = NULL;
            GVariant *deleteVar= NULL;

            ConnProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(path.c_str());
            if(ConnProxy == NULL)
                return false;

            deleteVar = g_dbus_proxy_call_sync(ConnProxy, "Delete", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
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

        bool NetworkManagerClient::getMatchingSSIDInfo(Exchange::INetworkManager::WiFiSSIDInfo& ssidInfo, std::string& apPathStr)
        {
            GError* error = nullptr;
            GDBusProxy* wProxy = nullptr;
            deviceInfo devProperty{};
            Exchange::INetworkManager::WiFiSSIDInfo apInfo{};
            bool ret = false;

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
                if(apPath == NULL)
                    continue;
                // NMLOG_DEBUG("Access Point Path: %s", apPath);
                if(GnomeUtils::getApDetails(m_dbus, apPath, apInfo))
                {
                    if(ssidInfo.ssid == apInfo.ssid)
                    {
                        ssidInfo = apInfo;
                        apPathStr = apPath;
                        ret = true;
                        break;
                    }
                }
            }

            g_variant_iter_free(iter);
            g_variant_unref(result);
            g_object_unref(wProxy);

            return ret;
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

            GVariant* timeVariant = NULL;
            timeVariant = g_dbus_proxy_get_cached_property(wProxy, "LastScan");
            if (!timeVariant) {
                NMLOG_ERROR("Failed to get LastScan properties");
                g_object_unref(wProxy);
                return false;
            }

            if (g_variant_is_of_type (timeVariant, G_VARIANT_TYPE_INT64)) {
                gint64 timestamp;
                timestamp = g_variant_get_int64 (timeVariant);
                NMLOG_DEBUG("Last scan timestamp: %lld", static_cast<long long int>(timestamp));
            } else {
                g_warning("Unexpected variant type: %s", g_variant_get_type_string (timeVariant));
            }

            if (!ssid.empty()) 
            {
                GVariantBuilder builder;
                NMLOG_INFO("Starting WiFi scanning with SSID: %s", ssid.c_str());
                GVariantBuilder ssidArray;
                g_variant_builder_init(&ssidArray, G_VARIANT_TYPE("aay"));
                g_variant_builder_add_value(
                    &ssidArray, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, (const guint8 *)ssid.c_str(), ssid.length(),1)
                    );
                /* ssid GVariant = [['S', 'S', 'I', 'D']] */
                g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
                g_variant_builder_add(&builder, "{sv}", "ssids", g_variant_builder_end(&ssidArray));
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

        bool NetworkManagerClient::isWifiScannedRecently(int timelimitInSec)
        {
            deviceInfo devInfo;
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), devInfo))
            {
                NMLOG_ERROR("Failed to get WiFi device info");
                return false;
            }

            if(devInfo.path.empty() || devInfo.state < NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_ERROR("WiFi device not available for scan check");
                return false;
            }

            GDBusProxy* wProxy = m_dbus.getNetworkManagerWirelessProxy(devInfo.path.c_str());
            if(wProxy == NULL)
            {
                NMLOG_ERROR("Failed to create wireless proxy for scan check");
                return false;
            }

            // Get LastScan property
            GVariant* timeVariant = g_dbus_proxy_get_cached_property(wProxy, "LastScan");
            if (!timeVariant) {
                NMLOG_ERROR("Failed to get LastScan property");
                g_object_unref(wProxy);
                return false;
            }

            if (!g_variant_is_of_type(timeVariant, G_VARIANT_TYPE_INT64)) {
                NMLOG_ERROR("Unexpected LastScan variant type: %s", g_variant_get_type_string(timeVariant));
                g_variant_unref(timeVariant);
                g_object_unref(wProxy);
                return false;
            }

            gint64 last_scan_time = g_variant_get_int64(timeVariant);
            g_variant_unref(timeVariant);
            g_object_unref(wProxy);

            if (last_scan_time <= 0) {
                NMLOG_INFO("No scan has been performed yet");
                return false;
            }

            // Get current time in milliseconds
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            gint64 current_time_in_msec = (gint64)ts.tv_sec * 1000 + (gint64)ts.tv_nsec / 1000000;

            gint64 time_difference_in_seconds = (current_time_in_msec - last_scan_time) / 1000;

            NMLOG_DEBUG("Last scan: %lld ms ago, limit: %d sec", (long long)time_difference_in_seconds, timelimitInSec);

            if (time_difference_in_seconds <= timelimitInSec) {
                NMLOG_DEBUG("WiFi was scanned recently (%lld seconds ago), skipping scan", (long long)time_difference_in_seconds);
                return true;
            }

            NMLOG_DEBUG("WiFi scan is needed, last scan was %lld seconds ago", (long long)time_difference_in_seconds);
            return false;
        }

        bool NetworkManagerClient::getDeviceInfo(const std::string& interface, deviceInfo& devInfo)
        {
            return GnomeUtils::getDeviceInfoByIfname(m_dbus, interface.c_str(), devInfo);
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

        bool addNewConnctionAndactivate(DbusMgr& m_dbus, GVariantBuilder& connBuilder, const char* devicePath, bool persist, const char* specificObject)
        {
            GDBusProxy* proxy = nullptr;
            GError* error = nullptr;
            GVariant* result = nullptr;

            proxy = m_dbus.getNetworkManagerProxy();
            if(proxy == NULL)
                return false;

            GVariant *connBuilderVariant = g_variant_builder_end(&connBuilder);
            if(connBuilderVariant == NULL)
            {
                NMLOG_ERROR("Failed to build connection settings");
                g_object_unref(proxy);
                return false;
            }

            GVariantBuilder optionsBuilder;
            g_variant_builder_init (&optionsBuilder, G_VARIANT_TYPE ("a{sv}"));
            if(!persist) // by default it will be in disk mode
            {
                NMLOG_WARNING("wifi connection will not persist to the disk");
                g_variant_builder_add(&optionsBuilder, "{sv}", "persist", g_variant_new_string("volatile"));
            }
            //else
                //g_variant_builder_add(&optionsBuilder, "{sv}", "persist", g_variant_new_string("disk"));

            GVariant *optionBuilderVariant = g_variant_builder_end(&optionsBuilder);
            if(optionBuilderVariant == NULL)
            {
                NMLOG_ERROR("Failed to build options settings");
                g_object_unref(proxy);
                return false;
            }

            NMLOG_DEBUG("devicePath %s, specificObject %s", devicePath, specificObject);
            result = g_dbus_proxy_call_sync (proxy, "AddAndActivateConnection2",
                g_variant_new("(@a{sa{sv}}oo@a{sv})", connBuilderVariant, devicePath?: "/", specificObject?: "/", optionBuilderVariant),
                G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error);

            if (result == NULL) {
                if(error != NULL)
                {
                    NMLOG_ERROR("Failed to call AddAndActivateConnection2: %s", error->message);
                    g_clear_error(&error);
                }
                g_object_unref(proxy);
                return false;
            }

            NMLOG_DEBUG("AddAndActivateConnection2 success !!!");

            GVariant* pathVariant = NULL;
            GVariant* activeConnVariant = NULL;
            GVariant* resultDict = NULL;

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

        static inline std::string generateUUID()
        {
            uuid_t uuid{};
            char buffer[37] = {0};  // 36 + NULL
            uuid_generate_random(uuid);
            uuid_unparse_lower(uuid, buffer);
            if(buffer[0] != '\0')
                return std::string(buffer);
            return std::string("");
        }

        static bool connectionBuilder(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo, GVariantBuilder& connBuilder, bool iswpsAP = false)
        {
            GVariantBuilder settingsBuilder;

            g_variant_builder_init (&connBuilder, G_VARIANT_TYPE ("a{sa{sv}}"));
            if(ssidinfo.ssid.empty() || ssidinfo.ssid.length() > 32)
            {
                NMLOG_WARNING("ssid name is missing or invalied");
                return false;
            }

            NMLOG_INFO("ssid %s, security %d, persist %d", ssidinfo.ssid.c_str(), (int)ssidinfo.security, (int)ssidinfo.persist);

             /* Adding 'connection' settings */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));
            std::string uuid = generateUUID();
            if(uuid.empty() || uuid.length() < 32)
            {
                NMLOG_ERROR("uuid generate failed");
                return false;
            }
            g_variant_builder_add (&settingsBuilder, "{sv}", "uuid", g_variant_new_string (uuid.c_str()));
            g_variant_builder_add (&settingsBuilder, "{sv}", "id", g_variant_new_string (ssidinfo.ssid.c_str())); // connection id = ssid name
            g_variant_builder_add (&settingsBuilder, "{sv}", "interface-name", g_variant_new_string (GnomeUtils::getWifiIfname()));
            g_variant_builder_add (&settingsBuilder, "{sv}", "type", g_variant_new_string ("802-11-wireless"));
            g_variant_builder_add (&connBuilder, "{sa{sv}}", "connection", &settingsBuilder);
            /* Adding '802-11-wireless 'settings */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));
            GVariant *ssidArray = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, (const guint8 *)ssidinfo.ssid.c_str(), ssidinfo.ssid.length(), 1);
            g_variant_builder_add (&settingsBuilder, "{sv}", "ssid", ssidArray);
            g_variant_builder_add (&settingsBuilder, "{sv}", "mode", g_variant_new_string("infrastructure"));
            if(!iswpsAP) // wps never be a hidden AP, it will be always visible
                g_variant_builder_add (&settingsBuilder, "{sv}", "hidden", g_variant_new_boolean(true)); // set hidden: yes
            g_variant_builder_add (&connBuilder, "{sa{sv}}", "802-11-wireless", &settingsBuilder);

             /* Adding '802-11-wireless-security' settings */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));

            switch(ssidinfo.security)
            {
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE:
                {
                    if(Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE == ssidinfo.security)
                    {
                        NMLOG_DEBUG("802-11-wireless-security key-mgmt: 'sae'");
                        g_variant_builder_add (&settingsBuilder, "{sv}", "key-mgmt", g_variant_new_string("sae"));
                    }
                    else
                    {
                        NMLOG_DEBUG("802-11-wireless-security key-mgmt: 'wpa-psk'");
                        g_variant_builder_add (&settingsBuilder, "{sv}", "key-mgmt", g_variant_new_string("wpa-psk"));  // WPA + WPA2 + WPA3 personal
                    }

                    /* if ap is not a wps network */
                    if(!iswpsAP)
                    {
                        if(ssidinfo.passphrase.empty() || ssidinfo.passphrase.length() < 8)
                        {
                            NMLOG_WARNING("wifi securtity type password erro length > 8");
                            GVariant *sVariant = g_variant_builder_end(&settingsBuilder);
                            if(sVariant)
                                g_variant_unref(sVariant);
                            return false;
                        }
                        g_variant_builder_add (&settingsBuilder, "{sv}", "psk", g_variant_new_string(ssidinfo.passphrase.c_str())); // password
                    }
                    break;
                }
                case Exchange::INetworkManager::WIFI_SECURITY_NONE:
                {
                    NMLOG_DEBUG("802-11-wireless-security key-mgmt: 'none'");
                   // g_variant_builder_add (&settingsBuilder, "{sv}", "key-mgmt", g_variant_new_string("none"));  // no password protection
                    break;
                }
                default:
                {
                    // ToDo handile Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_EAP
                    NMLOG_WARNING("connection wifi securtity type not supported %d", ssidinfo.security);
                    GVariant *sVariant = g_variant_builder_end(&settingsBuilder);
                    if(sVariant)
                        g_variant_unref(sVariant);
                    return false;
                }
            }

            g_variant_builder_add (&connBuilder, "{sa{sv}}", "802-11-wireless-security", &settingsBuilder);

            // Get persistent hostname or fall back to device hostname
            std::string hostname;
            if (!GnomeUtils::readPersistentHostname(hostname)) {
                const char* deviceHostname = "rdk-device"; // default hostname
                hostname = deviceHostname;
                NMLOG_DEBUG("No persistent hostname found, using device hostname: %s", hostname.c_str());
            }
            NMLOG_INFO("DHCP hostname: %s", hostname.c_str());

            /* Adding the 'ipv4' Setting */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));
            g_variant_builder_add (&settingsBuilder, "{sv}", "method", g_variant_new_string ("auto"));
            g_variant_builder_add (&settingsBuilder, "{sv}", "dhcp-hostname", g_variant_new_string(hostname.c_str()));
            g_variant_builder_add (&settingsBuilder, "{sv}", "dhcp-send-hostname", g_variant_new_boolean(TRUE));
            g_variant_builder_add (&connBuilder, "{sa{sv}}", "ipv4", &settingsBuilder);

            /* Adding the 'ipv6' Setting */
            g_variant_builder_init (&settingsBuilder, G_VARIANT_TYPE ("a{sv}"));
            g_variant_builder_add (&settingsBuilder, "{sv}", "method", g_variant_new_string ("auto"));
            g_variant_builder_add (&settingsBuilder, "{sv}", "dhcp-hostname", g_variant_new_string(hostname.c_str()));
            g_variant_builder_add (&settingsBuilder, "{sv}", "dhcp-send-hostname", g_variant_new_boolean(TRUE));
            g_variant_builder_add (&connBuilder, "{sa{sv}}", "ipv6", &settingsBuilder);
            NMLOG_DEBUG("connection builder success...");
            return true;
        }

        bool NetworkManagerClient::addToKnownSSIDs(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo)
        {
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

            if(!connectionBuilder(ssidinfo, connBuilder))
            {
                NMLOG_WARNING("connection builder failed");
                GVariant *variant = g_variant_builder_end(&connBuilder);
                if(variant)
                    g_variant_unref(variant);
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
                    result = g_dbus_proxy_call_sync (proxy, "AddConnection",
                                g_variant_new ("(a{sa{sv}})", &connBuilder), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

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
            bool ssidSpecified = !ssid.empty();
            bool connectionFound = false;

            if(!ssidSpecified)
                NMLOG_WARNING("ssid is not specified, Deleting all available wifi connections!");

            std::list<std::string> paths;
            if(!GnomeUtils::getConnectionPaths(m_dbus, paths))
            {
                NMLOG_ERROR("remove connection path fetch failed");
                return false;
            }

            for (const std::string& path : paths) {
                // NMLOG_DEBUG("remove connection path %s", path.c_str());
                std::string connSsid;
                if(getSSIDFromConnection(m_dbus, path, connSsid)) {
                    // If SSID is specified, only delete matching connections
                    // If SSID is empty, delete all wireless connections
                    if (!ssidSpecified || connSsid == ssid) {
                        if(deleteConnection(m_dbus, path, connSsid)) {
                            NMLOG_INFO("delete '%s' connection ...", connSsid.c_str());
                            connectionFound = true;
                            ret = true;
                        }
                    }
                }
            }

            if(!connectionFound)
            {
                if(ssidSpecified)
                    NMLOG_WARNING("'%s' no such connection profile", ssid.c_str());
                else
                    NMLOG_WARNING("No wifi connection profiles found!");
            }

            return ret;
        }

        bool NetworkManagerClient::getWiFiSignalQuality(string& ssid, string& signalStrength, Exchange::INetworkManager::WiFiSignalQuality& quality)
        {
            Exchange::INetworkManager::WiFiSSIDInfo ssidInfo;
            float rssi = 0.0f;
            float noise = 0.0f;
            float floatSignalStrength = 0.0f;
            unsigned int signalStrengthOut = 0;

            if(!getConnectedSSID(ssidInfo))
            {
                NMLOG_ERROR("no wifi connected");
                return false;
            }
            else
            {
                ssid              = ssidInfo.ssid;
                if (!ssidInfo.strength.empty())
                    rssi          = std::stof(ssidInfo.strength.c_str());
                if (!ssidInfo.noise.empty())
                    noise         = std::stof(ssidInfo.noise.c_str());
                floatSignalStrength = (rssi - noise);
                if (floatSignalStrength < 0)
                    floatSignalStrength = 0.0;

                signalStrengthOut = static_cast<unsigned int>(floatSignalStrength);
                NMLOG_INFO ("WiFiSignalQuality in dB = %u",signalStrengthOut);

                if (signalStrengthOut == 0)
                {
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED;
                    signalStrength = "0";
                }
                else if (signalStrengthOut > 0 && signalStrengthOut < NM_WIFI_SNR_THRESHOLD_FAIR)
                {
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_WEAK;
                }
                else if (signalStrengthOut > NM_WIFI_SNR_THRESHOLD_FAIR && signalStrengthOut < NM_WIFI_SNR_THRESHOLD_GOOD)
                {
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_FAIR;
                }
                else if (signalStrengthOut > NM_WIFI_SNR_THRESHOLD_GOOD && signalStrengthOut < NM_WIFI_SNR_THRESHOLD_EXCELLENT)
                {
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_GOOD;
                }
                else
                {
                    quality = Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_EXCELLENT;
                }

                signalStrength = std::to_string(signalStrengthOut);

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

        bool NetworkManagerClient::wifiConnect(const Exchange::INetworkManager::WiFiConnectTo& connectInfo, bool iswpsAP)
        {
            GVariantBuilder connBuilder;
            bool reuseConnection = false;
            deviceInfo deviceProp;
            std::string exsistingConn;
            Exchange::INetworkManager::WiFiConnectTo ssidinfo = connectInfo;

            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), deviceProp))
                return false;

            if(deviceProp.path.empty() || deviceProp.state < NM_DEVICE_STATE_UNAVAILABLE)
            {
                NMLOG_ERROR("wlan0 interface not active");
                return false;
            }

            // Check if already connected to the requested SSID
            Exchange::INetworkManager::WiFiSSIDInfo currentSSID;
            if(getConnectedSSID(currentSSID))
            {
                if(ssidinfo.ssid == currentSSID.ssid)
                {
                    NMLOG_INFO("'%s' already connected !", currentSSID.ssid.c_str());
                    if(_instance != nullptr)
                        _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTED);
                    return true;
                }
                else
                    NMLOG_DEBUG("wifi already connected with %s AP", currentSSID.ssid.c_str());
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

            /* if ap is available check the security is matching to user requested */
            Exchange::INetworkManager::WiFiSSIDInfo apInfo{};
            apInfo.ssid = ssidinfo.ssid;
            std::string apPathStr = "/"; // default specific object path is "/"
            if(getMatchingSSIDInfo(apInfo, apPathStr))
            {
                if(ssidinfo.security != apInfo.security)
                {
                    NMLOG_WARNING("user requested wifi security '%d' != AP supported security %d ", ssidinfo.security, apInfo.security);
                    ssidinfo.security = apInfo.security;
                    // NMLOG_DEBUG("ap path %s", apPathStr.c_str());
                }
            }
            else
            {
                NMLOG_WARNING("matching ssid (%s) not found in scanning result", ssidinfo.ssid.c_str());
                /* ssid not found in scan list so add to known ssid it will do a scanning and connect */
                if(ssidinfo.persist)
                {
                    if(addToKnownSSIDs(ssidinfo))
                    {
                        NMLOG_DEBUG("Adding to known ssid '%s' ", ssidinfo.ssid.c_str());
                        return activateKnownConnection(GnomeUtils::getWifiIfname(), ssidinfo.ssid);
                    }
                    else
                    {
                        NMLOG_ERROR("Failed to add SSID '%s' to known SSIDs", ssidinfo.ssid.c_str());
                        return false;
                    }
                }
                else
                {
                    NMLOG_WARNING("SSID '%s' not found and persist is false, cannot connect", ssidinfo.ssid.c_str());
                    return false;
                }
            }

            if(reuseConnection)
            {
                NMLOG_INFO("activating connection...");
                if(!connectionBuilder(ssidinfo, connBuilder, iswpsAP)) {
                    NMLOG_WARNING("connection builder failed");
                    return false;
                }

                if (ssidinfo.persist)
                {
                    if(updateConnctionAndactivate(m_dbus, connBuilder, deviceProp.path.c_str(), exsistingConn.c_str()))
                        NMLOG_INFO("updated connection request success");
                    else
                    {
                        NMLOG_ERROR("wifi connect request failed");
                        return false;
                    }
                }
                else
                {
                    // Don't persist changes, just activate existing connection
                    // Note: Any changes made by connectionBuilder will be temporary for this session only
                    NMLOG_DEBUG("activating existing connection without persisting changes '%s'", ssidinfo.ssid.c_str());
                    if(GnomeUtils::activateConnection(m_dbus, exsistingConn, deviceProp.path))
                        NMLOG_INFO("activated existing connection without persistence");
                    else {
                        NMLOG_ERROR("failed to activate existing connection");
                        return false;
                    }
                }
            }
            else
            {
                NMLOG_DEBUG("creating new connection '%s' persist=%d", ssidinfo.ssid.c_str(), ssidinfo.persist);
                if(!connectionBuilder(ssidinfo, connBuilder, iswpsAP)) {
                    NMLOG_WARNING("connection builder failed");
                    return false;
                }
                if (ssidinfo.persist)
                {
                    if(addNewConnctionAndactivate(m_dbus, connBuilder, deviceProp.path.c_str(), ssidinfo.persist, apPathStr.c_str()))
                        NMLOG_INFO("wifi connect request success");
                    else
                    {
                        NMLOG_ERROR("wifi connect request failed");
                        return false;
                    }
                }
                else
                {
                    // Create temporary connection - add to memory only, do not save to disk
                    NMLOG_DEBUG("creating temporary connection for '%s'", ssidinfo.ssid.c_str());
                    if(addNewConnctionAndactivate(m_dbus, connBuilder, deviceProp.path.c_str(), ssidinfo.persist, apPathStr.c_str()))
                        NMLOG_INFO("temporary wifi connect request success");
                    else {
                        NMLOG_ERROR("temporary wifi connect request failed");
                        return false;
                    }
                }
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

            NMLOG_DEBUG("wifi device current state is %d !", devInfo.state);
            if(devInfo.state <= NM_DEVICE_STATE_DISCONNECTED || devInfo.state == NM_DEVICE_STATE_FAILED || devInfo.state == NM_DEVICE_STATE_DEACTIVATING)
            {
                NMLOG_WARNING("wifi in disconnected state");
                return true;
            }

            wProxy = m_dbus.getNetworkManagerDeviceProxy(devInfo.path.c_str());
            if(wProxy == NULL)
                return false;
            else
            g_dbus_proxy_call_sync(wProxy, "Disconnect", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
                return true;
            if (error) {
                NMLOG_ERROR("Error calling Disconnect method: %s", error->message);
                g_error_free(error);
                g_object_unref(wProxy);
                return false;
            }
            else
               NMLOG_INFO("wifi disconnected success");

            g_object_unref(wProxy);
            return true;
        }

        bool NetworkManagerClient::activateKnownConnection(const std::string& interface, const std::string& knownConnectionID)
        {
            NMLOG_DEBUG("Activating known connection: interface=%s, connectionID=%s", interface.c_str(), knownConnectionID.c_str());

            deviceInfo devInfo;
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, interface.c_str(), devInfo))
            {
                NMLOG_ERROR("Failed to get device info for interface: %s", interface.c_str());
                return false;
            }

            if(devInfo.path.empty() || devInfo.state < NM_DEVICE_STATE_UNAVAILABLE)
            {
                NMLOG_ERROR("Interface %s not active or available", interface.c_str());
                return false;
            }

            // Set autoconnect to true for the device
            GDBusProxy *deviceProxy = m_dbus.getNetworkManagerDeviceProxy(devInfo.path.c_str());
            if (deviceProxy != nullptr) {
                GVariant *autoconnectValue = g_variant_new_boolean(TRUE);
                GError *setError = nullptr;
                g_dbus_proxy_call_sync(
                    deviceProxy,
                    "Set",
                    g_variant_new("(ssv)", "org.freedesktop.NetworkManager.Device", "Autoconnect", autoconnectValue),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &setError);

                if (setError) {
                    NMLOG_WARNING("Failed to set autoconnect for device %s: %s", interface.c_str(), setError->message);
                    g_error_free(setError);
                } else {
                    NMLOG_DEBUG("Set autoconnect=true for device %s", interface.c_str());
                }
                g_object_unref(deviceProxy);
            }

            // Get all connections
            GError *error = nullptr;
            GDBusProxy *settingsProxy = m_dbus.getNetworkManagerSettingsProxy();
            if (settingsProxy == nullptr) {
                NMLOG_ERROR("Error creating NetworkManager settings proxy");
                return false;
            }

            GVariant *connectionsResult = g_dbus_proxy_call_sync(
                    settingsProxy,
                    "ListConnections",
                    nullptr,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (error || connectionsResult == nullptr) {
                NMLOG_ERROR("Error retrieving connections: %s", error ? error->message : "unknown error");
                if (error) g_error_free(error);
                g_object_unref(settingsProxy);
                return false;
            }

            GVariantIter *connectionIter;
            const gchar *connectionPath;
            std::string foundConnectionPath;
            std::string firstWiFiConnectionPath;

            g_variant_get(connectionsResult, "(ao)", &connectionIter);

            // Find the requested connection or first WiFi connection
            while (g_variant_iter_loop(connectionIter, "o", &connectionPath)) {
                std::string ssid;
                if(getSSIDFromConnection(m_dbus, connectionPath, ssid))
                {
                    NMLOG_DEBUG("Found WiFi connection: %s (SSID: %s)", connectionPath, ssid.c_str());

                    // If no specific connection requested, use first WiFi connection
                    if(firstWiFiConnectionPath.empty())
                        firstWiFiConnectionPath = connectionPath;

                    // If specific connection requested, look for exact match
                    if(!knownConnectionID.empty() && ssid == knownConnectionID)
                    {
                        foundConnectionPath = connectionPath;
                        NMLOG_INFO("Found matching connection for SSID: %s", knownConnectionID.c_str());
                        break;
                    }
                }
            }

            g_variant_iter_free(connectionIter);
            g_variant_unref(connectionsResult);
            g_object_unref(settingsProxy);

            // Determine which connection to activate
            std::string connectionToActivate;
            if(!foundConnectionPath.empty())
                connectionToActivate = foundConnectionPath;
            else if(knownConnectionID.empty() && !firstWiFiConnectionPath.empty())
            {
                connectionToActivate = firstWiFiConnectionPath;
                NMLOG_INFO("No specific connection requested, using first WiFi connection");
            }
            else
            {
                NMLOG_ERROR("No suitable connection found. Requested: '%s'", knownConnectionID.c_str());
                return false;
            }

            // Activate the connection
            if(GnomeUtils::activateConnection(m_dbus, connectionToActivate, devInfo.path))
            {
                NMLOG_INFO("Successfully activated known connection: %s", connectionToActivate.c_str());
                return true;
            }
            else
            {
                NMLOG_ERROR("Failed to activate connection: %s", connectionToActivate.c_str());
                return false;
            }
        }

        static bool getWpsPbcSSIDDetails(DbusMgr& m_dbus, const char* apPath, std::string& wpsApSsid)
        {
            guint32 flags = 0;
            bool ret = false;
            GVariant* ssidVariant = NULL;
            GDBusProxy* proxy = m_dbus.getNetworkManagerAccessPointProxy(apPath);
            if (proxy == NULL)
                return false;

            if (GnomeUtils::getCachedPropertyU(proxy, "Flags", &flags) && flags & NM_802_11_AP_FLAGS_WPS_PBC)
            {
                gsize ssid_length = 0;
                ssidVariant = g_dbus_proxy_get_cached_property(proxy,"Ssid");
                if (!ssidVariant) {
                    NMLOG_ERROR("Failed to get AP ssid properties.");
                    g_object_unref(proxy);
                    return false;
                }

                const guchar *ssid_data = static_cast<const guchar*>(g_variant_get_fixed_array(ssidVariant, &ssid_length, sizeof(guchar)));
                if (ssid_data && ssid_length > 0 && ssid_length <= 32)
                {
                    GVariant* result = NULL;
                    gchar *_bssid = NULL;
                    wpsApSsid.assign(reinterpret_cast<const char*>(ssid_data), ssid_length);
                    result = g_dbus_proxy_get_cached_property(proxy,"HwAddress");
                    if (!result) {
                        NMLOG_ERROR("Failed to get AP properties.");
                        g_object_unref(proxy);
                        return false;
                    }
                    g_variant_get(result, "s", &_bssid);
                    if(_bssid != NULL) {
                        NMLOG_INFO("WPS PBC AP found ssid: %s, bssid: %s", wpsApSsid.c_str(), _bssid);
                        ret = true;
                        g_free(_bssid);
                    }
                    g_variant_unref(result);
                }
                else
                    NMLOG_ERROR("WPS PBC Ap ssid Error !");

                if(ssidVariant)
                    g_variant_unref(ssidVariant);
            }

            if(proxy)
               g_object_unref(proxy);

            return ret;
        }

        static bool findWpsPbcSSID(DbusMgr& m_dbus, std::string& wpsApSsid)
        {
            GError* error = nullptr;
            GDBusProxy* wProxy = nullptr;
            deviceInfo devProperty{};
            bool isfound = false;

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

            while (g_variant_iter_loop(iter, "o", &apPath))
            {
                guint32 flags = 0;
                if(apPath == NULL)
                    continue;
                // NMLOG_DEBUG("Access Point Path: %s", apPath);
                isfound = getWpsPbcSSIDDetails(m_dbus, apPath, wpsApSsid);
                if(isfound)
                    break;
            }

            if(!isfound)
                NMLOG_WARNING("WPS PBC AP not found in scanned result !");

            g_variant_iter_free(iter);
            g_variant_unref(result);
            g_object_unref(wProxy);

            return isfound;
        }

        bool NetworkManagerClient::isActiveApSameAsWps(const std::string& wpsSSID)
        {
            deviceInfo devProperty{};
            GDBusProxy* wProxy = nullptr;
            GVariant* result = nullptr;
            bool isSame = false;

            // Get WiFi device info
            if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), devProperty))
                return false;

            if(devProperty.path.empty() || devProperty.state < NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_ERROR("no wifi device found");
                return false;
            }

            // Get WiFi device proxy
            wProxy = m_dbus.getNetworkManagerWirelessProxy(devProperty.path.c_str());
            if (wProxy == NULL)
                return false;

            // Get active access point
            result = g_dbus_proxy_get_cached_property(wProxy, "ActiveAccessPoint");
            if (!result) {
                NMLOG_DEBUG("No active access point found");
                g_object_unref(wProxy);
                return false;
            }

            const gchar* activeApPath = nullptr;
            g_variant_get(result, "o", &activeApPath);

            if (activeApPath && g_strcmp0(activeApPath, "/") != 0) {
                // Get AP proxy and check SSID
                GDBusProxy* apProxy = m_dbus.getNetworkManagerAccessPointProxy(activeApPath);
                if (apProxy) {
                    GVariant* ssidVariant = g_dbus_proxy_get_cached_property(apProxy, "Ssid");
                    if (ssidVariant) {
                        gsize ssid_length = 0;
                        const guchar *ssid_data = static_cast<const guchar*>(g_variant_get_fixed_array(ssidVariant, &ssid_length, sizeof(guchar)));
                        if (ssid_data && ssid_length > 0 && ssid_length <= 32) {
                            std::string activeSSID(reinterpret_cast<const char*>(ssid_data), ssid_length);
                            if (activeSSID == wpsSSID) {
                                NMLOG_INFO("Active AP matches WPS SSID: %s", wpsSSID.c_str());
                                isSame = true;
                            }
                        }
                        g_variant_unref(ssidVariant);
                    }
                    g_object_unref(apProxy);
                }
            }

            g_variant_unref(result);
            g_object_unref(wProxy);
            return isSame;
        }

        void NetworkManagerClient::wpsProcess()
        {
            m_wpsProcessRun = true;
            m_wpsActionTriggered = false;
            Exchange::INetworkManager::WiFiConnectTo ssidinfo{};
            Exchange::INetworkManager::WiFiState state;
            deviceInfo devProperty{};
            bool wpsComplete = false;
            NMLOG_INFO("WPS process started !");

            if(!getWifiState(state))
            {
                NMLOG_ERROR("wifi state error ! wps process stoped !");
                m_wpsProcessRun = false;
                return;
            }

            for(int retry = 0; retry < GDBUS_WPS_RETRY_COUNT; retry++)
            {
                if(m_wpsProcessRun.load() == false) // stop wps process if requested
                    break;
                sleep(GDBUS_WPS_RETRY_WAIT_IN_MS);
                if(m_wpsProcessRun.load() == false)
                    break;


                // Get current device info for state monitoring
                if(!GnomeUtils::getDeviceInfoByIfname(m_dbus, GnomeUtils::getWifiIfname(), devProperty))
                {
                    NMLOG_ERROR("Failed to get device info during WPS process");
                    break;
                }

                if(m_wpsActionTriggered)
                {
                    // If WPS action started, monitor device state for completion
                    if(devProperty.state <= NM_DEVICE_STATE_DISCONNECTED)
                    {
                        NMLOG_WARNING("WPS process failed - device in disconnected state");
                        if(_instance != nullptr)
                            _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
                        break;
                    }
                    else if(devProperty.state > NM_DEVICE_STATE_NEED_AUTH)
                    {
                        NMLOG_INFO("WPS process completed successfully");
                        wpsComplete = true;
                        if(_instance != nullptr)
                            _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTED);
                        break;
                    }

                    NMLOG_INFO("WPS process not completed yet, device state: %d", devProperty.state);
                    if(retry >= GDBUS_WPS_RETRY_COUNT - 1) // Last retry attempt
                    {
                        NMLOG_ERROR("WPS process failed - timeout");
                        if(_instance != nullptr)
                            _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED);
                        break;
                    }
                    continue;
                }

                // Look for WPS PBC SSID
                if(!findWpsPbcSSID(m_dbus, ssidinfo.ssid))
                {
                    if(!m_wpsActionTriggered)
                    {
                        // If WPS action not triggered, do a scanning request
                        startWifiScan();
                    }
                    NMLOG_DEBUG("WPS process retrying: %d ", retry+1);
                    continue;
                }

                // Check if already connected to the same WPS AP
                if(isActiveApSameAsWps(ssidinfo.ssid))
                {
                    NMLOG_INFO("WPS process stopped - already connected to WPS AP '%s'", ssidinfo.ssid.c_str());
                    wpsComplete = true;
                    if(_instance != nullptr)
                        _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTED);
                    break;
                }

                if(!getWifiState(state))
                {
                    NMLOG_ERROR("wifi state error ! wps process stoped !");
                    m_wpsProcessRun = false;
                    return;
                }

                if(Exchange::INetworkManager::WiFiState::WIFI_STATE_DISCONNECTED != state)
                {
                    NMLOG_INFO("Disconnecting current connection for WPS process");
                    wifiDisconnect();
                    sleep(3); // Wait for disconnect to complete
                }

                ssidinfo.security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK;
                ssidinfo.persist = true;

                NMLOG_INFO("Starting WPS connection to SSID: %s", ssidinfo.ssid.c_str());

                // Mark WPS action as triggered and adjust retry count
                m_wpsActionTriggered = true;
                retry = GDBUS_WPS_RETRY_COUNT - 3; // Expecting WPS process will be completed in 30 sec (3 * 10sec retry)

                /* security mode will be updated in wifi connect function, if not mathing to wpa-psk */
                wifiConnect(ssidinfo, true); // isWps = true
            }

            // Final status reporting
            if(!wpsComplete && !m_wpsActionTriggered)
            {
                NMLOG_WARNING("WPS AP not found");
                if(_instance != nullptr)
                    _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
            }
            else if(!wpsComplete && m_wpsActionTriggered)
            {
                NMLOG_INFO("WPS process error");
                if(_instance != nullptr)
                    _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED);
            }

            NMLOG_INFO("WPS process thread exit");
            m_wpsProcessRun = false;
        }

        bool NetworkManagerClient::startWPS()
        {
            NMLOG_DEBUG("Start WPS %s", __FUNCTION__);
            if(m_wpsProcessRun.load())
            {
                NMLOG_WARNING("wps process WPS already running");
                return true;
            }

            m_secretAgent.RegisterAgent();
            m_wpsthread = std::thread(&NetworkManagerClient::wpsProcess, this);
            m_wpsthread.detach();
            return true;
        }

        bool NetworkManagerClient::stopWPS() {
            NMLOG_DEBUG("Stop WPS %s", __FUNCTION__);
            m_wpsProcessRun = false;
            m_wpsActionTriggered = false;
            return m_secretAgent.UnregisterAgent();
        }

        bool NetworkManagerClient::modifyDefaultConnectionsConfig()
        {
            NMLOG_DEBUG("Initializing hostname settings for existing connections");

            // Get persistent hostname or use default
            std::string hostname;
            if (!GnomeUtils::readPersistentHostname(hostname)) {
                const char* deviceHostname = "rdk-device"; // default hostname
                hostname = deviceHostname;
                NMLOG_DEBUG("No persistent hostname found, using device hostname: %s", hostname.c_str());
            }

            GError *error = nullptr;
            GDBusProxy *settingsProxy = m_dbus.getNetworkManagerSettingsProxy();
            if (settingsProxy == nullptr) {
                NMLOG_ERROR("Error creating NetworkManager settings proxy");
                return false;
            }

            // Get all connections
            GVariant *connectionsResult = g_dbus_proxy_call_sync(
                    settingsProxy,
                    "ListConnections",
                    nullptr,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (error || connectionsResult == nullptr) {
                NMLOG_ERROR("Error retrieving connections for hostname init: %s", error ? error->message : "unknown error");
                if (error) g_error_free(error);
                g_object_unref(settingsProxy);
                return false;
            }

            GVariantIter *connectionIter;
            const gchar *connectionPath;
            bool success = true;

            g_variant_get(connectionsResult, "(ao)", &connectionIter);

            while (g_variant_iter_loop(connectionIter, "o", &connectionPath)) {
                // Get connection settings to check interface name
                GDBusProxy *connectionProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(connectionPath);
                if (connectionProxy == nullptr) {
                    NMLOG_WARNING("Failed to create proxy for connection during hostname init: %s", connectionPath);
                    continue;
                }

                GVariant *settingsResult = g_dbus_proxy_call_sync(
                        connectionProxy,
                        "GetSettings",
                        nullptr,
                        G_DBUS_CALL_FLAGS_NONE,
                        -1,
                        nullptr,
                        &error);

                if (error || settingsResult == nullptr) {
                    NMLOG_WARNING("Failed to get settings for connection %s during hostname init: %s",
                                  connectionPath, error ? error->message : "unknown error");
                    if (error) {
                        g_error_free(error);
                        error = nullptr;
                    }
                    g_object_unref(connectionProxy);
                    continue;
                }

                // Parse connection settings to get interface name
                GVariantIter *settingsIter;
                const gchar *settingName;
                GVariant *settingDict;
                std::string interfaceName;
                std::string connectionType;

                g_variant_get(settingsResult, "(a{sa{sv}})", &settingsIter);
                while (g_variant_iter_loop(settingsIter, "{&s@a{sv}}", &settingName, &settingDict)) {
                    if (g_strcmp0(settingName, "connection") == 0) {
                        GVariantIter dictIter;
                        const gchar *key;
                        GVariant *value;

                        g_variant_iter_init(&dictIter, settingDict);
                        while (g_variant_iter_loop(&dictIter, "{&sv}", &key, &value)) {
                            if (g_strcmp0(key, "interface-name") == 0) {
                                interfaceName = g_variant_get_string(value, nullptr);
                            } else if (g_strcmp0(key, "type") == 0) {
                                connectionType = g_variant_get_string(value, nullptr);
                            }
                        }
                    }
                }
                g_variant_iter_free(settingsIter);

                // Only process Ethernet and WiFi connections
                bool isValidInterface = false;
                if (!interfaceName.empty()) {
                    if (interfaceName == GnomeUtils::getEthIfname() &&
                        (connectionType == "802-3-ethernet")) {
                        isValidInterface = true;
                    } else if (interfaceName == GnomeUtils::getWifiIfname() &&
                               (connectionType == "802-11-wireless")) {
                        isValidInterface = true;
                    }
                }

                if (isValidInterface) {
                    NMLOG_DEBUG("Initializing hostname for existing connection: %s (interface: %s)",
                                connectionPath, interfaceName.c_str());

                    if (!updateHostnameSettings(m_dbus, std::string(connectionPath), hostname, interfaceName)) {
                        NMLOG_WARNING("Failed to initialize hostname for existing connection: %s", interfaceName.c_str());
                        // Don't fail the whole operation for one connection
                    } else {
                        NMLOG_INFO("Successfully initialized hostname for existing connection: %s", interfaceName.c_str());
                    }
                } else {
                    NMLOG_DEBUG("Skipping hostname init for non-ethernet/wifi connection: %s (type: %s)",
                                interfaceName.c_str(), connectionType.c_str());
                }

                g_variant_unref(settingsResult);
                g_object_unref(connectionProxy);
            }

            g_variant_iter_free(connectionIter);
            g_variant_unref(connectionsResult);
            g_object_unref(settingsProxy);

            NMLOG_INFO("Completed hostname initialization for existing connections");
            return success;
        }

        bool NetworkManagerClient::setHostname(const std::string& hostname)
        {
            NMLOG_DEBUG("Setting DHCP hostname to: %s", hostname.c_str());

            // Validate hostname length (1-32 characters as per NetworkManager specs)
            if(hostname.length() < 1 || hostname.length() > 32)
            {
                NMLOG_ERROR("Invalid hostname length: %zu (must be 1-32 characters)", hostname.length());
                return false;
            }

            GError *error = nullptr;
            GDBusProxy *settingsProxy = m_dbus.getNetworkManagerSettingsProxy();
            if (settingsProxy == nullptr) {
                NMLOG_ERROR("Error creating NetworkManager settings proxy");
                return false;
            }

            // Get all connections
            GVariant *connectionsResult = g_dbus_proxy_call_sync(
                    settingsProxy,
                    "ListConnections",
                    nullptr,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    nullptr,
                    &error);

            if (error || connectionsResult == nullptr) {
                NMLOG_ERROR("Error retrieving connections: %s", error ? error->message : "unknown error");
                if (error) g_error_free(error);
                g_object_unref(settingsProxy);
                return false;
            }

            GVariantIter *connectionIter;
            const gchar *connectionPath;
            bool success = true;

            g_variant_get(connectionsResult, "(ao)", &connectionIter);

            while (g_variant_iter_loop(connectionIter, "o", &connectionPath)) {
                // Get connection settings to check interface name
                GDBusProxy *connectionProxy = m_dbus.getNetworkManagerSettingsConnectionProxy(connectionPath);
                if (connectionProxy == nullptr) {
                    NMLOG_WARNING("Failed to create proxy for connection: %s", connectionPath);
                    continue;
                }

                GVariant *settingsResult = g_dbus_proxy_call_sync(
                        connectionProxy,
                        "GetSettings",
                        nullptr,
                        G_DBUS_CALL_FLAGS_NONE,
                        -1,
                        nullptr,
                        &error);

                if (error || settingsResult == nullptr) {
                    NMLOG_WARNING("Failed to get settings for connection %s: %s",
                                  connectionPath, error ? error->message : "unknown error");
                    if (error) {
                        g_error_free(error);
                        error = nullptr;
                    }
                    g_object_unref(connectionProxy);
                    continue;
                }

                // Parse connection settings to get interface name
                GVariantIter *settingsIter;
                const gchar *settingName;
                GVariant *settingDict;
                std::string interfaceName;
                std::string connectionType;

                g_variant_get(settingsResult, "(a{sa{sv}})", &settingsIter);
                while (g_variant_iter_loop(settingsIter, "{&s@a{sv}}", &settingName, &settingDict)) {
                    if (g_strcmp0(settingName, "connection") == 0) {
                        GVariantIter dictIter;
                        const gchar *key;
                        GVariant *value;

                        g_variant_iter_init(&dictIter, settingDict);
                        while (g_variant_iter_loop(&dictIter, "{&sv}", &key, &value)) {
                            if (g_strcmp0(key, "interface-name") == 0) {
                                interfaceName = g_variant_get_string(value, nullptr);
                            } else if (g_strcmp0(key, "type") == 0) {
                                connectionType = g_variant_get_string(value, nullptr);
                            }
                        }
                    }
                }
                g_variant_iter_free(settingsIter);

                // Only process Ethernet and WiFi connections
                bool isValidInterface = false;
                if (!interfaceName.empty()) {
                    if (interfaceName == GnomeUtils::getEthIfname() &&
                        (connectionType == "802-3-ethernet")) {
                        isValidInterface = true;
                    } else if (interfaceName == GnomeUtils::getWifiIfname() &&
                               (connectionType == "802-11-wireless")) {
                        isValidInterface = true;
                    }
                }

                if (isValidInterface) {
                    NMLOG_DEBUG("Updating hostname for interface: %s (connection: %s)",
                                interfaceName.c_str(), connectionPath);

                    if (!updateHostnameSettings(m_dbus, std::string(connectionPath), hostname, interfaceName)) {
                        NMLOG_ERROR("Failed to update hostname for interface: %s", interfaceName.c_str());
                        success = false;
                    }
                } else {
                    NMLOG_DEBUG("Skipping non-ethernet/wifi connection: %s (type: %s)",
                                interfaceName.c_str(), connectionType.c_str());
                }

                g_variant_unref(settingsResult);
                g_object_unref(connectionProxy);
            }

            g_variant_iter_free(connectionIter);
            g_variant_unref(connectionsResult);
            g_object_unref(settingsProxy);

            if (success) {
                // Write hostname to persistent storage
                GnomeUtils::writePersistentHostname(hostname);
                NMLOG_INFO("Successfully set DHCP hostname to: %s", hostname.c_str());
            }

            return success;
        }

    } // WPEFramework
} // Plugin

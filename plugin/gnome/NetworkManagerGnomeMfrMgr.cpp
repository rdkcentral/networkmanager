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

#include "NetworkManagerGnomeMfrMgr.h"
#include "NetworkManagerLogger.h"
#include "NetworkManagerImplementation.h"
#include "INetworkManager.h"
#include "NetworkManagerGnomeUtils.h"
#include "gdbus/NetworkManagerGdbusMgr.h"
#include "gdbus/NetworkManagerGdbusUtils.h"
#include "mfrMgr.h"
#include <thread>
#include <mutex>

#include <gio/gio.h>
#include "libIBus.h"

namespace WPEFramework
{
    namespace Plugin
    {
        extern NetworkManagerImplementation* _instance;

        static std::mutex iarm_mutex;
        static bool iarm_initialized = false;

        static bool ensureIARMConnection()
        {
            std::lock_guard<std::mutex> lock(iarm_mutex);

            if (!iarm_initialized) {
                IARM_Result_t ret = IARM_Bus_Init("NetworkMfrMgr");
                if (ret != IARM_RESULT_SUCCESS) {
                    NMLOG_ERROR("IARM_Bus_Init failed: %d", ret);
                    return false;
                }

                ret = IARM_Bus_Connect();
                if (ret != IARM_RESULT_SUCCESS) {
                    NMLOG_ERROR("IARM_Bus_Connect failed: %d", ret);
                    return false;
                }

                iarm_initialized = true;
                NMLOG_DEBUG("IARM Bus connection established for MfrMgr");
            }
            return true;
        }

        // Helper function to retrieve current WiFi connection details using GDBus utilities
        static bool getCurrentWiFiConnectionDetails(std::string& ssid, std::string& passphrase, int& security)
        {
            GError *error = NULL;
            GDBusProxy *connection_proxy = NULL;
            GDBusProxy *device_proxy = NULL;
            bool result = false;

            NMLOG_DEBUG("Retrieving WiFi connection details using GDBus utilities");

            // Create DbusMgr instance for proxy management
            DbusMgr dbusMgr;

            // Get WiFi device information using existing utility
            deviceInfo devInfo;
            if (!GnomeUtils::getDeviceInfoByIfname(dbusMgr, nmUtils::wlanIface(), devInfo)) {
                NMLOG_ERROR("WiFi device '%s' not found", nmUtils::wlanIface());
                return false;
            }

            NMLOG_DEBUG("Found WiFi device at path: %s", devInfo.path.c_str());

            // Create device proxy to check state and refresh ActiveConnection property
            device_proxy = dbusMgr.getNetworkManagerDeviceProxy(devInfo.path.c_str());
            if (!device_proxy) {
                NMLOG_ERROR("Failed to create device proxy");
                return false;
            }

            // Check device state first
            GVariant *state_variant = g_dbus_proxy_get_cached_property(device_proxy, "State");
            if (state_variant) {
                guint32 device_state = g_variant_get_uint32(state_variant);
                NMLOG_DEBUG("WiFi device state: %u (100=activated, 30=disconnected)", device_state);
                g_variant_unref(state_variant);

                // NM_DEVICE_STATE_ACTIVATED = 100
                if (device_state != 100) {
                    NMLOG_WARNING("WiFi device not in activated state (state=%u)", device_state);
                }
            }

            // Force refresh ActiveConnection property from D-Bus instead of using cached value
            std::string active_conn_path;
            error = NULL;
            GVariant *props_variant = g_dbus_proxy_call_sync(device_proxy,
                                                             "org.freedesktop.DBus.Properties.Get",
                                                             g_variant_new("(ss)",
                                                             "org.freedesktop.NetworkManager.Device",
                                                             "ActiveConnection"),
                                                             G_DBUS_CALL_FLAGS_NONE,
                                                             -1,
                                                             NULL,
                                                             &error);
            if (!props_variant) {
                NMLOG_ERROR("Failed to get ActiveConnection property: %s", error ? error->message : "unknown");
                if (error) g_error_free(error);
                g_object_unref(device_proxy);
                return false;
            }

            GVariant *active_conn_variant = NULL;
            g_variant_get(props_variant, "(v)", &active_conn_variant);

            if (!active_conn_variant) {
                NMLOG_ERROR("ActiveConnection property variant is NULL");
                g_variant_unref(props_variant);
                g_object_unref(device_proxy);
                return false;
            }

            const gchar *active_conn_path_str = g_variant_get_string(active_conn_variant, NULL);

            if (active_conn_path_str) {
                active_conn_path = active_conn_path_str;
            }

            g_variant_unref(active_conn_variant);
            g_variant_unref(props_variant);
            g_object_unref(device_proxy);

            NMLOG_DEBUG("ActiveConnection path: %s", active_conn_path.c_str());

            // Check if device has an active connection
            if (active_conn_path.empty() || active_conn_path == "/") {
                NMLOG_ERROR("No active connection on WiFi device (path: %s)", active_conn_path.c_str());
                return false;
            }

            // Create active connection proxy using DbusMgr
            GDBusProxy *active_conn_proxy = dbusMgr.getNetworkManagerActiveConnProxy(active_conn_path.c_str());
            if (!active_conn_proxy) {
                NMLOG_ERROR("Failed to create active connection proxy");
                return false;
            }

            // Get connection path from active connection
            GVariant *conn_path_variant = g_dbus_proxy_get_cached_property(active_conn_proxy, "Connection");
            if (!conn_path_variant) {
                NMLOG_ERROR("Failed to get connection path from active connection");
                g_object_unref(active_conn_proxy);
                return false;
            }

            const gchar *connection_path = g_variant_get_string(conn_path_variant, NULL);
            if (connection_path == NULL) {
                NMLOG_ERROR("Connection path variant did not contain a valid string");
                g_variant_unref(conn_path_variant);
                g_object_unref(active_conn_proxy);
                return false;
            }
            NMLOG_DEBUG("Connection settings path: %s", connection_path);

            // Create connection proxy using DbusMgr
            connection_proxy = dbusMgr.getNetworkManagerSettingsConnectionProxy(connection_path);
            g_variant_unref(conn_path_variant);
            g_object_unref(active_conn_proxy);

            if (!connection_proxy) {
                NMLOG_ERROR("Failed to create connection proxy");
                return false;
            }

            // Get connection settings (without secrets)
            GVariant *settings_variant = g_dbus_proxy_call_sync(connection_proxy,
                                                                "GetSettings",
                                                                NULL,
                                                                G_DBUS_CALL_FLAGS_NONE,
                                                                -1,
                                                                NULL,
                                                                &error);
            if (!settings_variant) {
                NMLOG_ERROR("Failed to get connection settings: %s", error ? error->message : "unknown");
                if (error) g_error_free(error);
                g_object_unref(connection_proxy);
                return false;
            }

            // Parse settings to get SSID and security type
            GVariantIter *settings_iter;
            g_variant_get(settings_variant, "(a{sa{sv}})", &settings_iter);

            gchar *setting_name;
            GVariant *setting_dict;
            security = NET_WIFI_SECURITY_NONE;
            bool has_wireless_security = false;

            while (g_variant_iter_loop(settings_iter, "{s@a{sv}}", &setting_name, &setting_dict)) {
                if (g_strcmp0(setting_name, "802-11-wireless") == 0) {
                    // Extract SSID
                    GVariant *ssid_variant = g_variant_lookup_value(setting_dict, "ssid", G_VARIANT_TYPE_BYTESTRING);
                    if (ssid_variant) {
                        gsize ssid_len;
                        const guint8 *ssid_data = static_cast<const guint8*>(g_variant_get_fixed_array(ssid_variant, &ssid_len, sizeof(guint8)));
                        ssid = std::string(reinterpret_cast<const char*>(ssid_data), ssid_len);
                        NMLOG_DEBUG("Retrieved SSID: %s", ssid.c_str());
                        g_variant_unref(ssid_variant);
                    }
                } else if (g_strcmp0(setting_name, "802-11-wireless-security") == 0) {
                    has_wireless_security = true;
                    // Extract key-mgmt
                    GVariant *key_mgmt_variant = g_variant_lookup_value(setting_dict, "key-mgmt", G_VARIANT_TYPE_STRING);
                    if (key_mgmt_variant) {
                        const gchar *key_mgmt = g_variant_get_string(key_mgmt_variant, NULL);
                        NMLOG_DEBUG("Key management: %s", key_mgmt);

                        if (g_strcmp0(key_mgmt, "sae") == 0) {
                            security = NET_WIFI_SECURITY_WPA3_SAE;
                        } else if (g_strcmp0(key_mgmt, "wpa-psk") == 0) {
                            security = NET_WIFI_SECURITY_WPA2_PSK_AES;
                        } else if (g_strcmp0(key_mgmt, "wpa-eap") == 0) {
                            security = NET_WIFI_SECURITY_WPA2_ENTERPRISE_AES;
                        } else if (g_strcmp0(key_mgmt, "none") == 0) {
                            security = NET_WIFI_SECURITY_NONE;
                        } else {
                            security = NET_WIFI_SECURITY_WPA2_PSK_AES;
                        }

                        g_variant_unref(key_mgmt_variant);
                    }
                }
            }

            g_variant_iter_free(settings_iter);
            g_variant_unref(settings_variant);

            // Request secrets if PSK-based security
            if (has_wireless_security && (security == NET_WIFI_SECURITY_WPA2_PSK_AES || security == NET_WIFI_SECURITY_WPA3_SAE)) {
                NMLOG_DEBUG("Requesting secrets for PSK-based connection");

                // Use synchronous call instead of async to avoid cancellation complexity
                error = NULL;
                GVariant *secrets = g_dbus_proxy_call_sync(connection_proxy,
                                                           "GetSecrets",
                                                           g_variant_new("(s)", "802-11-wireless-security"),
                                                           G_DBUS_CALL_FLAGS_NONE,
                                                           5000, // 5 second timeout
                                                           NULL,
                                                           &error);

                if (error) {
                    NMLOG_WARNING("Failed to get secrets: %s", error->message);
                    g_error_free(error);
                    result = false;
                } else if (secrets) {
                    // Parse the returned secrets
                    // GetSecrets returns (a{sa{sv}}) - dict of dicts
                    GVariantIter *iter;
                    g_variant_get(secrets, "(a{sa{sv}})", &iter);

                    gchar *setting_name_key;
                    GVariant *setting_dict_value;
                    bool found_psk = false;

                    // Iterate through settings
                    while (g_variant_iter_loop(iter, "{s@a{sv}}", &setting_name_key, &setting_dict_value)) {
                        if (g_strcmp0(setting_name_key, "802-11-wireless-security") == 0) {
                            // Found wireless security settings
                            GVariantIter *setting_iter;
                            g_variant_get(setting_dict_value, "a{sv}", &setting_iter);

                            gchar *key;
                            GVariant *value;

                            // Look for psk key
                            while (g_variant_iter_loop(setting_iter, "{sv}", &key, &value)) {
                                if (g_strcmp0(key, "psk") == 0) {
                                    const gchar *psk = g_variant_get_string(value, NULL);
                                    if (psk) {
                                        passphrase = psk;
                                        NMLOG_DEBUG("Successfully retrieved PSK");
                                        found_psk = true;
                                    }
                                    break;
                                }
                            }
                            g_variant_iter_free(setting_iter);
                            break;
                        }
                    }

                    g_variant_iter_free(iter);
                    g_variant_unref(secrets);
                    result = found_psk;
                } else {
                    NMLOG_WARNING("GetSecrets returned NULL");
                    result = false;
                }
            } else {
                // No secrets needed for open networks
                result = true;
            }

            // Cleanup
            g_object_unref(connection_proxy);

            NMLOG_INFO("Retrieved WiFi connection details - SSID: %s, Security: %d, Has Passphrase: %s",
                       ssid.c_str(), security, passphrase.empty() ? "no" : "yes");

            return result && !ssid.empty();
        }

        NetworkManagerMfrManager* NetworkManagerMfrManager::getInstance()
        {
            static NetworkManagerMfrManager instance;
            return &instance;
        }

        NetworkManagerMfrManager::NetworkManagerMfrManager()
        {
            // Initialize IARM connection
            if (ensureIARMConnection()) {
                NMLOG_DEBUG("NetworkManagerMfrManager initialized with IARM connection");
            } else {
                NMLOG_ERROR("NetworkManagerMfrManager initialization failed - IARM connection error");
            }
        }

        NetworkManagerMfrManager::~NetworkManagerMfrManager()
        {
            std::lock_guard<std::mutex> lock(iarm_mutex);
            if (iarm_initialized) {
                iarm_initialized = false;
                NMLOG_DEBUG("IARM Bus connection terminated");
            }
            NMLOG_DEBUG("NetworkManagerMfrManager destroyed");
        }

        bool NetworkManagerMfrManager::saveWiFiSettingsToMfr()
        {
            // Launch asynchronous operation to save current WiFi connection details
            std::thread saveThread([this]() {
                std::string ssid, passphrase;
                int security;

                if (!getCurrentWiFiConnectionDetails(ssid, passphrase, security)) {
                    NMLOG_ERROR("Failed to retrieve current WiFi connection details for MfrMgr save");
                    return;
                }

                NMLOG_INFO("Retrieved current WiFi connection details for MfrMgr save - SSID: %s, Security: %d", ssid.c_str(), security);

                // Save the retrieved details synchronously
                bool result = this->saveWiFiSettingsToMfrSync(ssid, passphrase, security);
                if (result) {
                    NMLOG_DEBUG("Background WiFi connection details retrieval and save completed successfully for SSID: %s", ssid.c_str());
                } else {
                    NMLOG_ERROR("Background WiFi connection details retrieval and save failed for SSID: %s", ssid.c_str());
                }
            });

            // Detach the thread to run independently
            saveThread.detach();

            NMLOG_DEBUG("WiFi connection details retrieval and save operation queued for background execution");
            return true; // Return immediately, actual retrieval and save happens asynchronously
        }

        bool NetworkManagerMfrManager::saveWiFiSettingsToMfrSync(const std::string& ssid, const std::string& passphrase, int security)
        {
            int securityMode;
            if (!ensureIARMConnection()) {
                NMLOG_ERROR("IARM connection not available for saving WiFi settings");
                return false;
            }

            NMLOG_INFO("Saving WiFi settings to MfrMgr via IARM - SSID: %s, Security: %d", ssid.c_str(), security);

            IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t param{0};
            IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t setParam{0};
            IARM_Result_t ret;
            param.requestType = WIFI_GET_CREDENTIALS;
            ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_Credentials,
                                (void*)&param, sizeof(param));
            if(security == Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_NONE)
                securityMode = NET_WIFI_SECURITY_NONE;
            else if(security == Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE)
                securityMode = NET_WIFI_SECURITY_WPA3_SAE;
            else
                securityMode = NET_WIFI_SECURITY_WPA2_PSK_AES;
            if (ret == IARM_RESULT_SUCCESS)
            {
                if ((strcmp (param.wifiCredentials.cSSID, ssid.c_str()) == 0) &&
                (strcmp (param.wifiCredentials.cPassword, passphrase.c_str()) == 0) &&
                (param.wifiCredentials.iSecurityMode == securityMode))
                {
                    NMLOG_INFO("Same ssid info not storing it stored ssid %s new ssid %s", param.wifiCredentials.cSSID, ssid.c_str());
                    return true;
                }
                else
                {
                    NMLOG_INFO("ssid info is different continue to store ssid %s new ssid %s", param.wifiCredentials.cSSID, ssid.c_str());
                }
            }

            // Copy SSID
            strncpy(setParam.wifiCredentials.cSSID, ssid.c_str(), sizeof(setParam.wifiCredentials.cSSID));

            // Copy passphrase - only if not empty
            if (!passphrase.empty()) {
                strncpy(setParam.wifiCredentials.cPassword, passphrase.c_str(), sizeof(setParam.wifiCredentials.cPassword));
                NMLOG_DEBUG("WiFi passphrase set for MfrMgr save");
            } else {
                param.wifiCredentials.cPassword[0] = '\0'; // Ensure empty string
                NMLOG_DEBUG("Empty passphrase - setting empty string in MfrMgr");
            }

            setParam.wifiCredentials.iSecurityMode = securityMode;

            setParam.requestType = WIFI_SET_CREDENTIALS;
            NMLOG_INFO(" Set Params param.requestType = %d, param.wifiCredentials.cSSID = %s, param.wifiCredentials.iSecurityMode = %d", setParam.requestType, setParam.wifiCredentials.cSSID, setParam.wifiCredentials.iSecurityMode);
            // Make IARM Bus call to save credentials
            ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_Credentials,
                                (void*)&setParam, sizeof(setParam));
            if(ret == IARM_RESULT_SUCCESS)
            {
                memset(&param,0,sizeof(param));
                param.requestType = WIFI_GET_CREDENTIALS;
                ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_Credentials, (void*)&param, sizeof(param));
                if(security == Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_NONE)
                    securityMode = NET_WIFI_SECURITY_NONE;
                else if(security == Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE)
                    securityMode = NET_WIFI_SECURITY_WPA3_SAE;
                else
                    securityMode = NET_WIFI_SECURITY_WPA2_PSK_AES;

                if (ret == IARM_RESULT_SUCCESS)
                {
                    if ((strcmp (param.wifiCredentials.cSSID, ssid.c_str()) == 0) &&
                        (strcmp (param.wifiCredentials.cPassword, passphrase.c_str()) == 0) &&
                        (param.wifiCredentials.iSecurityMode == securityMode))
                    {
                        NMLOG_INFO("Successfully stored the credentials and verified stored ssid %s current ssid %s and security_mode %d", param.wifiCredentials.cSSID, ssid.c_str(), param.wifiCredentials.iSecurityMode);
                        return true;
                    }
                    else
                    {
                        NMLOG_INFO("failure in storing wifi credentials ssid %s", ssid.c_str());
                    }
                }
            }
            else
            {
                NMLOG_ERROR("IARM Bus call failed for WiFi credentials save: %d", ret);
                return false;
            }

            NMLOG_INFO("Successfully saved WiFi settings to MfrMgr via IARM - SSID: %s, Security: %d", ssid.c_str(), security);
            return true;
        }

        bool NetworkManagerMfrManager::clearWiFiSettingsFromMfr()
        {
            if (!ensureIARMConnection()) {
                NMLOG_ERROR("IARM connection not available for clearing WiFi settings");
                return false;
            }

            NMLOG_DEBUG("Clearing WiFi settings from MfrMgr via IARM");

            // Make IARM Bus call to clear credentials
            IARM_Result_t ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,IARM_BUS_MFRLIB_API_WIFI_EraseAllData,0,0);
            if (ret != IARM_RESULT_SUCCESS) {
                NMLOG_ERROR("IARM Bus call failed for WiFi credentials clear: %d", ret);
                return false;
            }

            NMLOG_INFO("Successfully cleared WiFi settings from MfrMgr via IARM");
            return true;
        }

    } // namespace Plugin
} // namespace WPEFramework

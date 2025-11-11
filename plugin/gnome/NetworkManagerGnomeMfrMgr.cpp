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
#include "mfrMgr.h"
#include <thread>
#include <mutex>
#include <condition_variable>

#include <gio/gio.h>
#include "libIBus.h"

// NetworkManager D-Bus constants
#define NM_DBUS_SERVICE                     "org.freedesktop.NetworkManager"
#define NM_DBUS_PATH                        "/org/freedesktop/NetworkManager"
#define NM_DBUS_INTERFACE                   "org.freedesktop.NetworkManager"
#define NM_DBUS_INTERFACE_DEVICE            "org.freedesktop.NetworkManager.Device"
#define NM_DBUS_INTERFACE_DEVICE_WIRELESS   "org.freedesktop.NetworkManager.Device.Wireless"
#define NM_DBUS_INTERFACE_SETTINGS          "org.freedesktop.NetworkManager.Settings"
#define NM_DBUS_INTERFACE_CONNECTION        "org.freedesktop.NetworkManager.Settings.Connection"
#define NM_DBUS_INTERFACE_ACTIVE_CONNECTION "org.freedesktop.NetworkManager.Connection.Active"
#define NM_DBUS_PATH_SETTINGS               "/org/freedesktop/NetworkManager/Settings"

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

        // Structure to hold secrets retrieval context
        struct SecretsContext {
            std::string* passphrase;
            bool* completed;
            bool* success;
            std::mutex* mutex;
            std::condition_variable* cv;
        };

        // Callback for async secrets retrieval via GDBus
        static void secrets_callback(GObject *source, GAsyncResult *result, gpointer user_data)
        {
            SecretsContext* context = static_cast<SecretsContext*>(user_data);
            GError *error = NULL;
            GDBusProxy *proxy = G_DBUS_PROXY(source);

            // Call the GetSecrets method finish
            GVariant *secrets = g_dbus_proxy_call_finish(proxy, result, &error);

            if (error) {
                NMLOG_WARNING("Failed to get secrets via GDBus: %s", error->message);
                g_error_free(error);
                *(context->success) = false;
            } else if (secrets) {
                // Parse the returned secrets
                // GetSecrets returns (a{sa{sv}}) - dict of dicts
                GVariantIter *iter;
                g_variant_get(secrets, "(a{sa{sv}})", &iter);

                gchar *setting_name;
                GVariant *setting_dict;

                // Iterate through settings
                while (g_variant_iter_loop(iter, "{s@a{sv}}", &setting_name, &setting_dict)) {
                    if (g_strcmp0(setting_name, "802-11-wireless-security") == 0) {
                        // Found wireless security settings
                        GVariantIter *setting_iter;
                        g_variant_get(setting_dict, "a{sv}", &setting_iter);

                        gchar *key;
                        GVariant *value;

                        // Look for psk key
                        while (g_variant_iter_loop(setting_iter, "{sv}", &key, &value)) {
                            if (g_strcmp0(key, "psk") == 0) {
                                const gchar *psk = g_variant_get_string(value, NULL);
                                if (psk && context->passphrase) {
                                    *(context->passphrase) = psk;
                                    NMLOG_DEBUG("Successfully retrieved PSK via GDBus");
                                    *(context->success) = true;
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
            }

            // Signal completion
            {
                std::lock_guard<std::mutex> lock(*(context->mutex));
                *(context->completed) = true;
            }
            context->cv->notify_one();
        }

        // Helper function to retrieve current WiFi connection details using GDBus
        static bool getCurrentWiFiConnectionDetails(std::string& ssid, std::string& passphrase, int& security)
        {
            GError *error = NULL;
            GDBusConnection *bus = NULL;
            GDBusProxy *nm_proxy = NULL;
            GDBusProxy *device_proxy = NULL;
            GDBusProxy *connection_proxy = NULL;
            bool result = false;

            NMLOG_DEBUG("Retrieving WiFi connection details using GDBus API");

            // Connect to system bus
            bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
            if (!bus) {
                NMLOG_ERROR("Failed to connect to system bus: %s", error ? error->message : "unknown");
                if (error) g_error_free(error);
                return false;
            }
            
            // Create NetworkManager proxy
            nm_proxy = g_dbus_proxy_new_sync(bus,
                                             G_DBUS_PROXY_FLAGS_NONE,
                                             NULL,
                                             NM_DBUS_SERVICE,
                                             NM_DBUS_PATH,
                                             NM_DBUS_INTERFACE,
                                             NULL,
                                             &error);
            if (!nm_proxy) {
                NMLOG_ERROR("Failed to create NetworkManager proxy: %s", error ? error->message : "unknown");
                if (error) g_error_free(error);
                g_object_unref(bus);
                return false;
            }
            
            // Get all devices
            GVariant *devices_variant = g_dbus_proxy_call_sync(nm_proxy,
                                                               "GetDevices",
                                                               NULL,
                                                               G_DBUS_CALL_FLAGS_NONE,
                                                               -1,
                                                               NULL,
                                                               &error);
            if (!devices_variant) {
                NMLOG_ERROR("Failed to get devices: %s", error ? error->message : "unknown");
                if (error) g_error_free(error);
                g_object_unref(nm_proxy);
                g_object_unref(bus);
                return false;
            }

            // Find WiFi device
            GVariantIter *devices_iter;
            const gchar *device_path = NULL;
            const gchar *wifi_device_path = NULL;
            g_variant_get(devices_variant, "(ao)", &devices_iter);

            while (g_variant_iter_loop(devices_iter, "&o", &device_path)) {
                // Create device proxy to check interface name
                GDBusProxy *temp_device_proxy = g_dbus_proxy_new_sync(bus,
                                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                                       NULL,
                                                                       NM_DBUS_SERVICE,
                                                                       device_path,
                                                                       NM_DBUS_INTERFACE_DEVICE,
                                                                       NULL,
                                                                       NULL);
                if (temp_device_proxy) {
                    GVariant *iface_variant = g_dbus_proxy_get_cached_property(temp_device_proxy, "Interface");
                    GVariant *device_type_variant = g_dbus_proxy_get_cached_property(temp_device_proxy, "DeviceType");

                    if (iface_variant && device_type_variant) {
                        const gchar *iface = g_variant_get_string(iface_variant, NULL);
                        guint32 device_type = g_variant_get_uint32(device_type_variant);

                        // DeviceType 2 = NM_DEVICE_TYPE_WIFI
                        if (device_type == 2 && g_strcmp0(iface, nmUtils::wlanIface()) == 0) {
                            wifi_device_path = g_strdup(device_path);
                            g_variant_unref(iface_variant);
                            g_variant_unref(device_type_variant);
                            g_object_unref(temp_device_proxy);
                            break;
                        }

                        g_variant_unref(iface_variant);
                        g_variant_unref(device_type_variant);
                    }
                    g_object_unref(temp_device_proxy);
                }
            }

            g_variant_iter_free(devices_iter);
            g_variant_unref(devices_variant);

            if (!wifi_device_path) {
                NMLOG_ERROR("WiFi device '%s' not found", nmUtils::wlanIface());
                g_object_unref(nm_proxy);
                g_object_unref(bus);
                return false;
            }

            NMLOG_DEBUG("Found WiFi device at path: %s", wifi_device_path);

            // Create WiFi device proxy
            device_proxy = g_dbus_proxy_new_sync(bus,
                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                 NULL,
                                                 NM_DBUS_SERVICE,
                                                 wifi_device_path,
                                                 NM_DBUS_INTERFACE_DEVICE,
                                                 NULL,
                                                 &error);
            if (!device_proxy) {
                NMLOG_ERROR("Failed to create device proxy: %s", error ? error->message : "unknown");
                if (error) g_error_free(error);
                g_free((gpointer)wifi_device_path);
                g_object_unref(nm_proxy);
                g_object_unref(bus);
                return false;
            }

            // Get active connection
            GVariant *active_conn_variant = g_dbus_proxy_get_cached_property(device_proxy, "ActiveConnection");
            if (!active_conn_variant) {
                NMLOG_ERROR("No active WiFi connection");
                g_free((gpointer)wifi_device_path);
                g_object_unref(device_proxy);
                g_object_unref(nm_proxy);
                g_object_unref(bus);
                return false;
            }

            const gchar *active_conn_path = g_variant_get_string(active_conn_variant, NULL);
            if (!active_conn_path || g_strcmp0(active_conn_path, "/") == 0) {
                NMLOG_ERROR("No active connection on WiFi device");
                g_variant_unref(active_conn_variant);
                g_free((gpointer)wifi_device_path);
                g_object_unref(device_proxy);
                g_object_unref(nm_proxy);
                g_object_unref(bus);
                return false;
            }

            NMLOG_DEBUG("Active connection path: %s", active_conn_path);

            // Create active connection proxy
            GDBusProxy *active_conn_proxy = g_dbus_proxy_new_sync(bus,
                                                                   G_DBUS_PROXY_FLAGS_NONE,
                                                                   NULL,
                                                                   NM_DBUS_SERVICE,
                                                                   active_conn_path,
                                                                   NM_DBUS_INTERFACE_ACTIVE_CONNECTION,
                                                                   NULL,
                                                                   &error);
            g_variant_unref(active_conn_variant);

            if (!active_conn_proxy) {
                NMLOG_ERROR("Failed to create active connection proxy: %s", error ? error->message : "unknown");
                if (error) g_error_free(error);
                g_free((gpointer)wifi_device_path);
                g_object_unref(device_proxy);
                g_object_unref(nm_proxy);
                g_object_unref(bus);
                return false;
            }

            // Get connection path
            GVariant *conn_path_variant = g_dbus_proxy_get_cached_property(active_conn_proxy, "Connection");
            if (!conn_path_variant) {
                NMLOG_ERROR("Failed to get connection path from active connection");
                g_object_unref(active_conn_proxy);
                g_free((gpointer)wifi_device_path);
                g_object_unref(device_proxy);
                g_object_unref(nm_proxy);
                g_object_unref(bus);
                return false;
            }

            const gchar *connection_path = g_variant_get_string(conn_path_variant, NULL);
            NMLOG_DEBUG("Connection settings path: %s", connection_path);

            // Create connection proxy
            connection_proxy = g_dbus_proxy_new_sync(bus,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     NULL,
                                                     NM_DBUS_SERVICE,
                                                     connection_path,
                                                     NM_DBUS_INTERFACE_CONNECTION,
                                                     NULL,
                                                     &error);
            g_variant_unref(conn_path_variant);
            g_object_unref(active_conn_proxy);

            if (!connection_proxy) {
                NMLOG_ERROR("Failed to create connection proxy: %s", error ? error->message : "unknown");
                if (error) g_error_free(error);
                g_free((gpointer)wifi_device_path);
                g_object_unref(device_proxy);
                g_object_unref(nm_proxy);
                g_object_unref(bus);
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
                g_free((gpointer)wifi_device_path);
                g_object_unref(device_proxy);
                g_object_unref(nm_proxy);
                g_object_unref(bus);
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

                // Setup synchronization
                bool completed = false;
                bool secrets_success = false;
                std::mutex mtx;
                std::condition_variable cv;
                SecretsContext context = { &passphrase, &completed, &secrets_success, &mtx, &cv };

                // Call GetSecrets asynchronously
                g_dbus_proxy_call(connection_proxy,
                                 "GetSecrets",
                                 g_variant_new("(s)", "802-11-wireless-security"),
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 secrets_callback,
                                 &context);

                // Wait for callback with timeout (5 seconds)
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    if (cv.wait_for(lock, std::chrono::seconds(5), [&completed] { return completed; })) {
                        if (secrets_success) {
                            NMLOG_DEBUG("Secrets retrieval completed successfully");
                            result = true;
                        } else {
                            NMLOG_WARNING("Secrets retrieval completed but failed to extract PSK");
                            result = false;
                        }
                    } else {
                        NMLOG_WARNING("Timeout waiting for secrets retrieval");
                        result = false;
                    }
                }
            } else {
                // No secrets needed for open networks
                result = true;
            }

            // Cleanup
            g_object_unref(connection_proxy);
            g_free((gpointer)wifi_device_path);
            g_object_unref(device_proxy);
            g_object_unref(nm_proxy);
            g_object_unref(bus);

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
            NMLOG_INFO(" Set Params param.requestType = %d, param.wifiCredentials.cSSID = %s, param.wifiCredentials.cPassword = %s, param.wifiCredentials.iSecurityMode = %d", setParam.requestType, setParam.wifiCredentials.cSSID, setParam.wifiCredentials.cPassword, setParam.wifiCredentials.iSecurityMode); 
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
                        NMLOG_INFO("Successfully stored the credentails and verified stored ssid %s current ssid %s and security_mode %d", param.wifiCredentials.cSSID, ssid.c_str(), param.wifiCredentials.iSecurityMode);
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

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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

#include <glib.h>
#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include "NetworkManagerLogger.h"
#include "INetworkManager.h"
#include "NetworkManagerGnomeWIFI.h"
#include "NetworkManagerGnomeUtils.h"
#include "NetworkManagerImplementation.h"

using namespace std;
namespace WPEFramework
{

    namespace Plugin
    {
        extern NetworkManagerImplementation* _instance;
        static std::atomic<bool> wpsProcessRun = {false};

        wifiManager::wifiManager() : m_client(nullptr), m_loop(nullptr), m_createNewConnection(false) {
            NMLOG_INFO("wifiManager");
            m_nmContext = g_main_context_new();
            g_main_context_push_thread_default(m_nmContext);
            m_loop = g_main_loop_new(m_nmContext, FALSE);
        }

        bool wifiManager::createClientNewConnection()
        {
            GError *error = NULL;

            m_client = nm_client_new(NULL, &error);
            if (!m_client || !m_loop) {
                NMLOG_ERROR("Could not connect to NetworkManager: %s.", error->message);
                g_error_free(error);
                return false;
            }
            return true;
        }

        void wifiManager::deleteClientConnection()
        {
            if(m_client != NULL) {
                GMainContext *context = g_main_context_ref(nm_client_get_main_context(m_client));
                GObject *contextBusyWatcher = nm_client_get_context_busy_watcher(m_client);
                g_object_add_weak_pointer(contextBusyWatcher,(gpointer *) &contextBusyWatcher);
                g_clear_object(&m_client);
                while (contextBusyWatcher)
                {
                    g_main_context_iteration(context, TRUE);
                }
                g_main_context_unref(context);
                m_client = NULL;
            }
        }

        bool wifiManager::quit(NMDevice *wifiNMDevice)
        {
            if(!g_main_loop_is_running(m_loop)) {
                NMLOG_ERROR("g_main_loop_is not running");
                return false;
            }

            g_main_loop_quit(m_loop);
            return false;
        }

        static gboolean gmainLoopTimoutCB(gpointer user_data)
        {
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            NMLOG_WARNING("GmainLoop ERROR_TIMEDOUT");
            _wifiManager->m_isSuccess = false;
            g_main_loop_quit(_wifiManager->m_loop);
            return true;
        }
    
        bool wifiManager::wait(GMainLoop *loop, int timeOutMs)
        {
            if(g_main_loop_is_running(loop)) {
                NMLOG_WARNING("g_main_loop_is running");
                return false;
            }
            m_source = g_timeout_source_new(timeOutMs);  // 10000ms interval
            g_source_set_callback(m_source, (GSourceFunc)gmainLoopTimoutCB, this, NULL);
            g_source_attach(m_source, NULL);
            g_main_loop_run(loop);
            if(m_source != nullptr) {
                if(g_source_is_destroyed(m_source)) {
                    NMLOG_WARNING("Source has been destroyed");
                }
                else {
                    g_source_destroy(m_source);
                }
                g_source_unref(m_source);
            }
            return true;
        }

        NMDevice* wifiManager::getWifiDevice()
        {
            NMDevice *wifiDevice = NULL;

            GPtrArray *devices = const_cast<GPtrArray *>(nm_client_get_devices(m_client));
            if (devices == NULL) {
                NMLOG_ERROR("Failed to get device list.");
                return wifiDevice;
            }

            for (guint j = 0; j < devices->len; j++)
            {
                NMDevice *device = NM_DEVICE(devices->pdata[j]);
                const char* interface = nm_device_get_iface(device);
                if(interface == nullptr)
                    continue;
                std::string iface = interface;
                if (iface == nmUtils::wlanIface())
                {
                    wifiDevice = device;
                    //NMLOG_DEBUG("Wireless Device found ifce : %s !", nm_device_get_iface (wifiDevice));
                    break;
                }
            }

            if (wifiDevice == NULL || !NM_IS_DEVICE_WIFI(wifiDevice))
            {
                NMLOG_ERROR("Wireless Device not found !");
                return NULL;
            }

            NMDeviceState deviceState = NM_DEVICE_STATE_UNKNOWN;
            deviceState = nm_device_get_state(wifiDevice);
            switch (deviceState)
            {
                case NM_DEVICE_STATE_UNKNOWN:
                case NM_DEVICE_STATE_UNMANAGED:
                case NM_DEVICE_STATE_UNAVAILABLE:
                     NMLOG_WARNING("wifi device state is not vallied; state: (%d)", deviceState);
                     return NULL;
                break;
            default:
                break;
            }
            return wifiDevice;
        }

        bool static getConnectedSSID(NMDevice *device, std::string& ssidin)
        {
            GBytes *ssid = nullptr;
            NMDeviceState deviceState = nm_device_get_state(device);
            if (deviceState == NM_DEVICE_STATE_ACTIVATED)
            {
                NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(device));
                if(activeAP == nullptr)
                    return false;
                ssid = nm_access_point_get_ssid(activeAP);
                if(ssid)
                {
                    char* ssidStr = nm_utils_ssid_to_utf8((const guint8*)g_bytes_get_data(ssid, NULL), g_bytes_get_size(ssid));
                    if(ssidStr != nullptr)
                    {
                        ssidin = ssidStr;
                        free(ssidStr);
                    }
                    else
                    {
                        NMLOG_ERROR("Invalid ssid length Error");
                        ssidin.clear();
                        return false;
                    }
                }
                NMLOG_INFO("connected ssid: %s", ssidin.c_str());
                return true;
            }
            return false;
        }

        static void getApInfo(NMAccessPoint *AccessPoint, Exchange::INetworkManager::WiFiSSIDInfo &wifiInfo, bool doPrint = true)
        {
            guint32 flags, wpaFlags, rsnFlags, freq, bitrate;
            guint8 strength;
            gint16 noise;
            GBytes *ssid = NULL;
            const char *hwaddr = NULL;
            NM80211Mode mode;
            /* Get AP properties */
            flags     = nm_access_point_get_flags(AccessPoint);
            wpaFlags = nm_access_point_get_wpa_flags(AccessPoint);
            rsnFlags = nm_access_point_get_rsn_flags(AccessPoint);
            ssid      = nm_access_point_get_ssid(AccessPoint);
            hwaddr    = nm_access_point_get_bssid(AccessPoint);
            freq      = nm_access_point_get_frequency(AccessPoint);
            mode      = nm_access_point_get_mode(AccessPoint);
            bitrate   = nm_access_point_get_max_bitrate(AccessPoint);
            strength  = nm_access_point_get_strength(AccessPoint);
            noise     = 0; /* ToDo: Returning as 0 as of now. Need to fetch actual noise value */

            /* Convert to strings */
            if (ssid)
            {
                char* ssidStr = nm_utils_ssid_to_utf8((const guint8*)g_bytes_get_data(ssid, NULL), g_bytes_get_size(ssid));
                if(ssidStr != nullptr)
                {
                    wifiInfo.ssid = ssidStr;
                    free(ssidStr);
                }
                else
                {
                    NMLOG_ERROR("Invalid ssid length Error");
                    wifiInfo.ssid.clear();
                    return;
                }
            }
            else
            {
                /* hidden ssid */
                wifiInfo.ssid.clear();
                NMLOG_DEBUG("hidden ssid found bssid: %s", hwaddr);
            }

            wifiInfo.bssid = (hwaddr != nullptr) ? hwaddr : "-----";
            std::string freqStr = std::to_string((double)freq/1000);
            wifiInfo.frequency = freqStr.substr(0, 5);

            wifiInfo.rate = std::to_string(bitrate);
            wifiInfo.noise = std::to_string(noise);
            NMLOG_DEBUG("bitrate : %s kbit/s", wifiInfo.rate.c_str());
            //TODO signal strenght to dBm
            wifiInfo.strength = std::string(nmUtils::convertPercentageToSignalStrengtStr(strength));
            wifiInfo.security = static_cast<Exchange::INetworkManager::WIFISecurityMode>(nmUtils::wifiSecurityModeFromAp(wifiInfo.ssid, flags, wpaFlags, rsnFlags, doPrint));
            if(doPrint)
            {
                NMLOG_INFO("ssid: %s, frequency: %s, sterngth: %s, security: %u", wifiInfo.ssid.c_str(), wifiInfo.frequency.c_str(), wifiInfo.strength.c_str(), wifiInfo.security);
                NMLOG_DEBUG("Mode: %s", mode == NM_802_11_MODE_ADHOC   ? "Ad-Hoc": mode == NM_802_11_MODE_INFRA ? "Infrastructure": "Unknown");
            }
            else
            {
                NMLOG_DEBUG("ssid: %s, frequency: %s, sterngth: %s, security: %u", wifiInfo.ssid.c_str(), wifiInfo.frequency.c_str(), wifiInfo.strength.c_str(), wifiInfo.security);
                NMLOG_DEBUG("Mode: %s", mode == NM_802_11_MODE_ADHOC   ? "Ad-Hoc": mode == NM_802_11_MODE_INFRA ? "Infrastructure": "Unknown");
            }
        }

        bool wifiManager::getWifiState(Exchange::INetworkManager::WiFiState& state)
        {
            if(!createClientNewConnection())
                return false;

            NMDevice *wifiDevice = getWifiDevice();
            if(wifiDevice == NULL) {
                NMLOG_FATAL("NMDeviceWifi * NULL !");
                deleteClientConnection();
                return false;
            }
            NMDeviceState deviceState = nm_device_get_state(wifiDevice);
            // Todo check NMDeviceStateReason for more information
            switch(deviceState)
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
                case NM_DEVICE_STATE_DISCONNECTED:
                case NM_DEVICE_STATE_UNAVAILABLE:
                    state = Exchange::INetworkManager::WiFiState::WIFI_STATE_DISCONNECTED;
                    break;
                default:
                    state = Exchange::INetworkManager::WiFiState::WIFI_STATE_DISABLED;
            }

            NMLOG_INFO("wifi state (%d) mapped state (%d) ", (int)deviceState, (int)state);
            deleteClientConnection();
            return true;
        }

        bool wifiManager::wifiConnectedSSIDInfo(Exchange::INetworkManager::WiFiSSIDInfo &ssidinfo)
        {
            if(!createClientNewConnection())
                return false;

            NMDevice* wifiDevice = getWifiDevice();
            if(wifiDevice == NULL) {
                NMLOG_FATAL("NMDeviceWifi * NULL !");
                deleteClientConnection();
                return false;
            }

            NMDeviceState deviceState = nm_device_get_state(wifiDevice);
            if(deviceState >= NM_DEVICE_STATE_IP_CONFIG)
            {
                NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(wifiDevice));
                if(activeAP == NULL) {
                    NMLOG_ERROR("NMAccessPoint = NULL !");
                    deleteClientConnection();
                    return false;
                }
                NMLOG_DEBUG("active access point found !");
                getApInfo(activeAP, ssidinfo, false);
                deleteClientConnection();
                return true;
            }
            else
                NMLOG_WARNING("no active access point!; wifi device state: (%d)", deviceState);

            deleteClientConnection();
            return true;
        }

        static void disconnectCb(GObject *object, GAsyncResult *result, gpointer user_data)
        {
            NMDevice     *device = NM_DEVICE(object);
            GError       *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));

            NMLOG_DEBUG("Disconnecting... ");
            _wifiManager->m_isSuccess = true;
            if (!nm_device_disconnect_finish(device, result, &error)) {
                if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                    return;

                NMLOG_ERROR("Device '%s' (%s) disconnecting failed: %s",
                            nm_device_get_iface(device),
                            nm_object_get_path(NM_OBJECT(device)),
                            error->message);
                g_error_free(error);
                _wifiManager->quit(device);
                 _wifiManager->m_isSuccess = false;
            }
            _wifiManager->quit(device);
        }

        bool wifiManager::wifiDisconnect()
        {
            NMDeviceState deviceState = NM_DEVICE_STATE_UNKNOWN;
            if(!createClientNewConnection())
                return false;

            NMDevice *wifiNMDevice = getWifiDevice();
            if(wifiNMDevice == NULL) {
                NMLOG_WARNING("wifi state is unmanaged !");
                deleteClientConnection();
                return true;
            }

            NMLOG_DEBUG("wifi device current state is %d !", deviceState);
            deviceState = nm_device_get_state(wifiNMDevice);
            if (deviceState <= NM_DEVICE_STATE_DISCONNECTED || deviceState == NM_DEVICE_STATE_FAILED || deviceState == NM_DEVICE_STATE_DEACTIVATING)
            {
                NMLOG_WARNING("wifi already disconnected !");
                deleteClientConnection();
                return true;
            }

            nm_device_disconnect_async(wifiNMDevice, NULL, disconnectCb, this);
            wait(m_loop);
            deleteClientConnection();
            return m_isSuccess;
        }

        static NMAccessPoint* findMatchingSSID(const GPtrArray* ApList, std::string& ssid)
        {
            NMAccessPoint *AccessPoint = nullptr;
            if(ssid.empty())
                return nullptr;

            for (guint i = 0; i < ApList->len; i++)
            {
                std::string ssidstr{};
                NMAccessPoint *ap = static_cast<NMAccessPoint *>(g_ptr_array_index(ApList, i));
                GBytes *ssidGBytes = nm_access_point_get_ssid(ap);
                if(ssidGBytes)
                {
                    char* ssidUtf8 = nm_utils_ssid_to_utf8((const guint8*)g_bytes_get_data(ssidGBytes, NULL), g_bytes_get_size(ssidGBytes));
                    if(ssidUtf8 != nullptr)
                    {
                        ssidstr = ssidUtf8;
                        if(ssid == ssidstr)
                        {
                            AccessPoint = ap;
                            free(ssidUtf8);
                            break;
                        }
                        // NMLOG_DEBUG("ssid <  %s  >", ssidstr.c_str());
                    }
                    else
                        NMLOG_WARNING("Invalid ssid length Error");

                    if(ssidUtf8 != nullptr)
                        free(ssidUtf8);
                }
            }

            return AccessPoint;
        }

        static void wifiConnectCb(GObject *client, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            NMActiveConnection *activeConnection = NULL;

            if (_wifiManager->m_createNewConnection) {
                NMLOG_DEBUG("nm_client_add_and_activate_connection_finish");
                activeConnection = nm_client_add_and_activate_connection_finish(NM_CLIENT(_wifiManager->m_client), result, &error);
                 _wifiManager->m_isSuccess = true;
            }
            else {
                NMLOG_DEBUG("nm_client_activate_connection_finish ");
                activeConnection = nm_client_activate_connection_finish(NM_CLIENT(_wifiManager->m_client), result, &error);
                 _wifiManager->m_isSuccess = true;
            }

            if (error) {
                 _wifiManager->m_isSuccess = false;
                if (_wifiManager->m_createNewConnection) {
                    NMLOG_ERROR("Failed to add/activate new connection: %s", error->message);
                } else {
                    NMLOG_ERROR("Failed to activate connection: %s", error->message);
                }
                g_error_free(error);
            }

            if(activeConnection)
                g_object_unref(activeConnection);
            g_main_loop_quit(_wifiManager->m_loop);
        }

        static void wifiConnectionUpdate(GObject *rmObject, GAsyncResult *res, gpointer user_data)
        {
            NMRemoteConnection *remote_con = NM_REMOTE_CONNECTION(rmObject);
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            GVariant *ret = NULL;
            GError *error = NULL;

            ret = nm_remote_connection_update2_finish(remote_con, res, &error);

            if (!ret || error) {
                if(error) {
                    NMLOG_ERROR("Error: %s.", error->message);
                    g_error_free(error);
                }
                _wifiManager->m_isSuccess = false;
                _wifiManager->quit(NULL);
                return;
            }
            else
                g_variant_unref(ret);

            _wifiManager->m_createNewConnection = false; // no need to create new connection
            nm_client_activate_connection_async(
                _wifiManager->m_client, NM_CONNECTION(remote_con), _wifiManager->m_wifidevice, _wifiManager->m_objectPath, NULL, wifiConnectCb, _wifiManager);
        }

        static bool connectionBuilder(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo, NMConnection *m_connection, bool iswpsAP = false)
        {
            if(ssidinfo.ssid.empty() || ssidinfo.ssid.length() > 32)
            {
                NMLOG_WARNING("ssid name is missing or invalid");
                return false;
            }
            /* Build up the 'connection' Setting */
            NMSettingConnection  *sConnection = (NMSettingConnection *) nm_setting_connection_new();
            const char *uuid = nm_utils_uuid_generate();
            g_object_set(G_OBJECT(sConnection), NM_SETTING_CONNECTION_UUID, uuid, NULL); // uuid
            g_object_set(G_OBJECT(sConnection), NM_SETTING_CONNECTION_ID, ssidinfo.ssid.c_str(), NULL); // connection id = ssid
            g_object_set(G_OBJECT(sConnection), NM_SETTING_CONNECTION_INTERFACE_NAME, "wlan0", NULL); // interface name
            g_object_set(G_OBJECT(sConnection), NM_SETTING_CONNECTION_TYPE, "802-11-wireless", NULL); // type 802.11wireless
            nm_connection_add_setting(m_connection, NM_SETTING(sConnection));
            if(uuid) 
                g_free((void *)uuid);

            /* Build up the '802-11-wireless-security' settings */
            NMSettingWireless *sWireless = NULL;
            sWireless = (NMSettingWireless *)nm_setting_wireless_new();
            nm_connection_add_setting(m_connection, NM_SETTING(sWireless));
            GBytes *ssid = g_bytes_new(ssidinfo.ssid.c_str(), strlen(ssidinfo.ssid.c_str()));
            g_object_set(G_OBJECT(sWireless), NM_SETTING_WIRELESS_SSID, ssid, NULL); // ssid in Gbyte
            g_object_set(G_OBJECT(sWireless), NM_SETTING_WIRELESS_MODE, NM_SETTING_WIRELESS_MODE_INFRA, NULL); // infra mode
            if(!iswpsAP) // wps never be a hidden AP, it will be always visible
                g_object_set(G_OBJECT(sWireless), NM_SETTING_WIRELESS_HIDDEN, true, NULL); // hidden = true 
            if(ssid)
                g_bytes_unref(ssid);
            // 'bssid' parameter is used to restrict the connection only to the BSSID
            // g_object_set(s_wifi, NM_SETTING_WIRELESS_BSSID, bssid, NULL);

            NMSettingWirelessSecurity *sSecurity = NULL;
            switch(ssidinfo.security)
            {
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE:
                {

                    sSecurity = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new();
                    nm_connection_add_setting(m_connection, NM_SETTING(sSecurity));
                    if(Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE == ssidinfo.security)
                    {
                        NMLOG_INFO("key-mgmt: %s", "sae");
                        g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,"sae", NULL);
                    }
                    else
                    {
                        NMLOG_INFO("key-mgmt: %s", "wpa-psk");
                        g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,"wpa-psk", NULL);
                    }

                    /* if ap is not a wps network */
                    if(!iswpsAP)
                    {
                        if(ssidinfo.passphrase.empty() || ssidinfo.passphrase.length() < 8)
                        {
                            NMLOG_WARNING("password legth should be >= 8");
                            return false;
                        }
                        g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_PSK, ssidinfo.passphrase.c_str(), NULL);
                    }
                    break;
                }
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_EAP:
                {
                    NMSetting8021x *s8021X = NULL;
                    GError *error = NULL;

                    NMLOG_INFO("key-mgmt: %s", "802.1X");
                    NMLOG_DEBUG("802.1x Identity : %s", ssidinfo.eap_identity.c_str());
                    NMLOG_DEBUG("802.1x CA cert path : %s", ssidinfo.ca_cert.c_str());
                    NMLOG_DEBUG("802.1x Client cert path : %s", ssidinfo.client_cert.c_str());
                    NMLOG_DEBUG("802.1x Private key path : %s", ssidinfo.private_key.c_str());
                    NMLOG_DEBUG("802.1x Private key psswd : %s", ssidinfo.private_key_passwd.c_str());

                    s8021X = (NMSetting8021x *) nm_setting_802_1x_new();
                    nm_connection_add_setting(m_connection, NM_SETTING(s8021X));
                    g_object_set(s8021X, NM_SETTING_802_1X_IDENTITY, ssidinfo.eap_identity.c_str(), NULL);
                    nm_setting_802_1x_add_eap_method(s8021X, "tls");
                    if(!ssidinfo.ca_cert.empty() && !nm_setting_802_1x_set_ca_cert(s8021X,
                                                ssidinfo.ca_cert.c_str(),
                                                NM_SETTING_802_1X_CK_SCHEME_PATH,
                                                NULL,
                                                &error))
                    {
                        NMLOG_ERROR("ca certificate add failed: %s", error->message);
                        g_error_free(error);
                        return false;
                    }

                    if(!ssidinfo.client_cert.empty() && !nm_setting_802_1x_set_client_cert(s8021X,
                                                ssidinfo.client_cert.c_str(),
                                                NM_SETTING_802_1X_CK_SCHEME_PATH,
                                                NULL,
                                                &error))
                    {
                        NMLOG_ERROR("client certificate add failed: %s", error->message);
                        g_error_free(error);
                        return false;
                    }

                    if(!ssidinfo.private_key.empty() && !nm_setting_802_1x_set_private_key(s8021X,
                                                    ssidinfo.private_key.c_str(),
                                                    ssidinfo.private_key_passwd.c_str(),
                                                    NM_SETTING_802_1X_CK_SCHEME_PATH,
                                                    NULL,
                                                    &error))
                    {
                        NMLOG_ERROR("client private key add failed: %s", error->message);
                        g_error_free(error);
                        return false;
                    }

                    sSecurity = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new();
                    nm_connection_add_setting(m_connection, NM_SETTING(sSecurity));
                    g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,"wpa-eap", NULL);
                    break;
                }
                case Exchange::INetworkManager::WIFI_SECURITY_NONE:
                {
                    NMLOG_WARNING("open wifi network configuration key-mgmt: none");
                    break;
                }
                default:
                {
                    NMLOG_ERROR("wifi securtity type not supported %d", ssidinfo.security);
                    return false;
                }
            }

            /* Build up the 'ipv4' Setting */
            NMSettingIP4Config *sIpv4Conf = (NMSettingIP4Config *) nm_setting_ip4_config_new();
            g_object_set(G_OBJECT(sIpv4Conf), NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL); // autoconf = true
            nm_connection_add_setting(m_connection, NM_SETTING(sIpv4Conf));

            /* Build up the 'ipv6' Setting */
            NMSettingIP6Config *sIpv6Conf = (NMSettingIP6Config *) nm_setting_ip6_config_new();
            g_object_set(G_OBJECT(sIpv6Conf), NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_AUTO, NULL); // autoconf = true
            nm_connection_add_setting(m_connection, NM_SETTING(sIpv6Conf));
            return true;
        }

        bool wifiManager::activateKnownConnection(std::string iface, std::string knowConnectionID)
        {
            const GPtrArray *devConnections = NULL;
            NMConnection *knownConnection = NULL;
            NMConnection *firstConnection = NULL;
            const char* specificObjPath = "/";

            if(!createClientNewConnection())
                return false;

            NMDevice *nmDevice = nm_client_get_device_by_iface(m_client, iface.c_str());;
            if(nmDevice == NULL) {
                NMLOG_WARNING("nm_client_get_device_by_iface failed !");
                deleteClientConnection();
                return false;
            }

            nm_device_set_autoconnect(nmDevice, true); // set autoconnect true

            devConnections = nm_client_get_connections(m_client);
            if(devConnections == NULL || devConnections->len == 0)
            {
                NMLOG_ERROR("No connections found !");
                deleteClientConnection();
                return false;
            }

            for (guint i = 0; i < devConnections->len; i++)
            {
                NMConnection *connection = static_cast<NMConnection*>(g_ptr_array_index(devConnections, i));
                if(connection == NULL)
                    continue;

                const char *connId = nm_connection_get_id(NM_CONNECTION(connection));
                if (connId == NULL) {
                    NMLOG_WARNING("connection id is NULL");
                    continue;
                }

                const char *connTyp = nm_connection_get_connection_type(NM_CONNECTION(connection));
                if (connTyp == NULL) {
                    NMLOG_WARNING("connection type is NULL");
                    continue;
                }

                std::string connTypStr = connTyp;
                NMLOG_DEBUG("connection id: %s, type: %s", connId != NULL ? connId : "NULL", connTypStr.c_str());
                if(connTypStr == "802-11-wireless") {
                    NMLOG_INFO("wifi conn found : %s", connId);
                    // if no known ssid given then use first wifi connection from list
                    // it usefule when bootup there will be no known ssid
                    if(firstConnection == NULL)
                        firstConnection = connection;
                }

                if (connId != NULL && strcmp(connId, knowConnectionID.c_str()) == 0)
                {
                    knownConnection = g_object_ref(connection);
                    NMLOG_DEBUG("connection '%s' exists !", knowConnectionID.c_str());
                    if (knownConnection == NULL)
                    {
                        NMLOG_ERROR("knownConnection == NULL smothing went worng");
                        deleteClientConnection();
                        return false;
                    }
                    break;
                }
            }

            // if no connection found then try activate first connection form connection list
            if(knownConnection == NULL && firstConnection != NULL)
            {
                NMLOG_INFO("No known connection found, using first wifi connection");
                knownConnection = g_object_ref(firstConnection);
                knowConnectionID = nm_connection_get_id(NM_CONNECTION(knownConnection));
            }

            m_isSuccess = false;
            if (knownConnection != NULL && NM_IS_REMOTE_CONNECTION(knownConnection))
            {
                NMLOG_INFO("activating known wifi '%s' connection", knowConnectionID.c_str());
                m_createNewConnection = false; // no need to create new connection
                nm_client_activate_connection_async(m_client, NM_CONNECTION(knownConnection), nmDevice, specificObjPath, NULL, wifiConnectCb, this);
                wait(m_loop);
            }
            else
            {
                NMLOG_ERROR("'%s' connection not found !",  knowConnectionID.c_str());
            }

            if(knownConnection != NULL)
                g_object_unref(knownConnection);

            deleteClientConnection();
            return m_isSuccess;
        }

        bool wifiManager::wifiConnect(Exchange::INetworkManager::WiFiConnectTo ssidInfo)
        {
            NMAccessPoint *AccessPoint = NULL;
            const GPtrArray* ApList = NULL;
            NMConnection *m_connection = NULL;
            const GPtrArray  *availableConnections = NULL;

            Exchange::INetworkManager::WiFiSSIDInfo apinfo;
            std::string activeSSID{};

            NMLOG_DEBUG("wifi connect ssid: %s, security %d persist %d", ssidInfo.ssid.c_str(), ssidInfo.security, ssidInfo.persist);

            m_isSuccess = false;
            if(!createClientNewConnection())
                return false;

            m_wifidevice = getWifiDevice();
            if(m_wifidevice == NULL)
            {
                deleteClientConnection();
                return false;
            }

            if(getConnectedSSID(m_wifidevice, activeSSID))
            {
                if(ssidInfo.ssid == activeSSID)
                {
                    NMLOG_INFO("'%s' already connected !", activeSSID.c_str());
                    _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTED);
                    deleteClientConnection();
                    return true;
                }
                else
                    NMLOG_DEBUG("wifi already connected with %s AP", activeSSID.c_str());
            }

            ApList = nm_device_wifi_get_access_points(NM_DEVICE_WIFI(m_wifidevice));
            if(ApList == NULL)
            {
                NMLOG_ERROR("Aplist Error !");
                deleteClientConnection();
                return false;
            }

            AccessPoint = findMatchingSSID(ApList, ssidInfo.ssid);
            if(AccessPoint == NULL) {
                NMLOG_WARNING("SSID '%s' not found !", ssidInfo.ssid.c_str());
                // if(_instance != nullptr)
                //     _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
                /* ssid not found in scan list so add to known ssid it will do a scanning and connect */
                if(addToKnownSSIDs(ssidInfo))
                {
                    NMLOG_DEBUG("Adding to known ssid '%s' ", ssidInfo.ssid.c_str());
                    deleteClientConnection();
                    return activateKnownConnection(nmUtils::wlanIface(), ssidInfo.ssid);
                }
                deleteClientConnection();
                return false;
            }

            getApInfo(AccessPoint, apinfo);

            if(ssidInfo.security != apinfo.security)
            {
                NMLOG_WARNING("user requested wifi security '%d' != AP supported security %d ", ssidInfo.security, apinfo.security);
                ssidInfo.security = apinfo.security;
            }

            availableConnections = nm_device_get_available_connections(m_wifidevice);
            if(availableConnections != NULL)
            {
                for (guint i = 0; i < availableConnections->len; i++)
                {
                    NMConnection *connection = static_cast<NMConnection*>(g_ptr_array_index(availableConnections, i));
                    const char *connId = nm_connection_get_id(NM_CONNECTION(connection));
                    if (connId != NULL && strcmp(connId, ssidInfo.ssid.c_str()) == 0)
                    {
                        if (nm_access_point_connection_valid(AccessPoint, NM_CONNECTION(connection))) {
                            m_connection = g_object_ref(connection);
                            NMLOG_DEBUG("connection '%s' exists !", ssidInfo.ssid.c_str());
                            if (m_connection == NULL)
                            {
                                NMLOG_ERROR("m_connection == NULL smothing went worng");
                                deleteClientConnection();
                                return false;
                            }
                            break;
                        }
                        else
                        {
                            if (NM_IS_REMOTE_CONNECTION(connection))
                            {
                                /* 
                                * libnm reuses the existing connection if new settings match the AP properties;
                                * remove the old one because now only one connection per SSID is supported. 
                                */
                                GError *error = NULL;
                                NMLOG_WARNING(" '%s' connection existing but properties mismatch; deleting...", ssidInfo.ssid.c_str());
                                nm_remote_connection_delete (NM_REMOTE_CONNECTION(connection), NULL, &error);
                                if (error) {
                                    NMLOG_ERROR("deleting connection failed %s", error->message);
                                    g_error_free(error);
                                }
                                else
                                    connection = NULL;
                            }
                        }
                    }
                }
            }

            if (m_connection != NULL && NM_IS_REMOTE_CONNECTION(m_connection))
            {
                if(!connectionBuilder(ssidInfo, m_connection)) {
                    NMLOG_ERROR("connection builder failed");
                    deleteClientConnection();
                    return false;
                }
                GVariant *connSettings = nm_connection_to_dbus(m_connection, NM_CONNECTION_SERIALIZE_ALL);
                nm_remote_connection_update2(NM_REMOTE_CONNECTION(m_connection),
                                            connSettings,
                                            NM_SETTINGS_UPDATE2_FLAG_BLOCK_AUTOCONNECT, // block auto connect becuse manualy activate 
                                            NULL,
                                            NULL,
                                            wifiConnectionUpdate,
                                            this);
            }
            else
            {
                NMLOG_DEBUG("creating new connection '%s' ", ssidInfo.ssid.c_str());
                m_connection = nm_simple_connection_new();
                m_objectPath = nm_object_get_path(NM_OBJECT(AccessPoint));
                if(!connectionBuilder(ssidInfo, m_connection))
                {
                    NMLOG_ERROR("connection builder failed");
                    if(m_connection)
                        g_object_unref(m_connection);
                    deleteClientConnection();
                    return false;
                }
                m_createNewConnection = true;
                nm_client_add_and_activate_connection_async(m_client, m_connection, m_wifidevice, m_objectPath, NULL, wifiConnectCb, this);
                if(m_connection)
                    g_object_unref(m_connection);
            }

            wait(m_loop);
            deleteClientConnection();
            return m_isSuccess;
        }

         static void addToKnownSSIDsUpdateCb(GObject *rmObject, GAsyncResult *res, gpointer user_data)
        {
            NMRemoteConnection *remote_con = NM_REMOTE_CONNECTION(rmObject);
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            GVariant *ret = NULL;
            GError *error = NULL;

            ret = nm_remote_connection_update2_finish(remote_con, res, &error);

            if (!ret) {
                NMLOG_ERROR("Error: %s.", error->message);
                g_error_free(error);
                _wifiManager->m_isSuccess = false;
                NMLOG_ERROR("AddToKnownSSIDs failed");
            }
            else
            {
                _wifiManager->m_isSuccess = true;
                NMLOG_INFO("AddToKnownSSIDs success");
            }

            if(ret)
                g_variant_unref(ret);
            
            _wifiManager->quit(NULL);
        }

        static void addToKnownSSIDsCb(GObject *client, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            if (!nm_client_add_connection2_finish(NM_CLIENT(client), result, NULL, &error)) {
                NMLOG_ERROR("AddToKnownSSIDs Failed");
                _wifiManager->m_isSuccess = false;
            }
            else
            {
                NMLOG_INFO("AddToKnownSSIDs success");
                _wifiManager->m_isSuccess = true;
            }

            if(error) {
                NMLOG_ERROR("AddToKnownSSIDs Failed: %s", error->message);
                _wifiManager->m_isSuccess = false;
                g_error_free(error);
            }
            g_main_loop_quit(_wifiManager->m_loop);
        }

        bool wifiManager::addToKnownSSIDs(const Exchange::INetworkManager::WiFiConnectTo ssidinfo)
        {
            m_isSuccess = false;
            NMConnection *m_connection = NULL;

            if(!createClientNewConnection())
                return false;

            NMDevice *device = getWifiDevice();
            if(device == NULL)
            {
                deleteClientConnection();
                return false;
            }

            const GPtrArray  *availableConnections = nm_device_get_available_connections(device);
            if(availableConnections == NULL)
            {
                NMLOG_ERROR("No available connections found !");
                deleteClientConnection();
                return false;
            }

            for (guint i = 0; i < availableConnections->len; i++)
            {
                NMConnection *connection = static_cast<NMConnection*>(g_ptr_array_index(availableConnections, i));
                const char *connId = nm_connection_get_id(NM_CONNECTION(connection));
                if (connId != NULL && strcmp(connId, ssidinfo.ssid.c_str()) == 0)
                {
                    m_connection = g_object_ref(connection);
                }
            }

            if (NM_IS_REMOTE_CONNECTION(m_connection))
            {
                if(!connectionBuilder(ssidinfo, m_connection))
                {
                    NMLOG_ERROR("connection builder failed");
                    deleteClientConnection();
                    return false;
                }
                NMLOG_DEBUG("update exsisting connection '%s' ", ssidinfo.ssid.c_str());
                GVariant *connSettings = nm_connection_to_dbus(m_connection, NM_CONNECTION_SERIALIZE_ALL);
                nm_remote_connection_update2(NM_REMOTE_CONNECTION(m_connection),
                                            connSettings,
                                            NM_SETTINGS_UPDATE2_FLAG_TO_DISK,
                                            NULL,
                                            NULL,
                                            addToKnownSSIDsUpdateCb,
                                            this);
            }
            else
            {
                NMLOG_DEBUG("creating new connection '%s' ", ssidinfo.ssid.c_str());
                m_connection = nm_simple_connection_new();
                if(!connectionBuilder(ssidinfo, m_connection))
                {
                    NMLOG_ERROR("connection builder failed");
                    deleteClientConnection();
                    return false;
                }
                m_createNewConnection = true;
                GVariant *connSettings = nm_connection_to_dbus(m_connection, NM_CONNECTION_SERIALIZE_ALL);
                nm_client_add_connection2(m_client,
                                        connSettings,
                                        NM_SETTINGS_ADD_CONNECTION2_FLAG_TO_DISK,
                                        NULL, TRUE, NULL,
                                        addToKnownSSIDsCb, this);
            }
            wait(m_loop);
            deleteClientConnection();
            return m_isSuccess;
        }

        bool wifiManager::removeKnownSSID(const string& ssid)
        {
            NMConnection *m_connection = NULL;
            bool ssidSpecified = false;
            bool connectionFound = false;

            if(!createClientNewConnection())
                return false;

            if(!ssid.empty())
                ssidSpecified = true;
            else
                NMLOG_WARNING("ssid is not specified, Deleting all availble wifi connection !");

            const GPtrArray  *allconnections = nm_client_get_connections(m_client);
            if(allconnections == NULL)
            {
                NMLOG_ERROR("nm connections list null ");
                deleteClientConnection();
                return false;
            }
            for (guint i = 0; i < allconnections->len; i++)
            {
                GError *error = NULL;
                NMConnection *connection = static_cast<NMConnection*>(g_ptr_array_index(allconnections, i));
                if(connection == NULL)
                {
                    NMLOG_WARNING("ssid connection null !");
                    continue;
                }
                if (!NM_IS_SETTING_WIRELESS(nm_connection_get_setting_wireless(connection)))
                    continue; // if not wireless connection skipt
                const char *connId = nm_connection_get_id(NM_CONNECTION(connection));
                if(connId == NULL)
                {
                    NMLOG_WARNING("ssid connection id null !");
                    continue;
                }

                NMLOG_DEBUG("wireless connection '%s'", connId);
                if (ssidSpecified && strcmp(connId, ssid.c_str()) != 0)
                    continue;

                m_connection = g_object_ref(connection);
                if (NM_IS_REMOTE_CONNECTION(connection))
                {
                    nm_remote_connection_delete(NM_REMOTE_CONNECTION(connection), NULL, &error);
                    if(error) {
                        NMLOG_ERROR("deleting connection failed %s", error->message);
                    }
                    else
                        NMLOG_INFO("delete '%s' connection ...", connId);
                    connectionFound = true;
                }

                if(m_connection)
                {
                    g_object_unref(m_connection);
                    m_connection = NULL;
                }
            }

            if(!connectionFound)
            {
                if(ssidSpecified)
                    NMLOG_WARNING("'%s' no such connection profile", ssid.c_str());
                else
                    NMLOG_WARNING("No wifi connection profiles found !!"); 
            }

            deleteClientConnection();
            // ssid is specified and connection is not found return false
            // all other case return true, even if no wificonnection is found
            return((ssidSpecified && !connectionFound)?false:true);
        }

        bool wifiManager::getKnownSSIDs(std::list<string>& ssids)
        {
            std::string ssidPrint{};

            if(!createClientNewConnection())
                return false;

            const GPtrArray *connections = nm_client_get_connections(m_client);
            if(connections == nullptr)
            {
                NMLOG_ERROR("nm connections list null ");
                deleteClientConnection();
                return false;
            }
            for (guint i = 0; i < connections->len; i++)
            {
                NMConnection *connection = NM_CONNECTION(connections->pdata[i]);
                if(NM_IS_SETTING_WIRELESS(nm_connection_get_setting_wireless(connection)))
                {
                    GBytes *ssidBytes = nm_setting_wireless_get_ssid(nm_connection_get_setting_wireless(connection));
                    if (ssidBytes)
                    {
                        char* ssidStr = nm_utils_ssid_to_utf8((const guint8*)g_bytes_get_data(ssidBytes, NULL), g_bytes_get_size(ssidBytes));
                        if(ssidStr != nullptr)
                        {
                            ssids.push_back(string(ssidStr));
                            ssidPrint += ssidStr;
                            ssidPrint += ", ";
                            free(ssidStr);
                        }
                        else
                        {
                            NMLOG_ERROR("Invalid ssid length Error");
                            continue;
                        }
                    }
                    else
                    {
                        /* hidden ssid */
                        NMLOG_WARNING("wifi connection list have hidden ssid also !");
                    }
                }
            }
            if (!ssids.empty())
            {
                NMLOG_INFO("known wifi connections are %s", ssidPrint.c_str());
                deleteClientConnection();
                return true;
            }

            deleteClientConnection();
            return false;
        }

        static void wifiScanCb(GObject *object, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            if(nm_device_wifi_request_scan_finish(NM_DEVICE_WIFI(object), result, &error)) {
                 _wifiManager->m_isSuccess = true;
            }
            else
            {
                NMLOG_ERROR("Scanning Failed");
                _wifiManager->m_isSuccess = false;
            }
            if (error) {
                NMLOG_ERROR("Scanning Failed Error: %s.", error->message);
                _wifiManager->m_isSuccess = false;
                g_error_free(error);
            }

            g_main_loop_quit(_wifiManager->m_loop);
        }

        bool wifiManager::wifiScanRequest(std::string ssidReq)
        {
            if(!createClientNewConnection())
                return false;
            NMDeviceWifi *wifiDevice = NM_DEVICE_WIFI(getWifiDevice());
            if(wifiDevice == NULL) {
                NMLOG_FATAL("NMDeviceWifi * NULL !");
                deleteClientConnection();
                return false;
            }
            m_isSuccess = false;
            if(!ssidReq.empty() && ssidReq != "null")
            {
                NMLOG_INFO("staring wifi scanning .. %s", ssidReq.c_str());
                GVariantBuilder builder, array_builder;
                GVariant *options;
                g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
                g_variant_builder_init(&array_builder, G_VARIANT_TYPE("aay"));
                g_variant_builder_add(&array_builder, "@ay",
                                    g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, (const guint8 *) ssidReq.c_str(), ssidReq.length(), 1)
                                    );
                g_variant_builder_add(&builder, "{sv}", "ssids", g_variant_builder_end(&array_builder));
                options = g_variant_builder_end(&builder);
                nm_device_wifi_request_scan_options_async(wifiDevice, options, NULL, wifiScanCb, this);
                g_variant_unref(options); // Unreference the GVariant after passing it to the async function
            }
            else {
                nm_device_wifi_request_scan_async(wifiDevice, NULL, wifiScanCb, this);
            }
            wait(m_loop);
            deleteClientConnection();
            return m_isSuccess;
        }

        bool wifiManager::isWifiScannedRecently(int timelimitInSec)
        {
            if (!createClientNewConnection())
                return false;

            NMDeviceWifi *wifiDevice = NM_DEVICE_WIFI(getWifiDevice());
            if (wifiDevice == NULL) {
                NMLOG_ERROR("Invalid Wi-Fi device.");
                deleteClientConnection();
                return false;
            }

            gint64 last_scan_time = nm_device_wifi_get_last_scan(wifiDevice);
            if (last_scan_time <= 0) {
                NMLOG_INFO("No scan has been performed yet");
                deleteClientConnection();
                return false;
            }

            gint64 current_time_in_msec = nm_utils_get_timestamp_msec();
            gint64 time_difference_in_seconds = (current_time_in_msec - last_scan_time) / 1000;
            if (time_difference_in_seconds <= timelimitInSec) {
                deleteClientConnection();
                return true;
            }
            deleteClientConnection();
            return false;
        }

        static void wpsWifiConnectCb(GObject *client, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            GMainLoop *loop = static_cast<GMainLoop *>(user_data);
            nm_client_add_and_activate_connection_finish(NM_CLIENT(client), result, &error);

            if (error)
                NMLOG_ERROR("Failed to add/activate new connection: %s", error->message);
            else
                NMLOG_INFO("WPS connection added/activated successfully");
            g_main_loop_quit(loop);
        }

        static gboolean wpsGmainLoopTimoutCB(gpointer user_data)
        {
            GMainLoop *loop = static_cast<GMainLoop *>(user_data);
            NMLOG_WARNING("GmainLoop ERROR_TIMEDOUT");
            g_main_loop_quit(loop);
            return true;
        }

        static bool removeSameNmConnection(NMDevice* wifidevice, std::string ssid)
        {
            const GPtrArray *deviceConns = NULL;

            deviceConns = nm_device_get_available_connections(wifidevice);
            if(deviceConns != NULL)
            {
                for (guint i = 0; i < deviceConns->len; i++)
                {
                    NMConnection* connection = static_cast<NMConnection*>(g_ptr_array_index(deviceConns, i));
                    if(connection == NULL)
                        continue;
                    const char* connId = nm_connection_get_id(NM_CONNECTION(connection));
                    if (connId != NULL && strcmp(connId, ssid.c_str()) == 0)
                    {
                        GError* error = NULL;
                        NMLOG_WARNING(" '%s' wps connection existing deleting...", ssid.c_str());
                        nm_remote_connection_delete (NM_REMOTE_CONNECTION(connection), NULL, &error);
                        if(error) {
                            NMLOG_ERROR("deleting connection failed %s", error->message);
                            return false;
                        }
                    }
                }
            }

            return true;
        }

        static bool findWpsPbcSSID(const GPtrArray* ApList, Exchange::INetworkManager::WiFiSSIDInfo& wpsApInfo, const char** apObjPath)
        {
            for (guint i = 0; i < ApList->len; i++)
            {
                NMAccessPoint *Ap = static_cast<NMAccessPoint *>(g_ptr_array_index(ApList, i));
                guint32 flags = nm_access_point_get_flags(Ap);
                if (flags & NM_802_11_AP_FLAGS_WPS_PBC)
                {
                    getApInfo(Ap, wpsApInfo, true);
                    *apObjPath = nm_object_get_path(NM_OBJECT(Ap));
                    if(*apObjPath == NULL)
                        return false;
                    NMLOG_INFO("WPS PBC AP found! bssid: %s, Ap Path: %s", wpsApInfo.bssid.c_str(), *apObjPath);
                    return true;
                }
            }

            NMLOG_INFO("WPS PBC Not found !!!");
            return false;
        }

        void wifiManager::wpsProcess()
        {
            wpsProcessRun = true;
            GError *error = NULL;
            const GPtrArray* ApList = NULL;
            std::string wpsApSsid{};
            GMainContext *wpsContext = NULL;
            const char *wpsApPath = NULL;
            GMainLoop *loop = NULL;
            NMClient* client = NULL;
            Exchange::INetworkManager::WiFiConnectTo wifiConnectInfo{};
            Exchange::INetworkManager::WiFiSSIDInfo wpsApInfo{};

            bool wpsComplete= false;
            bool wpsActionTriggerd = false;

            if(_instance != nullptr)
                _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTING);

            for(int retry = 0; retry < WPS_RETRY_COUNT; retry++)
            {
                sleep(WPS_RETRY_WAIT_IN_MS);
                if(wpsProcessRun.load() == false) // stop wps process if reuested
                {
                    NMLOG_INFO("stop wps process reuested");
                    break;
                }

                wpsContext = g_main_context_new();
                if(wpsContext == NULL)
                {
                    NMLOG_ERROR("wpsContext create failed !!");
                    break;
                }

                if(!g_main_context_acquire(wpsContext))
                {
                    NMLOG_ERROR("acquire wpsContext failed !!");
                    break;
                }

                g_main_context_push_thread_default(wpsContext);

                client = nm_client_new(NULL, &error);
                if (!client)
                {
                    if(error != NULL) {
                        NMLOG_ERROR("Could not connect to NetworkManager: %s.", error->message);
                        g_error_free(error);
                    }
                    else
                        NMLOG_ERROR("NetworkManager cleint create failed");
                    break;
                }

                NMDevice* wifidevice = nm_client_get_device_by_iface(client, nmUtils::wlanIface());
                if(wifidevice == NULL)
                {
                    NMLOG_ERROR("Failed to get device list.");
                    break;
                }

                if(wpsActionTriggerd)
                {
                    /* if wps action started we need to check the status of wifi device and post event based on that */
                    NMDeviceState state = nm_device_get_state(wifidevice);
                    if(state <= NM_DEVICE_STATE_DISCONNECTED)
                    {
                        if(_instance != nullptr)
                            _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
                        // TODO post correct error code insted of WIFI_STATE_SSID_NOT_FOUND
                        // sedning WIFI_STATE_SSID_NOT_FOUND to avoid UI stuck issue
                        break;
                    }
                    else if(state > NM_DEVICE_STATE_NEED_AUTH)
                    {
                        NMLOG_INFO("WPS process completed successfully");
                        wpsComplete = true;
                        // TODO stop Secret agenet ?
                        break;
                    }

                    NMLOG_INFO("WPS process not completed yet, state: %d", state);
                    if(retry >= WPS_RETRY_COUNT - 1) // 3 times retry to check the status
                    {
                        // wifi state stuck in betwen disconnected and connected
                        NMLOG_ERROR("WPS process failed");
                        if(_instance != nullptr)
                            _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
                        // TODO post correct error code insted of WIFI_STATE_SSID_NOT_FOUND
                        // sedning WIFI_STATE_SSID_NOT_FOUND to avoid UI stuck issue
                    }
                }

                ApList = nm_device_wifi_get_access_points(NM_DEVICE_WIFI(wifidevice));
                if(ApList == NULL)
                {
                    NMLOG_ERROR("Aplist Error !");
                    break;
                }

                if(!wpsActionTriggerd && findWpsPbcSSID(ApList, wpsApInfo, &wpsApPath) && wpsApPath != NULL)
                {
                    Exchange::INetworkManager::WiFiSSIDInfo activeApInfo{};
                    NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(wifidevice));
                    if(activeAP != NULL)
                    {
                        NMLOG_WARNING("active access point found during wps operation !");
                        getApInfo(activeAP, activeApInfo);
                        if(activeApInfo.ssid == wpsApInfo.ssid)
                        {
                            NMLOG_INFO("WPS process stoped connected to same '%s' wps ap ", wpsApInfo.ssid.c_str());
                            wpsComplete = true;
                            //TODO Post SSID connected event ?
                            if(_instance != nullptr)
                                _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTED);

                        }
                        else
                        {
                            NMDeviceState state = nm_device_get_state(wifidevice);
                            if(state > NM_DEVICE_STATE_DISCONNECTED)
                            {
                                NMLOG_INFO("stopping the ongoing Wi-Fi connection");
                                // some other ssid connected or connecting; wps need a disconnected wifi state
                                nm_device_disconnect(wifidevice, NULL,  &error);
                                if (error) {
                                    NMLOG_ERROR("disconnect connection failed %s", error->message);
                                    g_error_free(error);
                                }
                                sleep(3); // wait for 3 sec to complete disconnect process
                            }
                        }
                    }

                    if(wpsComplete)
                    {
                        break;
                    }

                    wifiConnectInfo.ssid = wpsApInfo.ssid;
                    wifiConnectInfo.security = wpsApInfo.security;
                    NMConnection* connection = NULL;
                    /* if same connection name exsist we remove and add new one */
                    removeSameNmConnection(wifidevice, wifiConnectInfo.ssid);
                    NMLOG_DEBUG("creating new connection '%s' ", wifiConnectInfo.ssid.c_str());
                    connection = nm_simple_connection_new();
                    if(!connectionBuilder(wifiConnectInfo, connection, true))
                    {
                        NMLOG_ERROR("wps connection builder failed");
                        break;
                    }

                    loop = g_main_loop_new(wpsContext, FALSE);
                    if(loop == NULL)
                    {
                        NMLOG_ERROR("g_main_loop_new failed");
                        break;
                    }

                    nm_client_add_and_activate_connection_async(client, connection, wifidevice, wpsApPath, NULL, wpsWifiConnectCb, loop);
                    GSource *source = g_timeout_source_new(10000);  // 10000ms interval
                    if(source != nullptr) {
                        g_source_set_callback(source, (GSourceFunc)wpsGmainLoopTimoutCB, loop, NULL);
                        g_source_attach(source, NULL);
                        g_main_loop_run(loop);
                        if(g_source_is_destroyed(source)) {
                            NMLOG_WARNING("Source has been destroyed");
                        }
                        else {
                            g_source_destroy(source);
                        }
                        g_source_unref(source);
                    }
                    if(loop != NULL) {
                        g_main_loop_unref(loop);
                        loop = NULL;
                    }
                    if(connection != NULL) {
                        g_object_unref(connection);
                        connection = NULL;
                    }

                    wpsActionTriggerd = true;
                    retry = WPS_RETRY_COUNT - 3; // expecting wps process will be completed in 30 sec(3 10sec retry) (Ex: retry = 10-3)
                }
                else if(!wpsActionTriggerd)
                {
                    /* if wps action not triggerd do a scanning request */
                    nm_device_wifi_request_scan(NM_DEVICE_WIFI(wifidevice), NULL, &error);
                }

                g_main_context_pop_thread_default(wpsContext);
                g_main_context_unref(wpsContext);
                wpsContext = NULL;
                if(client != NULL) {
                    g_object_unref(client);
                    client = NULL;
                }

                if(wpsComplete || wpsProcessRun.load() == false) // stop wps process if user requested
                    break;
            }

            if(wpsApPath == NULL)
            {
                NMLOG_WARNING("WPS AP not found");
                if(_instance != nullptr)
                    _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
            }
            else if(!wpsComplete)
            {
                NMLOG_INFO("WPS process Error");
                if(_instance != nullptr)
                    _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED);
            }

            if(wpsContext != NULL)
            {
                g_main_context_pop_thread_default(wpsContext);
                g_main_context_unref(wpsContext);
            }

            if(client != NULL) {
                g_object_unref(client);
                client = NULL;
            }

            // wps success ! wifi connect event will be send by wifi event monitor
            m_secretAgent.UnregisterAgent();
            NMLOG_INFO("WPS process thread exist");
            wpsProcessRun = false;
        }

        bool wifiManager::startWPS()
        {
            NMLOG_DEBUG("Start WPS %s", __FUNCTION__);
            if(wpsProcessRun.load())
            {
                NMLOG_WARNING("wps process WPS already running");
                return true;
            }

            m_secretAgent.RegisterAgent();
            std::thread wpsthread(&wifiManager::wpsProcess, this);
            wpsthread.detach();
            return true;
        }

        bool wifiManager::stopWPS() {
            NMLOG_DEBUG("Stop WPS %s", __FUNCTION__);
            wpsProcessRun = false; 
            return m_secretAgent.UnregisterAgent();
        }

        static void deviceManagedCb(GObject *object, GAsyncResult *result, gpointer user_data)
        {
            wifiManager *_wifiManager = static_cast<wifiManager *>(user_data);
            GError *error = nullptr;

            if (!nm_client_dbus_set_property_finish(NM_CLIENT(object), result, &error)) {
                if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                    g_error_free(error);
                    return;
                }

                NMLOG_ERROR("Failed to set Managed property: %s", error->message);
                g_error_free(error);
                _wifiManager->m_isSuccess = false;
            } else {
                NMLOG_DEBUG("Successfully set Managed property.");
                _wifiManager->m_isSuccess = true;
            }

            _wifiManager->quit(nullptr);
        }

        bool wifiManager::setInterfaceState(std::string interface, bool enabled)
        {
            m_isSuccess = false;
            NMDevice *device = nullptr;

            if(interface.empty() || (interface != nmUtils::ethIface() && interface != nmUtils::wlanIface()))
            {
                NMLOG_ERROR("Invalid interface name: %s", interface.c_str());
                return false;
            }

            if (!createClientNewConnection())
                return false;

            GPtrArray *devices = const_cast<GPtrArray *>(nm_client_get_devices(m_client));
            if (devices == nullptr) {
                NMLOG_ERROR("Failed to get device list.");
                deleteClientConnection();
                return m_isSuccess;
            }

            for (guint j = 0; j < devices->len; j++) {
                device = NM_DEVICE(devices->pdata[j]);
                const char *ifaceStr = nm_device_get_iface(device);
                if (ifaceStr == nullptr)
                    continue;
                if (interface == ifaceStr) {
                    // NMLOG_DEBUG("Device found: %s", interface.c_str());
                    break;
                } else {
                    device = nullptr;
                }
            }

            if (device == nullptr)
            {
                NMLOG_ERROR("Device not found: %s", interface.c_str());
                deleteClientConnection();
                return false;
            }

            NMDeviceState deviceState = nm_device_get_state(device);

            if (enabled) {
                NMLOG_DEBUG("Enabling interface...");
                if (deviceState >= NM_DEVICE_STATE_UNAVAILABLE) // already enabled
                {
                    deleteClientConnection();
                    return true;
                }
            } else {
                NMLOG_DEBUG("Disabling interface...");
                if (deviceState < NM_DEVICE_STATE_UNAVAILABLE) // already disabled
                {
                    deleteClientConnection();
                    return true;
                }
                else if (deviceState > NM_DEVICE_STATE_DISCONNECTED) {
                    NMLOG_DEBUG("Disconnecting device...");
                    // Disconnect the device before setting it to unmanaged.
                    // This ensures that NetworkManager cleanly removes any IP addresses, routes,
                    // and DNS configuration associated with the interface. Setting an interface
                    // to unmanaged without disconnecting first may leave residual configuration
                    // that can cause networking issues.
                    nm_device_disconnect_async(device, nullptr, disconnectCb, this);
                    wait(m_loop);
                    // Wait until device is truly disconnected
                    int retry = 24; // 12 seconds
                    NMDeviceState oldDevState = NM_DEVICE_STATE_UNKNOWN;
                    while (retry-- > 0) {
                        /* Force glib event processing to update state
                         * This below line will create an uncertain time wait. We are taking a fixed time interval of 12 seconds.
                         */
                        // while (g_main_context_iteration(NULL, FALSE));
                        g_usleep(500 * 1000);  // give some time to NM to process the request
                        deviceState = nm_device_get_state(device);
                        if(oldDevState != deviceState)
                        {
                            oldDevState = deviceState;
                            NMLOG_WARNING("Device state: %d", deviceState);
                        }

                        if (deviceState <= NM_DEVICE_STATE_DISCONNECTED)
                            break;
                    }
                }
            }

            if(deviceState > NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_WARNING("Device not fully disconnected (state: %d), setting to unmanaged state", deviceState);
            }
            // Set the "Managed" property to enable/disable the device
            m_isSuccess = false;
            const char *objectPath = nm_object_get_path(NM_OBJECT(device));
            GVariant *value = g_variant_new_boolean(enabled);

            nm_client_dbus_set_property( m_client, objectPath, NM_DBUS_INTERFACE_DEVICE,"Managed",
                                                                    value, -1, nullptr, deviceManagedCb, this);
            wait(m_loop);
            deleteClientConnection();

            if(m_isSuccess)
            {
                // set persistent marker file for specific interface
                if(interface == nmUtils::ethIface())
                    nmUtils::setMarkerFile(EthernetDisableMarker, enabled);
                else // wifi
                    nmUtils::setMarkerFile(WiFiDisableMarker, enabled);
            }

            return m_isSuccess;
        }

        static void onActivateComplete(GObject *source_object, GAsyncResult *res, gpointer user_data)
        {
            GError *error = NULL;
            wifiManager *_wifiManager = static_cast<wifiManager *>(user_data);
            NMLOG_DEBUG("activate connection completeing...");
            // Check if the operation was successful
            if (!nm_client_activate_connection_finish(NM_CLIENT(source_object), res, &error)) {
                NMLOG_DEBUG("Activating connection failed: %s", error->message);
                g_error_free(error);
                _wifiManager->m_isSuccess = false;
            } else {
                NMLOG_DEBUG("Activating connection successful");
                _wifiManager->m_isSuccess = true;
            }
            _wifiManager->quit(nullptr);
        }

        bool static checkAutoConnectEnabledInIPv4Conn(NMConnection *connection)
        {
            if(connection == NULL)
                return false;
            NMSettingIPConfig *ipConfig = nm_connection_get_setting_ip4_config(connection);
            if(ipConfig)
            {
                const char* ipConfMethod = nm_setting_ip_config_get_method (ipConfig);
                if(ipConfMethod != NULL && g_strcmp0(ipConfMethod, "auto") == 0)
                    return true;
                else
                    NMLOG_WARNING("ip configuration: %s", ipConfMethod != NULL? ipConfMethod: "null");
            }

            return false;
        }

        bool wifiManager::setIpSettings(const string interface, const Exchange::INetworkManager::IPAddress address)
        {
            m_isSuccess = false;
            NMConnection *connection = NULL;
            NMRemoteConnection *remoteConn = NULL;
            NMActiveConnection* activeConnection = NULL;
            NMSetting *setting = NULL;
            NMDevice *device = NULL;
            const char *specObject = NULL;

            if(interface.empty() || (interface != nmUtils::ethIface() && interface != nmUtils::wlanIface()))
            {
                NMLOG_ERROR("Invalid interface name: %s", interface.c_str());
                return false;
            }

            if (!createClientNewConnection())
                return false;

            device = nm_client_get_device_by_iface(m_client, interface.c_str());
            if(device == NULL)
            {
                deleteClientConnection();
                return false;
            }

            if(interface == nmUtils::ethIface())
            {
                NMSettingConnection *settings = NULL;
                if(device == NULL)
                {
                    deleteClientConnection();
                    return false;
                }

                const GPtrArray *connections = nm_device_get_available_connections(device);
                if (connections == NULL || connections->len == 0)
                {
                    NMLOG_WARNING("no connections availble to edit ");
                    deleteClientConnection();
                    return true;
                }

                for (guint i = 0; i < connections->len; i++)
                {
                    NMConnection *tmpConn = NM_CONNECTION(connections->pdata[i]);
                    if(tmpConn == nullptr)
                        continue;
                    settings = nm_connection_get_setting_connection(tmpConn);
                    if(settings == NULL)
                        continue;
                    if (g_strcmp0(nm_setting_connection_get_interface_name(settings), interface.c_str()) == 0) {
                        connection = tmpConn;

                        if (NM_IS_REMOTE_CONNECTION(connection)) {
                            remoteConn = NM_REMOTE_CONNECTION(connection);
                        } else {
                            NMLOG_ERROR("The connection is not a remote connection.");
                            deleteClientConnection();
                            return false; /* connection and remoteconnection should match */
                        }

                        NMLOG_DEBUG("ethernet connection found "); 
                        break;
                    }
                }
            }
            else if(interface == nmUtils::wlanIface())
            {
                /* for wifi we need active connection to modify becuse of multiple wifi connection */
                activeConnection = nm_device_get_active_connection(device);
                if(activeConnection == NULL)
                {
                    NMLOG_WARNING("no active connection for wifi");
                    deleteClientConnection();
                    return false;
                }
                remoteConn = nm_active_connection_get_connection(activeConnection);
                if(remoteConn == NULL) {
                    NMLOG_WARNING("no remote connection for wifi");
                    deleteClientConnection();
                    return false;
                }
                connection = NM_CONNECTION(remoteConn);
            }

            if(connection == nullptr)
            {
                NMLOG_WARNING("not a single connection availble for %s", interface.c_str());
                deleteClientConnection();
                return true;
            }

            if(address.autoconfig)
            {
                NMSettingIP4Config *sIp4 = nullptr;
                NMSettingIP6Config *sIp6 = nullptr;
                if(address.ipversion.empty() || nmUtils::caseInsensitiveCompare("IPv4", address.ipversion))
                {
                    if(checkAutoConnectEnabledInIPv4Conn(connection)) // already auto connect true connection
                    {
                        NMLOG_INFO("Setting IPv4 auto connect enabled");
                        deleteClientConnection();
                        return true; // no need to modify
                    }
                    sIp4 = (NMSettingIP4Config *)nm_setting_ip4_config_new();
                    g_object_set(G_OBJECT(sIp4), NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL);
                    nm_connection_add_setting(connection, NM_SETTING(sIp4));
                }
                else
                {
                    sIp6 = (NMSettingIP6Config *)nm_setting_ip6_config_new();
                    g_object_set(G_OBJECT(sIp6), NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_AUTO, NULL);
                    nm_connection_add_setting(connection, NM_SETTING(sIp6));
                }
            }
            else
            {
                if (nmUtils::caseInsensitiveCompare("IPv4", address.ipversion))
                {
                    NMSettingIPConfig *ip4Config = nm_connection_get_setting_ip4_config(connection);
                    if (ip4Config == nullptr) 
                    {
                        ip4Config = (NMSettingIPConfig *)nm_setting_ip4_config_new();
                    }
                    NMIPAddress *ipAddress;
                    setting = nm_connection_get_setting_by_name(connection, "ipv4");
                    ipAddress = nm_ip_address_new(AF_INET, address.ipaddress.c_str(), address.prefix, NULL);
                    nm_setting_ip_config_clear_addresses(ip4Config);
                    nm_setting_ip_config_add_address(NM_SETTING_IP_CONFIG(setting), ipAddress);
                    nm_setting_ip_config_clear_dns(ip4Config);
                    if(!address.primarydns.empty())
                        nm_setting_ip_config_add_dns(ip4Config, address.primarydns.c_str());
                    if(!address.secondarydns.empty())
                        nm_setting_ip_config_add_dns(ip4Config, address.secondarydns.c_str());

                    g_object_set(G_OBJECT(ip4Config),NM_SETTING_IP_CONFIG_GATEWAY, address.gateway.c_str(), NULL);
                    g_object_set(G_OBJECT(ip4Config),NM_SETTING_IP_CONFIG_NEVER_DEFAULT, FALSE, NULL);
                    g_object_set(G_OBJECT(ip4Config), NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_MANUAL, NULL);
                }
                else
                {
                    //FIXME : Add IPv6 support here
                    NMLOG_WARNING("Setting IPv6 is not supported at this point in time. This is just a place holder");
                }
            }

            // TODO fix depricated api
            GError *error = NULL;

            if (!nm_remote_connection_commit_changes(remoteConn, FALSE, NULL, &error)) {
                if (error) {
                    NMLOG_ERROR("Failed to commit changes to the remote connection: %s", error->message);
                    g_error_free(error); // Free the GError object after handling it
                } else {
                    NMLOG_ERROR("Failed to commit changes to the remote connection (unknown error).");
                }
                deleteClientConnection();
                return false;
            }

            if (activeConnection != NULL) {
                specObject = nm_object_get_path(NM_OBJECT(activeConnection));
                GError *deactivate_error = NULL;
                // TODO fix depricated api
                if (!nm_client_deactivate_connection(m_client, activeConnection, NULL, &deactivate_error)) {
                    NMLOG_ERROR("Failed to deactivate connection: %s", deactivate_error->message);
                    g_clear_error(&deactivate_error);
                    deleteClientConnection();
                    return false;
                }
            }

            nm_client_activate_connection_async(m_client, connection, device, specObject, NULL, onActivateComplete, this);
            wait(m_loop);
            deleteClientConnection();
            return m_isSuccess;
        }

        bool wifiManager::setPrimaryInterface(const string interface)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            GError *error = NULL;
            std::string otherInterface;
            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();
            const char *specObject = NULL;
            if (!createClientNewConnection())                                                                                     return false;
                                                                                                                              if(interface.empty() || (wifiname != interface && ethname != interface))
            {                                                                                                                     NMLOG_FATAL("interface is not valied %s", interface.c_str()!=nullptr? interface.c_str():"empty");
                deleteClientConnection();
                return false;                                                                                                 }
            otherInterface = (interface == wifiname)?ethname:wifiname;

            // Retrieve the active connections by interface name
            const GPtrArray* activeConnections = nm_client_get_active_connections(m_client);
            if (!activeConnections) {
                NMLOG_ERROR("Failed to retrieve active connections");
                deleteClientConnection();
                return false;
            }
            for (guint i = 0; i < activeConnections->len; i++)
            {
                gpointer elem = g_ptr_array_index(activeConnections, i);
                if (!elem || !NM_IS_ACTIVE_CONNECTION(elem)) {
                    NMLOG_WARNING("Invalid or null active connection at index %u", i);
                    continue;
                }
                NMActiveConnection *activeConnection = NM_ACTIVE_CONNECTION(elem);
                const GPtrArray* activeDevices = nm_active_connection_get_devices(activeConnection);
                for (guint j = 0; j < activeDevices->len; j++)
                {
                    NMDevice* device = NM_DEVICE(g_ptr_array_index(activeDevices, j));
                    const char *iface = nm_device_get_iface(device);
                    if (g_strcmp0(iface, interface.c_str()) == 0 || g_strcmp0(iface, otherInterface.c_str()) == 0)                    {
                        NMRemoteConnection* connection = NM_REMOTE_CONNECTION(nm_active_connection_get_connection(activeConnection));
                        NMSettingIPConfig *s_ip4 = nm_connection_get_setting_ip4_config(NM_CONNECTION(connection));
                        NMSettingIPConfig *s_ip6 = nm_connection_get_setting_ip6_config(NM_CONNECTION(connection));
                        if (!s_ip4 || !s_ip6)
                        {
                            NMLOG_ERROR("Failed to retrieve IP settings for connection on interface: %s\n", iface);
                            continue;
                        }

                        int metric_value = (g_strcmp0(iface, interface.c_str()) == 0) ? ROUTE_METRIC_PRIORITY_HIGH:ROUTE_METRIC_PRIORITY_LOW;
                        g_object_set(G_OBJECT(s_ip4),NM_SETTING_IP_CONFIG_ROUTE_METRIC, metric_value, NULL);
                        g_object_set(G_OBJECT(s_ip6),NM_SETTING_IP_CONFIG_ROUTE_METRIC, metric_value, NULL);
                        // TODO fix depricated api
                        if (!nm_remote_connection_commit_changes(connection, FALSE, NULL, &error))
                        {
                            if (error)                                                                                                        {
                                NMLOG_ERROR("Failed to commit changes to the remote connection: %s", error->message);
                                g_error_free(error); // Free the GError object after handling it
                            }
                            else                                                                                                              {
                                NMLOG_ERROR("Failed to commit changes to the remote connection (unknown error).");                            }
                            deleteClientConnection();
                            return false;
                        }
                        if (activeConnection != NULL)
                        {
                            specObject = nm_object_get_path(NM_OBJECT(activeConnection));
                            GError *deactivate_error = NULL;
                            // TODO fix depricated api
                            if (!nm_client_deactivate_connection(m_client, activeConnection, NULL, &deactivate_error))
                            {
                                NMLOG_ERROR("Failed to deactivate connection: %s", deactivate_error->message);
                                g_clear_error(&deactivate_error);
                                deleteClientConnection();
                                return false;
                            }
                        }
                        nm_client_activate_connection_async(m_client, NM_CONNECTION(connection), device, specObject, NULL, onActivateComplete, this);
                    }
                }
            }
            wait(m_loop);
            deleteClientConnection();
            return rc;
        }
    } // namespace Plugin
} // namespace WPEFramework

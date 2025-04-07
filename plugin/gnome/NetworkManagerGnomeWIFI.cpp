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
            if(m_client != nullptr)
            {
                g_object_unref(m_client);
                m_client = nullptr;
            }

            m_client = nm_client_new(NULL, &error);
            if (!m_client || !m_loop) {
                NMLOG_ERROR("Could not connect to NetworkManager: %s.", error->message);
                g_error_free(error);
                return false;
            }
            return true;
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
            GBytes *ssid;
            NMDeviceState device_state = nm_device_get_state(device);
            if (device_state == NM_DEVICE_STATE_ACTIVATED)
            {
                NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(device));
                ssid = nm_access_point_get_ssid(activeAP);
                gsize size;
                const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssid, &size));
                std::string ssidTmp(reinterpret_cast<const char *>(ssidData), size);
                ssidin = ssidTmp;
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
            if (ssid) {
                gsize size;
                const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssid, &size));
                if(size<=32)
                {
                    std::string ssidTmp(reinterpret_cast<const char *>(ssidData), size);
                    wifiInfo.ssid = ssidTmp;
                    // NMLOG_INFO("ssid: %s", wifiInfo.ssid.c_str());
                }
                else
                {
                    NMLOG_ERROR("Invallied ssid length Error");
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
            if(noise <= 0 && noise >= DEFAULT_NOISE)
                wifiInfo.noise = std::to_string(noise);
            else
                wifiInfo.noise = std::to_string(0);
            NMLOG_DEBUG("bitrate : %s kbit/s", wifiInfo.rate.c_str());
            //TODO signal strenght to dBm
            wifiInfo.strength = std::string(nmUtils::convertPercentageToSignalStrengtStr(strength));
            wifiInfo.security = static_cast<Exchange::INetworkManager::WIFISecurityMode>(nmUtils::wifiSecurityModeFromAp(wifiInfo.ssid, flags, wpaFlags, rsnFlags, doPrint));
            if(doPrint)
            {
                NMLOG_INFO("ssid: %s, frequency: %s, sterngth: %s, security: %u", wifiInfo.ssid.c_str(), wifiInfo.frequency.c_str(), wifiInfo.strength.c_str(), wifiInfo.security);
                NMLOG_INFO("Mode: %s", mode == NM_802_11_MODE_ADHOC   ? "Ad-Hoc": mode == NM_802_11_MODE_INFRA ? "Infrastructure": "Unknown");
            }
            else
            {
                NMLOG_DEBUG("ssid: %s, frequency: %s, sterngth: %s, security: %u", wifiInfo.ssid.c_str(), wifiInfo.frequency.c_str(), wifiInfo.strength.c_str(), wifiInfo.security);
                NMLOG_DEBUG("Mode: %s", mode == NM_802_11_MODE_ADHOC   ? "Ad-Hoc": mode == NM_802_11_MODE_INFRA ? "Infrastructure": "Unknown");
            }
        }

        bool wifiManager::isWifiConnected()
        {
            if(!createClientNewConnection())
                return false;

            NMDeviceWifi *wifiDevice = NM_DEVICE_WIFI(getWifiDevice());
            if(wifiDevice == NULL) {
                NMLOG_FATAL("NMDeviceWifi * NULL !");
                return false;
            }

            NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(wifiDevice);
            if(activeAP == NULL) {
                NMLOG_DEBUG("No active access point found !");
                return false;
            }
            else
                NMLOG_DEBUG("active access point found !");
            return true;
        }

        bool wifiManager::wifiConnectedSSIDInfo(Exchange::INetworkManager::WiFiSSIDInfo &ssidinfo)
        {
            if(!createClientNewConnection())
                return false;

            NMDevice* wifiDevice = getWifiDevice();
            if(wifiDevice == NULL) {
                NMLOG_FATAL("NMDeviceWifi * NULL !");
                return false;
            }

            NMDeviceState deviceState = nm_device_get_state(wifiDevice);
            if(deviceState >= NM_DEVICE_STATE_IP_CONFIG)
            {
                NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(wifiDevice));
                if(activeAP == NULL) {
                    NMLOG_ERROR("NMAccessPoint = NULL !");
                    return false;
                }
                NMLOG_DEBUG("active access point found !");
                getApInfo(activeAP, ssidinfo, false);
                return true;
            }
            else
                NMLOG_WARNING("no active access point!; wifi device state: (%d)", deviceState);

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
                return true;
            }

            NMLOG_DEBUG("wifi device current state is %d !", deviceState);
            deviceState = nm_device_get_state(wifiNMDevice);
            if (deviceState <= NM_DEVICE_STATE_DISCONNECTED || deviceState == NM_DEVICE_STATE_FAILED || deviceState == NM_DEVICE_STATE_DEACTIVATING)
            {
                NMLOG_WARNING("wifi already disconnected !");
                return true;
            }

            nm_device_disconnect_async(wifiNMDevice, NULL, disconnectCb, this);
            wait(m_loop);
            return m_isSuccess;
        }

        static NMAccessPoint* findMatchingSSID(const GPtrArray* ApList, std::string& ssid)
        {
            NMAccessPoint *AccessPoint = NULL;
            if(ssid.empty())
                return NULL;

            for (guint i = 0; i < ApList->len; i++)
            {
                NMAccessPoint *ap = static_cast<NMAccessPoint *>(g_ptr_array_index(ApList, i));
                GBytes *ssidGBytes;
                ssidGBytes = nm_access_point_get_ssid(ap);
                if (!ssidGBytes)
                    continue;
                gsize size;
                const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssidGBytes, &size));
                std::string ssidstr(reinterpret_cast<const char *>(ssidData), size);
                // NMLOG_DEBUG("ssid <  %s  >", ssidstr.c_str());
                if (ssid == ssidstr)
                {
                    AccessPoint = ap;
                    break;
                }
            }

            return AccessPoint;
        }

        static void wifiConnectCb(GObject *client, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));

            if (_wifiManager->m_createNewConnection) {
                NMLOG_DEBUG("nm_client_add_and_activate_connection_finish");
                nm_client_add_and_activate_connection_finish(NM_CLIENT(_wifiManager->m_client), result, &error);
                 _wifiManager->m_isSuccess = true;
            }
            else {
                NMLOG_DEBUG("nm_client_activate_connection_finish ");
                nm_client_activate_connection_finish(NM_CLIENT(_wifiManager->m_client), result, &error);
                 _wifiManager->m_isSuccess = true;
            }

            if (error) {
                 _wifiManager->m_isSuccess = false;
                if (_wifiManager->m_createNewConnection) {
                    NMLOG_ERROR("Failed to add/activate new connection: %s", error->message);
                } else {
                    NMLOG_ERROR("Failed to activate connection: %s", error->message);
                }
            }

            g_main_loop_quit(_wifiManager->m_loop);
        }

        static void wifiConnectionUpdate(GObject *rmObject, GAsyncResult *res, gpointer user_data)
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
                _wifiManager->quit(NULL);
                return;
            }
            _wifiManager->m_createNewConnection = false; // no need to create new connection
            nm_client_activate_connection_async(
                _wifiManager->m_client, NM_CONNECTION(remote_con), _wifiManager->m_wifidevice, _wifiManager->m_objectPath, NULL, wifiConnectCb, _wifiManager);
        }

        static bool connectionBuilder(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo, NMConnection *m_connection, bool iswpsAP = false)
        {
            if(ssidinfo.ssid.empty() || ssidinfo.ssid.length() > 32)
            {
                NMLOG_WARNING("ssid name is missing or invalied");
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

            /* Build up the '802-11-wireless-security' settings */
            NMSettingWireless *sWireless = NULL;
            sWireless = (NMSettingWireless *)nm_setting_wireless_new();
            nm_connection_add_setting(m_connection, NM_SETTING(sWireless));
            GBytes *ssid = g_bytes_new(ssidinfo.ssid.c_str(), strlen(ssidinfo.ssid.c_str()));
            g_object_set(G_OBJECT(sWireless), NM_SETTING_WIRELESS_SSID, ssid, NULL); // ssid in Gbyte
            g_object_set(G_OBJECT(sWireless), NM_SETTING_WIRELESS_MODE, NM_SETTING_WIRELESS_MODE_INFRA, NULL); // infra mode
            if(!iswpsAP) // wps never be a hidden AP, it will be always visible
                g_object_set(G_OBJECT(sWireless), NM_SETTING_WIRELESS_HIDDEN, true, NULL); // hidden = true 
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

        bool wifiManager::activateKnownWifiConnection(std::string knownssid)
        {
            NMAccessPoint *AccessPoint = NULL;
            const GPtrArray *wifiConnections = NULL;
            NMConnection *m_connection = NULL;
            const GPtrArray* ApList = NULL;
            const char* specificObjPath = "/";

            if(!createClientNewConnection())
                return false;

            NMDevice *m_wifidevice = getWifiDevice();
            if(m_wifidevice == NULL) {
                NMLOG_WARNING("wifi state is unmanaged !");
                return false;
            }

            nm_device_set_autoconnect(m_wifidevice, true); // set autoconnect true

            if(knownssid.empty())
            {
                NMLOG_WARNING("ssid not specified !");
                return false;
            }

            wifiConnections = nm_device_get_available_connections(m_wifidevice);
            if(wifiConnections == NULL)
            {
                NMLOG_WARNING("No wifi connection found !");
                return false;
            }

            for (guint i = 0; i < wifiConnections->len; i++)
            {
                NMConnection *connection = static_cast<NMConnection*>(g_ptr_array_index(wifiConnections, i));
                if(connection == NULL)
                    continue;

                const char *connId = nm_connection_get_id(NM_CONNECTION(connection));
                if (connId != NULL && strcmp(connId, knownssid.c_str()) == 0)
                {
                    m_connection = g_object_ref(connection);
                    NMLOG_DEBUG("connection '%s' exists !", knownssid.c_str());
                    if (m_connection == NULL)
                    {
                        NMLOG_ERROR("m_connection == NULL smothing went worng");
                        return false;
                    }
                    break;
                }
            }

            m_isSuccess = false;
            if (m_connection != NULL && NM_IS_REMOTE_CONNECTION(m_connection))
            {
                NMLOG_INFO("activating known wifi '%s' connection", knownssid.c_str());
                m_createNewConnection = false; // no need to create new connection
                nm_client_activate_connection_async(m_client, NM_CONNECTION(m_connection), m_wifidevice, specificObjPath, NULL, wifiConnectCb, this);
                wait(m_loop);
            }
            else
            {
                NMLOG_ERROR("'%s' connection not found !",  knownssid.c_str());
            }

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
                return false;

            if(getConnectedSSID(m_wifidevice, activeSSID))
            {
                if(ssidInfo.ssid == activeSSID)
                {
                    NMLOG_INFO("'%s' already connected !", activeSSID.c_str());
                    _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_CONNECTED);
                    return true;
                }
                else
                    NMLOG_DEBUG("wifi already connected with %s AP", activeSSID.c_str());
            }

            ApList = nm_device_wifi_get_access_points(NM_DEVICE_WIFI(m_wifidevice));
            if(ApList == NULL)
            {
                NMLOG_ERROR("Aplist Error !");
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
                    return activateKnownWifiConnection(ssidInfo.ssid);
                }
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
                                if (error)
                                    NMLOG_ERROR("deleting connection failed %s", error->message);
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
                    return false;
                }
                m_createNewConnection = true;
                nm_client_add_and_activate_connection_async(m_client, m_connection, m_wifidevice, m_objectPath, NULL, wifiConnectCb, this);
            }

            wait(m_loop);
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
            _wifiManager->quit(NULL);
        }

        static void addToKnownSSIDsCb(GObject *client, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            GVariant **outResult = NULL;
            if (!nm_client_add_connection2_finish(NM_CLIENT(client), result, outResult, &error)) {
                NMLOG_ERROR("AddToKnownSSIDs Failed");
                _wifiManager->m_isSuccess = false;
            }
            else
            {
                NMLOG_INFO("AddToKnownSSIDs success");
                _wifiManager->m_isSuccess = true;
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
                return false;

            const GPtrArray  *availableConnections = nm_device_get_available_connections(device);
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
            return m_isSuccess;
        }

        bool wifiManager::removeKnownSSID(const string& ssid)
        {
            NMConnection *m_connection = NULL;
            bool ssidSpecified = false;

            if(!createClientNewConnection())
                return false;

            if(!ssid.empty())
                ssidSpecified = true;
            else
                NMLOG_WARNING("ssid is not specified, Deleting all availble wifi connection !");

            const GPtrArray  *allconnections = nm_client_get_connections(m_client);
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
                }
            }

            if(!m_connection)
            {
                if(ssidSpecified)
                    NMLOG_WARNING("'%s' no such connection profile", ssid.c_str());
                else
                    NMLOG_WARNING("No wifi connection profiles found !!"); 
            }

            return true;
        }

        bool wifiManager::getKnownSSIDs(std::list<string>& ssids)
        {
            if(!createClientNewConnection())
                return false;
            const GPtrArray *connections = nm_client_get_connections(m_client);
            std::string ssidPrint;
            for (guint i = 0; i < connections->len; i++)
            {
                NMConnection *connection = NM_CONNECTION(connections->pdata[i]);

                if (NM_IS_SETTING_WIRELESS(nm_connection_get_setting_wireless(connection)))
                {
                    GBytes *ssidBytes = nm_setting_wireless_get_ssid(nm_connection_get_setting_wireless(connection));
                    if (ssidBytes)
                    {
                        gsize ssidSize;
                        const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssidBytes, &ssidSize));
                        std::string ssidstr(reinterpret_cast<const char *>(ssidData), ssidSize);
                        if (!ssidstr.empty())
                        {
                            ssids.push_back(ssidstr);
                            ssidPrint += ssidstr;
                            ssidPrint += ", ";
                        }
                    }
                }
            }
            if (!ssids.empty())
            {
                NMLOG_INFO("known wifi connections are %s", ssidPrint.c_str());
                return true;
            }

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
            }
            else {
                nm_device_wifi_request_scan_async(wifiDevice, NULL, wifiScanCb, this);
            }
            wait(m_loop);
            return m_isSuccess;
        }

        bool wifiManager::isWifiScannedRecently(int timelimitInSec)
        {
            if (!createClientNewConnection())
                return false;

            NMDeviceWifi *wifiDevice = NM_DEVICE_WIFI(getWifiDevice());
            if (wifiDevice == NULL) {
                NMLOG_ERROR("Invalid Wi-Fi device.");
                return false;
            }

            gint64 last_scan_time = nm_device_wifi_get_last_scan(wifiDevice);
            if (last_scan_time <= 0) {
                NMLOG_INFO("No scan has been performed yet");
                return false;
            }

            gint64 current_time_in_msec = nm_utils_get_timestamp_msec();
            gint64 time_difference_in_seconds = (current_time_in_msec - last_scan_time) / 1000;
            if (time_difference_in_seconds <= timelimitInSec) {
                return true;
            }
            return false;
        }

        static void wpsWifiConnectCb(GObject *client, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            GMainLoop *loop = static_cast<GMainLoop *>(user_data);
            nm_client_add_and_activate_connection_finish(NM_CLIENT(client), result, &error);

            if (error)
                NMLOG_ERROR("Failed to add/activate new connection: %s", error->message);

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

        static bool findWpsPbcSSID(const GPtrArray* ApList, std::string& wpsApSsid, NMAccessPoint** wpsAp)
        {
            Exchange::INetworkManager::WiFiSSIDInfo wpsApInfo{};
            for (guint i = 0; i < ApList->len; i++)
            {
                NMAccessPoint *Ap = static_cast<NMAccessPoint *>(g_ptr_array_index(ApList, i));
                guint32 flags = nm_access_point_get_flags(Ap);
                if (flags & NM_802_11_AP_FLAGS_WPS_PBC)
                {
                    *wpsAp = Ap;
                    const char *bssid = nm_access_point_get_bssid(Ap);
                    if (bssid != NULL) {
                        NMLOG_INFO("WPS PBC AP found bssid = %s", bssid);
                    }
                    
                    getApInfo(Ap, wpsApInfo, false);
                    wpsApSsid = wpsApInfo.ssid;
                    return true;
                }
            }

            NMLOG_INFO("WPS PBC Not found !!!");
            return false;
        }

        static void wpsProcess()
        {
            wpsProcessRun = true;
            GError *error = NULL;
            const GPtrArray* ApList = NULL;
            std::string wpsApSsid{};
            GMainContext *wpsContext = NULL;
            NMAccessPoint *wpsAp = NULL;
            GMainLoop *loop = NULL;
            Exchange::INetworkManager::WiFiConnectTo ssidInfo{};
            bool wpsComplete= false;

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

                NMClient* client = nm_client_new(NULL, &error);
                if (!client && error != NULL) {
                    NMLOG_ERROR("Could not connect to NetworkManager: %s.", error->message);
                    g_error_free(error);
                    break;
                }

                NMDevice* wifidevice = nm_client_get_device_by_iface(client, nmUtils::wlanIface());
                if(wifidevice == NULL)
                {
                    NMLOG_ERROR("Failed to get device list.");
                    break;
                }

                ApList = nm_device_wifi_get_access_points(NM_DEVICE_WIFI(wifidevice));
                if(ApList == NULL)
                {
                    NMLOG_ERROR("Aplist Error !");
                    break;
                }

                if(findWpsPbcSSID(ApList, wpsApSsid, &wpsAp) && wpsAp != NULL)
                {
                    Exchange::INetworkManager::WiFiSSIDInfo activeApInfo{};
                    NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(NM_DEVICE_WIFI(wifidevice));
                    if(activeAP != NULL) {
                        NMLOG_DEBUG("active access point found !");
                        getApInfo(activeAP, activeApInfo);
                        if(activeApInfo.ssid == wpsApSsid)
                        {
                            NMLOG_INFO("WPS process stoped connected to '%s' wps ap ", wpsApSsid.c_str());
                            wpsComplete = true;
                        }

                        NMDeviceState state = nm_device_get_state(wifidevice);
                        if(!wpsComplete && state > NM_DEVICE_STATE_DISCONNECTED)
                        {
                            NMLOG_INFO("stopping the ongoing Wi-Fi connection");
                            // some other ssid connected or connecting; wps need a disconnected wifi state
                            nm_device_disconnect(wifidevice, NULL,  &error);
                            if (error)
                                NMLOG_ERROR("disconnect connection failed %s", error->message);
                        }
                    }

                    if(wpsComplete)
                        break;

                    ssidInfo.ssid = wpsApSsid;
                    NMConnection* connection = NULL;
                    /* if same connection name exsist we remove and add new one */
                    removeSameNmConnection(wifidevice, wpsApSsid);
                    NMLOG_DEBUG("creating new connection '%s' ", wpsApSsid.c_str());
                    connection = nm_simple_connection_new();
                    const char* apObjPath = nm_object_get_path(NM_OBJECT(wpsAp));
                    if(!connectionBuilder(ssidInfo, connection, true))
                    {
                        NMLOG_ERROR("wps connection builder failed");
                        return;
                    }

                    loop = g_main_loop_new(wpsContext, FALSE);
                    nm_client_add_and_activate_connection_async(client, connection, wifidevice, apObjPath, NULL, wpsWifiConnectCb, loop);
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
                    wpsComplete = true;
                }
                else
                {
                    //TODO Post SSID lost event ?
                    nm_device_wifi_request_scan(NM_DEVICE_WIFI(wifidevice), NULL, &error);
                }

                g_main_context_pop_thread_default(wpsContext);

                if(wpsComplete)
                    break;

                g_main_context_unref(wpsContext);
            }

            if(wpsAp == NULL)
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

            wpsProcessRun = false;
            NMLOG_INFO("WPS process thread exist");
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
            std::thread wpsthread(&wpsProcess);
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
                return false;

            NMDeviceState deviceState = nm_device_get_state(device);

            if (enabled) {
                NMLOG_DEBUG("Enabling interface...");
                if (deviceState >= NM_DEVICE_STATE_DISCONNECTED) // already enabled
                    return true;
            } else {
                NMLOG_DEBUG("Disabling interface...");
                if (deviceState <= NM_DEVICE_STATE_UNMANAGED) // already disabled
                    return true;

                else if (deviceState > NM_DEVICE_STATE_DISCONNECTED) {
                    nm_device_disconnect_async(device, nullptr, disconnectCb, this);
                    wait(m_loop);
                    sleep(1); // to remove the connection
                }
            }

            const char *objectPath = nm_object_get_path(NM_OBJECT(device));
            GVariant *value = g_variant_new_boolean(enabled);

            nm_client_dbus_set_property( m_client, objectPath, NM_DBUS_INTERFACE_DEVICE,"Managed",
                                                                    value, -1, nullptr, deviceManagedCb, this);
            wait(m_loop);
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
                return false;

            if(interface == nmUtils::ethIface())
            {
                NMSettingConnection *settings = NULL;
                if(device == NULL)
                    return false;

                const GPtrArray *connections = nm_device_get_available_connections(device);
                if (connections == NULL || connections->len == 0)
                {
                    NMLOG_WARNING("no connections availble to edit ");
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
                    return false;
                }
                remoteConn = nm_active_connection_get_connection(activeConnection);
                if(remoteConn == NULL) {
                    NMLOG_WARNING("no remote connection for wifi");
                    return false;
                }
                connection = NM_CONNECTION(remoteConn);
            }

            if(connection == nullptr)
            {
                NMLOG_WARNING("not a single connection availble for %s", interface.c_str());
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
                return false;
            }

            if (activeConnection != NULL) {
                specObject = nm_object_get_path(NM_OBJECT(activeConnection));
                GError *deactivate_error = NULL;
                // TODO fix depricated api
                if (!nm_client_deactivate_connection(m_client, activeConnection, NULL, &deactivate_error)) {
                    NMLOG_ERROR("Failed to deactivate connection: %s", deactivate_error->message);
                    g_clear_error(&deactivate_error);
                    return false;
                }
            }

            nm_client_activate_connection_async(m_client, connection, device, specObject, NULL, onActivateComplete, this);
            wait(m_loop);
            return m_isSuccess;
        }
    } // namespace Plugin
} // namespace WPEFramework

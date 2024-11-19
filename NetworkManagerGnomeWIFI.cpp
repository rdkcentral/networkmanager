/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 */
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
    class Job : public Core::IDispatch {
    public:
        Job(function<void()> work)
        : _work(work)
        {
        }
        void Dispatch() override
        {
            _work();
        }

    private:
        function<void()> _work;
    };
    namespace Plugin
    {
        extern NetworkManagerImplementation* _instance;

        wifiManager::wifiManager() : client(nullptr), loop(nullptr), createNewConnection(false) {
            NMLOG_INFO("wifiManager");
            nmContext = g_main_context_new();
            g_main_context_push_thread_default(nmContext);
            loop = g_main_loop_new(nmContext, FALSE);
        }

        bool wifiManager::createClientNewConnection()
        {
            GError *error = NULL;
            if(client != nullptr)
            {
                g_object_unref(client);
                client = nullptr;
            }

            client = nm_client_new(NULL, &error);
            if (!client || !loop) {
                NMLOG_ERROR("Could not connect to NetworkManager: %s.", error->message);
                g_error_free(error);
                return false;
            }
            return true;
        }

        bool wifiManager::quit(NMDevice *wifiNMDevice)
        {
            if (wifiNMDevice && wifiDeviceStateGsignal > 0) {
                g_signal_handler_disconnect(wifiNMDevice, wifiDeviceStateGsignal);
                wifiDeviceStateGsignal = 0;
            }

            if(!g_main_loop_is_running(loop)) {
                NMLOG_ERROR("g_main_loop_is not running");
                return false;
            }

            g_main_loop_quit(loop);
            return false;
        }

        static gboolean gmainLoopTimoutCB(gpointer user_data)
        {
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            NMLOG_WARNING("GmainLoop ERROR_TIMEDOUT");
            _wifiManager->isSuccess = false;
            g_main_loop_quit(_wifiManager->loop);
            return true;
        }
    
        bool wifiManager::wait(GMainLoop *loop, int timeOutMs)
        {
            if(g_main_loop_is_running(loop)) {
                NMLOG_WARNING("g_main_loop_is running");
                return false;
            }
            source = g_timeout_source_new(timeOutMs);  // 10000ms interval
            g_source_set_callback(source, (GSourceFunc)gmainLoopTimoutCB, this, NULL);
            g_source_attach(source, NULL);
            g_main_loop_run(loop);
            if(source != nullptr) {
                if(g_source_is_destroyed(source)) {
                    NMLOG_WARNING("Source has been destroyed");
                }
                else {
                    g_source_destroy(source);
                }
                g_source_unref(source);
            }
            return true;
        }

        NMDevice* wifiManager::getNmDevice()
        {
            NMDevice *wifiDevice = NULL;

            GPtrArray *devices = const_cast<GPtrArray *>(nm_client_get_devices(client));
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

        bool static getConnectedSSID(NMDeviceWifi *wifiDevice, std::string& ssidin)
        {
            GBytes *ssid;
            NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(wifiDevice);
            if(activeAP == NULL) {
                return false;
            }

            ssid = nm_access_point_get_ssid(activeAP);
            gsize size;
            const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssid, &size));
            std::string ssidTmp(reinterpret_cast<const char *>(ssidData), size);
            ssidin = ssidTmp;
            NMLOG_INFO("connected ssid: %s", ssidin.c_str());
            return true;
        }

        static void getApInfo(NMAccessPoint *AccessPoint, Exchange::INetworkManager::WiFiSSIDInfo &wifiInfo)
        {
            guint32     flags, wpaFlags, rsnFlags, freq, bitrate;
            guint8      strength;
            GBytes     *ssid;
            const char *hwaddr;
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

            /* Convert to strings */
            if (ssid) {
                gsize size;
                const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssid, &size));
                if(size<=32)
                {
                    std::string ssidTmp(reinterpret_cast<const char *>(ssidData), size);
                    wifiInfo.ssid = ssidTmp;
                    NMLOG_INFO("ssid: %s", wifiInfo.ssid.c_str());
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
                wifiInfo.ssid = "-----";
                NMLOG_DEBUG("ssid: %s", wifiInfo.ssid.c_str());
            }

            wifiInfo.bssid = (hwaddr != nullptr) ? hwaddr : "-----";
            NMLOG_DEBUG("bssid: %s", wifiInfo.bssid.c_str());
            wifiInfo.frequency = std::to_string((double)freq/1000);
            wifiInfo.rate = std::to_string(bitrate);
            NMLOG_DEBUG("bitrate : %s kbit/s", wifiInfo.rate.c_str());
            //TODO signal strenght to dBm
            wifiInfo.strength = std::string(nmUtils::convertPercentageToSignalStrengtStr(strength));
            NMLOG_DEBUG("sterngth: %s dbm", wifiInfo.strength.c_str());
            wifiInfo.security = static_cast<Exchange::INetworkManager::WIFISecurityMode>(nmUtils::wifiSecurityModeFromAp(flags, wpaFlags, rsnFlags));
            NMLOG_DEBUG("security %s", nmUtils::getSecurityModeString(flags, wpaFlags, rsnFlags).c_str());
            NMLOG_DEBUG("Mode: %s", mode == NM_802_11_MODE_ADHOC   ? "Ad-Hoc": mode == NM_802_11_MODE_INFRA ? "Infrastructure": "Unknown");
        }

        bool wifiManager::isWifiConnected()
        {
            if(!createClientNewConnection())
                return false;

            NMDeviceWifi *wifiDevice = NM_DEVICE_WIFI(getNmDevice());
            if(wifiDevice == NULL) {
                NMLOG_FATAL("NMDeviceWifi * NULL !");
                return false;
            }

            NMAccessPoint *activeAP = nm_device_wifi_get_active_access_point(wifiDevice);
            if(activeAP == NULL) {
                NMLOG_INFO("No active access point found !");
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

            NMDeviceWifi *wifiDevice = NM_DEVICE_WIFI(getNmDevice());
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

            getApInfo(activeAP, ssidinfo);
            return true;
        }

        static void disconnectCb(GObject *object, GAsyncResult *result, gpointer user_data)
        {
            NMDevice     *device = NM_DEVICE(object);
            GError       *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));

            NMLOG_DEBUG("Disconnecting... ");
            _wifiManager->isSuccess = true;
            if (!nm_device_disconnect_finish(device, result, &error)) {
                if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                    return;

                NMLOG_ERROR("Device '%s' (%s) disconnecting failed: %s",
                            nm_device_get_iface(device),
                            nm_object_get_path(NM_OBJECT(device)),
                            error->message);
                g_error_free(error);
                _wifiManager->quit(device);
                 _wifiManager->isSuccess = false;
            }
            _wifiManager->quit(device);
        }

        bool wifiManager::wifiDisconnect()
        {
            if(!createClientNewConnection())
                return false;

            NMDevice *wifiNMDevice = getNmDevice();
            if(wifiNMDevice == NULL) {
                NMLOG_WARNING("wifi state is unmanaged !");
                return true;
            }

            nm_device_disconnect_async(wifiNMDevice, NULL, disconnectCb, this);
            wait(loop);
            return isSuccess;
        }

        static NMAccessPoint *checkSSIDAvailable(NMDevice *device, const char *ssid)
        {
            NMAccessPoint *AccessPoint = NULL;
            const GPtrArray *aps = NULL;
            if(ssid == NULL)
                return NULL;

            aps = nm_device_wifi_get_access_points(NM_DEVICE_WIFI(device));
            for (guint i = 0; i < aps->len; i++)
            {
                NMAccessPoint *ap = static_cast<NMAccessPoint *>(g_ptr_array_index(aps, i));
                GBytes *ssidGBytes;
                ssidGBytes = nm_access_point_get_ssid(ap);
                if (!ssidGBytes)
                    continue;
                gsize size;
                const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssidGBytes, &size));
                std::string ssidstr(reinterpret_cast<const char *>(ssidData), size);
                //g_bytes_unref(ssidGBytes);
                // NMLOG_DEBUG("ssid <  %s  >", ssidstr.c_str());
                if (strcmp(ssid, ssidstr.c_str()) == 0)
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

            if (_wifiManager->createNewConnection) {
                NMLOG_DEBUG("nm_client_add_and_activate_connection_finish");
                nm_client_add_and_activate_connection_finish(NM_CLIENT(_wifiManager->client), result, &error);
                 _wifiManager->isSuccess = true;
            }
            else {
                NMLOG_DEBUG("nm_client_activate_connection_finish ");
                nm_client_activate_connection_finish(NM_CLIENT(_wifiManager->client), result, &error);
                 _wifiManager->isSuccess = true;
            }

            if (error) {
                 _wifiManager->isSuccess = false;
                if (_wifiManager->createNewConnection) {
                    NMLOG_ERROR("Failed to add/activate new connection: %s", error->message);
                } else {
                    NMLOG_ERROR("Failed to activate connection: %s", error->message);
                }
            }

            g_main_loop_quit(_wifiManager->loop);
        }

        static void removeKnownSSIDCb(GObject *client, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            NMRemoteConnection *connection = NM_REMOTE_CONNECTION(client);
            if (!nm_remote_connection_delete_finish(connection, result, &error)) {
                NMLOG_ERROR("RemoveKnownSSID failed %s", error->message);
                _wifiManager->isSuccess = false;
            }
            else
            {
                NMLOG_INFO ("RemoveKnownSSID is success");
                _wifiManager->isSuccess = true;
            }

            _wifiManager->quit(NULL);
        }

        static void wifiConnectionUpdate(GObject *rmObject, GAsyncResult *res, gpointer user_data)
        {
            NMRemoteConnection        *remote_con = NM_REMOTE_CONNECTION(rmObject);
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            GVariant *ret = NULL;
            GError *error = NULL;

            ret = nm_remote_connection_update2_finish(remote_con, res, &error);

            if (!ret) {
                NMLOG_ERROR("Error: %s.", error->message);
                g_error_free(error);
                _wifiManager->isSuccess = false;
                _wifiManager->quit(NULL);
                return;
            }
            _wifiManager->createNewConnection = false; // no need to create new connection
            nm_client_activate_connection_async(
                _wifiManager->client, NM_CONNECTION(remote_con), _wifiManager->wifidevice, _wifiManager->objectPath, NULL, wifiConnectCb, _wifiManager);
        }

        static bool connectionBuilder(const Exchange::INetworkManager::WiFiConnectTo& ssidinfo, NMConnection *m_connection)
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
            g_object_set(G_OBJECT(sWireless), NM_SETTING_WIRELESS_HIDDEN, true, NULL); // hidden = true 
            // 'bssid' parameter is used to restrict the connection only to the BSSID
            // g_object_set(s_wifi, NM_SETTING_WIRELESS_BSSID, bssid, NULL);

            NMSettingWirelessSecurity *sSecurity = NULL;
            switch(ssidinfo.security)
            {
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK_AES:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_WPA2_PSK:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK_TKIP:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_PSK_AES:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_PSK_TKIP:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA3_SAE:
                {
                    if(ssidinfo.passphrase.empty() || ssidinfo.passphrase.length() < 8)
                    {
                        NMLOG_WARNING("password legth should be > 8");
                        return false;
                    }

                    sSecurity = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new();
                    nm_connection_add_setting(m_connection, NM_SETTING(sSecurity));
                    if(Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA3_SAE == ssidinfo.security)
                    {
                        NMLOG_INFO("key-mgmt: %s", "sae");
                        g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,"sae", NULL);
                    }
                    else
                    {
                        NMLOG_INFO("key-mgmt: %s", "wpa-psk");
                        g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,"wpa-psk", NULL);
                    }
                    g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, "open", NULL);
                    g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_PSK, ssidinfo.passphrase.c_str(), NULL);
                    break;
                }
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_ENTERPRISE_TKIP:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_ENTERPRISE_AES:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_ENTERPRISE_TKIP:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_ENTERPRISE_AES:
                case Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_WPA2_ENTERPRISE:
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
                    NMLOG_INFO("key-mgmt: %s", "none");
                    sSecurity = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new();
                    nm_connection_add_setting(m_connection, NM_SETTING(sSecurity));
                    g_object_set(G_OBJECT(sSecurity), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,"none", NULL);
                    NMLOG_WARNING("open wifi network configuration");
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

        bool wifiManager::wifiConnect(Exchange::INetworkManager::WiFiConnectTo ssidInfo)
        {
            NMAccessPoint *AccessPoint = NULL;
            NMConnection *m_connection = NULL;
            const GPtrArray  *availableConnections;
            bool SSIDmatch = false;
            isSuccess = false;

            if(!createClientNewConnection())
                return false;

            NMDevice *device = getNmDevice();
            if(device == NULL)
                return false;

            std::string activeSSID;
            if(getConnectedSSID(NM_DEVICE_WIFI(device), activeSSID))
            {
                if(ssidInfo.ssid == activeSSID)
                {
                    NMLOG_INFO("ssid already connected !");
                    return true;
                }
                else
                    NMLOG_DEBUG("wifi already connected with %s AP", activeSSID.c_str());
            }

            AccessPoint = checkSSIDAvailable(device, ssidInfo.ssid.c_str());
            if(AccessPoint == NULL) {
                NMLOG_WARNING("SSID '%s' not found !", ssidInfo.ssid.c_str());
                if(_instance != nullptr)
                    _instance->ReportWiFiStateChange(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
                return false;
            }

            Exchange::INetworkManager::WiFiSSIDInfo apinfo;
            getApInfo(AccessPoint, apinfo);

            availableConnections = nm_device_get_available_connections(device);
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
                            NMLOG_WARNING(" '%s' connection exist but properties miss match; deleting...", ssidInfo.ssid.c_str());
                            nm_remote_connection_delete_async(NM_REMOTE_CONNECTION(connection),
                                                        NULL,
                                                        removeKnownSSIDCb,
                                                        this);
                        }
                    }
                }
            }

            if (NM_IS_REMOTE_CONNECTION(m_connection))
            {
                if(!connectionBuilder(ssidInfo, m_connection))
                {
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
                objectPath = nm_object_get_path(NM_OBJECT(AccessPoint));
                if(!connectionBuilder(ssidInfo, m_connection))
                {
                    NMLOG_ERROR("connection builder failed");
                    return false;
                }
                createNewConnection = true;
                nm_client_add_and_activate_connection_async(client, m_connection, device, objectPath, NULL, wifiConnectCb, this);
            }

            wait(loop);
            return isSuccess;
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
                _wifiManager->isSuccess = false;
                NMLOG_ERROR("AddToKnownSSIDs failed");
            }
            else
            {
                _wifiManager->isSuccess = true;
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
                _wifiManager->isSuccess = false;
            }
            else
            {
                NMLOG_INFO("AddToKnownSSIDs success");
                _wifiManager->isSuccess = true;
            }

            g_main_loop_quit(_wifiManager->loop);
        }

        bool wifiManager::addToKnownSSIDs(const Exchange::INetworkManager::WiFiConnectTo ssidinfo)
        {
            isSuccess = false;
            NMConnection *m_connection = NULL;

            if(!createClientNewConnection())
                return false;

            NMDevice *device = getNmDevice();
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
                createNewConnection = true;
                GVariant *connSettings = nm_connection_to_dbus(m_connection, NM_CONNECTION_SERIALIZE_ALL);
                nm_client_add_connection2(client,
                                        connSettings,
                                        NM_SETTINGS_ADD_CONNECTION2_FLAG_TO_DISK,
                                        NULL, TRUE, NULL,
                                        addToKnownSSIDsCb, this);
            }
            wait(loop);
            return isSuccess;
        }

        bool wifiManager::removeKnownSSID(const string& ssid)
        {
            NMConnection *m_connection = NULL;
            isSuccess = false;

            if(!createClientNewConnection())
                return false;

            if(ssid.empty())
            {
                NMLOG_ERROR("ssid is empty");
                return false;
            }

            const GPtrArray  *allconnections = nm_client_get_connections(client);
            for (guint i = 0; i < allconnections->len; i++)
            {
                NMConnection *connection = static_cast<NMConnection*>(g_ptr_array_index(allconnections, i));
                if (!NM_IS_SETTING_WIRELESS(nm_connection_get_setting_wireless(connection)))
                    continue; // if not wireless connection skipt
                const char *connId = nm_connection_get_id(NM_CONNECTION(connection));
                if(connId == NULL)
                    continue;
                NMLOG_DEBUG("wireless connection '%s'", connId);
                if (strcmp(connId, ssid.c_str()) == 0)
                {
                    m_connection = g_object_ref(connection);
                    if (NM_IS_REMOTE_CONNECTION(m_connection))
                    {
                        NMLOG_INFO("deleting '%s' connection...", ssid.c_str());
                        nm_remote_connection_delete_async(NM_REMOTE_CONNECTION(m_connection),
                                                    NULL,
                                                    removeKnownSSIDCb,
                                                    this);
                    }
                    wait(loop);
                    break; // multiple connection with same name not handiled
                }
            }

            if(!m_connection)
                NMLOG_INFO("'%s' no such connection profile", ssid.c_str());

            return isSuccess;
        }

        bool wifiManager::getKnownSSIDs(std::list<string>& ssids)
        {
            if(!createClientNewConnection())
                return false;
            const GPtrArray *connections = nm_client_get_connections(client);
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
                NMLOG_DEBUG("known wifi connections are %s", ssidPrint.c_str());
                return true;
            }

            return false;
        }

        static void wifiScanCb(GObject *object, GAsyncResult *result, gpointer user_data)
        {
            GError *error = NULL;
            wifiManager *_wifiManager = (static_cast<wifiManager*>(user_data));
            if(nm_device_wifi_request_scan_finish(NM_DEVICE_WIFI(object), result, &error)) {
                 NMLOG_DEBUG("Scanning success");
                 _wifiManager->isSuccess = true;
            }
            else
            {
                NMLOG_ERROR("Scanning Failed");
                _wifiManager->isSuccess = false;
            }
            if (error) {
                NMLOG_ERROR("Scanning Failed Error: %s.", error->message);
                _wifiManager->isSuccess = false;
                g_error_free(error);
            }

            g_main_loop_quit(_wifiManager->loop);
        }

        bool wifiManager::wifiScanRequest(std::string ssidReq)
        {
            if(!createClientNewConnection())
                return false;
            NMDeviceWifi *wifiDevice = NM_DEVICE_WIFI(getNmDevice());
            if(wifiDevice == NULL) {
                NMLOG_FATAL("NMDeviceWifi * NULL !");
                return false;
            }
            isSuccess = false;
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
                NMLOG_DEBUG("staring normal wifi scanning");
                nm_device_wifi_request_scan_async(wifiDevice, NULL, wifiScanCb, this);
            }
            wait(loop);
            return isSuccess;
        }

        bool wifiManager::isWifiScannedRecently(int timelimitInSec)
        {
            if (!createClientNewConnection())
                return false;

            NMDeviceWifi *wifiDevice = NM_DEVICE_WIFI(getNmDevice());
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

            NMLOG_DEBUG("Current time in milliseconds: %" G_GINT64_FORMAT, current_time_in_msec);
            NMLOG_DEBUG("Last scan time in milliseconds: %" G_GINT64_FORMAT, last_scan_time);
            NMLOG_DEBUG("Time difference in seconds: %" G_GINT64_FORMAT, time_difference_in_seconds);

            if (time_difference_in_seconds <= timelimitInSec) {
                return true;
            }
            NMLOG_DEBUG("Last Wi-Fi scan exceeded time limit.");
            return false;
        }

        bool wifiManager::initiateWPS()
        {
            Core::IWorkerPool::Instance().Submit(Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create([&]() {
            const GPtrArray *aps;
            int count = 1, wpsConnected = 0;
            if(!createClientNewConnection())
                return;

            sleep(10); /* As we will get the ap info with NM_802_11_AP_FLAGS_WPS_PBC set after pressing the PBC button.
                          So we are waiting for 10 seconds here*/
            do{
                if(wifiScanRequest(""))
                {
                    aps = nm_device_wifi_get_access_points(NM_DEVICE_WIFI(getNmDevice()));
                    for (guint i = 0; i < aps->len; i++) {
                        NMAccessPoint *ap = static_cast<NMAccessPoint *>(g_ptr_array_index(aps, i));

                        guint32 flags = nm_access_point_get_flags(ap);

                        NMLOG_INFO("AP Flags: 0x%x\n", flags);

                        if (flags & NM_802_11_AP_FLAGS_WPS_PBC) {
                            Exchange::INetworkManager::WiFiConnectTo wifiData;
                            GBytes *ssid;
                            ssid = nm_access_point_get_ssid(ap);
                            gsize size;
                            const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssid, &size));
                            std::string ssidTmp(reinterpret_cast<const char *>(ssidData), size);
                            wifiData.ssid = ssidTmp.c_str();
                            NMLOG_INFO("connected ssid: %s", ssidTmp.c_str());
                            if(wifiConnect(wifiData))
                                wpsConnected = 1;
                            break;
                        }
                    }
                }
                sleep(3); /* Waiting time between successive scan */
                count++;
            }while(count <= 3 && !wpsConnected);
            NMLOG_INFO("Completed scanning and wpsconnect status = %d", wpsConnected);
            })));
            return true;
        }

        bool wifiManager::cancelWPS()
        {
            return true;
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
                _wifiManager->isSuccess = false;
            } else {
                NMLOG_DEBUG("Successfully set Managed property.");
                _wifiManager->isSuccess = true;
            }

            _wifiManager->quit(nullptr);
        }

        bool wifiManager::setInterfaceState(std::string interface, bool enabled)
        {
            isSuccess = false;
            NMDevice *device = nullptr;

            if (!createClientNewConnection())
                return false;

            GPtrArray *devices = const_cast<GPtrArray *>(nm_client_get_devices(client));
            if (devices == nullptr) {
                NMLOG_ERROR("Failed to get device list.");
                return isSuccess;
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
                    wait(loop);
                    sleep(1); // to remove the connection
                }
            }

            const char *objectPath = nm_object_get_path(NM_OBJECT(device));
            GVariant *value = g_variant_new_boolean(enabled);

            nm_client_dbus_set_property( client, objectPath, NM_DBUS_INTERFACE_DEVICE,"Managed",
                                                                    value, -1, nullptr, deviceManagedCb, this);
            wait(loop);
            return isSuccess;
        }

    } // namespace Plugin
} // namespace WPEFramework

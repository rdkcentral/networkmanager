#include "NetworkManagerImplementation.h"
#include "NetworkManagerGnomeWIFI.h"
#include "NetworkManagerGnomeEvents.h"
#include "NetworkManagerGnomeUtils.h"
#include <libnm/NetworkManager.h>
#include <fstream>
#include <sstream>

static NMClient *client = NULL;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using namespace std;

namespace WPEFramework
{
    namespace Plugin
    {
        wifiManager *wifi = nullptr;
        GnomeNetworkManagerEvents *nmEvent = nullptr;
        const float signalStrengthThresholdExcellent = -50.0f;
        const float signalStrengthThresholdGood = -60.0f;
        const float signalStrengthThresholdFair = -67.0f;
        NetworkManagerImplementation* _instance = nullptr;

        void NetworkManagerInternalEventHandler(const char *owner, int eventId, void *data, size_t len)
        {
            return;
        }

        void NetworkManagerImplementation::platform_init()
        {
            ::_instance = this;
            GError *error = NULL;
            // initialize the NMClient object
            client = nm_client_new(NULL, &error);
            if (client == NULL) {
                NMLOG_DEBUG("Error initializing NMClient: %s", error->message);
                g_error_free(error);
                return;
            }

            nmEvent = GnomeNetworkManagerEvents::getInstance();
            nmEvent->startNetworkMangerEventMonitor();
            wifi = wifiManager::getInstance();
            return;
        }

        uint32_t NetworkManagerImplementation::GetAvailableInterfaces (Exchange::INetworkManager::IInterfaceDetailsIterator*& interfacesItr/* @out */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            NMDeviceType type;
            NMDeviceState state;
            NMDevice *device = NULL;
            static std::vector<Exchange::INetworkManager::InterfaceDetails> interfaceList;

            if(interfaceList.empty())
            {
                std::string interfaces[2];
                if(!nmUtils::GetInterfacesName(interfaces[0], interfaces[1]))
                {
                    NMLOG_WARNING("GetInterface Name Error !");
                    return Core::ERROR_GENERAL;
                }
                for (size_t i = 0; i < 2; i++)
                {
                    if(!interfaces[i].empty())
                    {
                        Exchange::INetworkManager::InterfaceDetails tmp;
                        device = nm_client_get_device_by_iface(client, interfaces[i].c_str());
                        if (device)
                        {
                            if(i == 0)        
                                tmp.type = Exchange::INetworkManager::INTERFACE_TYPE_WIFI;
                            else
                                tmp.type = Exchange::INetworkManager::INTERFACE_TYPE_ETHERNET;
                            tmp.name = interfaces[i].c_str();
                            tmp.mac = nm_device_get_hw_address(device);
                            state = nm_device_get_state(device);
                            tmp.enabled = (state > NM_DEVICE_STATE_UNAVAILABLE) ? true : false;
                            tmp.connected = (state > NM_DEVICE_STATE_DISCONNECTED) ? true : false;
                            interfaceList.push_back(tmp);
                            //g_clear_object(&device);
                        }
                    }
                }
            }

            using Implementation = RPC::IteratorType<Exchange::INetworkManager::IInterfaceDetailsIterator>;
            interfacesItr = Core::Service<Implementation>::Create<Exchange::INetworkManager::IInterfaceDetailsIterator>(interfaceList);
            rc = Core::ERROR_NONE;
            return rc;
        }

        /* @brief Get the active Interface used for external world communication */
        uint32_t NetworkManagerImplementation::GetPrimaryInterface (string& interface /* @out */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            GError *error = NULL;
            NMActiveConnection *activeConn = NULL;
            NMRemoteConnection *remoteConn = NULL;
            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_GENERAL;
            }

            activeConn = nm_client_get_primary_connection(client);
            if (activeConn == NULL) {
                NMLOG_ERROR("No active activeConn Interface found");
                return Core::ERROR_GENERAL;
            }
            remoteConn = nm_active_connection_get_connection(activeConn);
            if(remoteConn == NULL)
            {
                NMLOG_WARNING("remote connection error");
                return Core::ERROR_GENERAL;
            }
            interface.clear();
            const char *ifacePtr = nm_connection_get_interface_name(NM_CONNECTION(remoteConn));
            if(ifacePtr == NULL)
            {
                NMLOG_ERROR("nm_connection_get_interface_name is failed");
                return Core::ERROR_GENERAL;
            }
            interface = ifacePtr;
            if(interface != "eth0" && interface != "wlan0")
            {
                NMLOG_DEBUG("interface name is unknow");
                interface.clear();
            }
            else
                rc = Core::ERROR_NONE;

            return rc;
        }

        /* @brief Set the active Interface used for external world communication */
        uint32_t NetworkManagerImplementation::SetPrimaryInterface (const string& interface/* @in */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            std::string iface = "eth0";
            std::string eth, wifi;
            if(!nmUtils::GetInterfacesName(wifi, eth))
            {
                NMLOG_WARNING("GetInterface Name Error !");
                return Core::ERROR_GENERAL;
            }

            else if(interface == "wlan0" || nmUtils::caseInsensitiveCompare(interface,"WIFI"))
                iface = wifi;
            else if(interface == "eth0" || nmUtils::caseInsensitiveCompare(interface,"ETHERNET"))
                iface = eth;

            NMDevice *device = nm_client_get_device_by_iface(client, iface.c_str());
            if (device == NULL) {
                NMLOG_WARNING("no interface found ");
                return Core::ERROR_GENERAL;
            }
            const GPtrArray *connections = nm_client_get_connections(client);
            NMConnection *conn = NULL;
            NMSettingConnection *settings;
            NMRemoteConnection *remoteConnection;
            for (guint i = 0; i < connections->len; i++) {
                NMConnection *connection = NM_CONNECTION(connections->pdata[i]);
                settings = nm_connection_get_setting_connection(connection);

                /* Check if the interface name matches */
                if (g_strcmp0(nm_setting_connection_get_interface_name(settings), iface.c_str()) == 0) {
                    conn = connection;
                    break;
                }
            }
            if(conn == NULL)
            {
                NMLOG_WARNING("no nm setting available for the interface");
                return Core::ERROR_GENERAL;
            }
            g_object_set(settings,
                    NM_SETTING_CONNECTION_AUTOCONNECT,
                    true,
                    NM_SETTING_CONNECTION_AUTOCONNECT_PRIORITY,
                    NM_SETTING_CONNECTION_AUTOCONNECT_PRIORITY_MAX,
                    NULL);
            const char *uuid = nm_connection_get_uuid(conn);
            remoteConnection = nm_client_get_connection_by_uuid(client, uuid);
            nm_remote_connection_commit_changes(remoteConnection, false, NULL, NULL);

            return rc;
        }

        uint32_t NetworkManagerImplementation::SetInterfaceState(const string& interface/* @in */, const bool enabled /* @in */)
        {
            uint32_t rc = Core::ERROR_NONE;
            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            std::string iface = "eth0";
            std::string eth, wifi;
            if(!nmUtils::GetInterfacesName(wifi, eth))
            {
                NMLOG_WARNING("GetInterface Name Error !");
                return Core::ERROR_GENERAL;
            }

            else if(interface == "wlan0" || nmUtils::caseInsensitiveCompare(interface,"WIFI"))
                iface = wifi;
            else if(interface == "eth0" || nmUtils::caseInsensitiveCompare(interface,"ETHERNET"))
                iface = eth;

            const GPtrArray *devices = nm_client_get_devices(client);
            NMDevice *device = NULL;

            for (guint i = 0; i < devices->len; ++i) {
                device = NM_DEVICE(g_ptr_array_index(devices, i));
                const char *name = nm_device_get_iface(device);
                if (g_strcmp0(name, iface.c_str()) == 0) {
                    nm_device_set_managed(device, enabled);
                    NMLOG_INFO("Interface %s status set to %s", iface.c_str(), enabled ? "Enabled" : "Disabled");
                }
            }

            // if(device)
            //     g_clear_object(&device);
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetInterfaceState(const string& interface/* @in */, bool& isEnabled /* @out */)
        {
            uint32_t rc = Core::ERROR_NONE;
#if 0 //FIXME
            const GPtrArray *devices = nm_client_get_devices(client);
            NMDevice *device = NULL;

            for (guint i = 0; i < devices->len; ++i) {
                device = NM_DEVICE(g_ptr_array_index(devices, i));

                // Get the device details
                const char *name = nm_device_get_iface(device);

                // Check if the device name matches
                if (g_strcmp0(name, interface.c_str()) == 0) {
                    nm_device_set_managed(device, false);

                    NMLOG_DEBUG("Interface %s status set to disabled",
                            interface.c_str());
                }
            }
 
            // Cleanup
            if(device)
                g_clear_object(&device);
#endif
            return rc;
        } 

        /* @brief Get IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::GetIPSettings(string& interface /* @inout */, const string &ipversion /* @in */, IPAddress& result /* @out */) 
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            NMActiveConnection *conn = NULL;
            NMIPConfig *ip4_config = NULL;
            NMIPConfig *ip6_config = NULL;
            const gchar *gateway = NULL;
            char **dns_arr = NULL;
            NMDhcpConfig *dhcp4_config = NULL;
            NMDhcpConfig *dhcp6_config = NULL;
            const char* dhcpserver;
            NMSettingConnection *settings;
            NMIPAddress *address = NULL;
            NMDevice *device = NULL;

            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            std::string iface = "eth0";
            std::string ethIface, wifiIface;
            if(!nmUtils::GetInterfacesName(wifiIface, ethIface))
            {
                NMLOG_WARNING("GetInterface Name Error !");
                return Core::ERROR_GENERAL;
            }

            else if(interface == "wlan0" || nmUtils::caseInsensitiveCompare(interface,"WIFI"))
                iface = wifiIface;
            else if(interface == "eth0" || nmUtils::caseInsensitiveCompare(interface,"ETHERNET"))
                iface = ethIface;
            else
            {
                if(Core::ERROR_NONE != GetPrimaryInterface(iface))
                {
                    NMLOG_WARNING("interface is not specified and default interface get failed");
                    return Core::ERROR_GENERAL;
                }
            }

            device = nm_client_get_device_by_iface(client, iface.c_str());
            if (device == NULL) {
                NMLOG_WARNING("no interface found / wifi not connected no ip found");
                return Core::ERROR_GENERAL;
            }

            NMDeviceState deviceState = NM_DEVICE_STATE_UNKNOWN;
            deviceState = nm_device_get_state(device);
            if(deviceState != NM_DEVICE_STATE_ACTIVATED)
            {
                NMLOG_WARNING("device state is not activated state: (%d)", deviceState);
                return Core::ERROR_GENERAL;
            }

            if(ipversion.empty())
                NMLOG_WARNING("ipversion is empty default value IPV4");

            const GPtrArray *connections = nm_client_get_active_connections(client);
            if(connections == NULL)
            {
                NMLOG_WARNING("nm_client_get_active_connections error");
                return Core::ERROR_GENERAL;
            }
            for (guint i = 0; i < connections->len; i++){
                NMActiveConnection *connection = NM_ACTIVE_CONNECTION(connections->pdata[i]);
                settings = nm_connection_get_setting_connection(NM_CONNECTION(nm_active_connection_get_connection(connection)));

                /* Check if the interface name matches */
                if (g_strcmp0(nm_setting_connection_get_interface_name(settings), iface.c_str()) == 0) {
                    conn = connection;
                    break;
                }
            }
            if (conn == NULL) {
                NMLOG_ERROR("no active connection found");
                return Core::ERROR_GENERAL;
            }

            if(ipversion.empty()||nmUtils::caseInsensitiveCompare(ipversion,"IPV4"))
            {
                ip4_config = nm_active_connection_get_ip4_config(conn);
                if (ip4_config != NULL) {
                    const GPtrArray *p; 
                    int              i;
                    p = nm_ip_config_get_addresses(ip4_config);
                    for (i = 0; i < p->len; i++) {
                        address = static_cast<NMIPAddress*>(p->pdata[i]);
                    }
                    gateway = nm_ip_config_get_gateway(ip4_config);
                }   
                dns_arr =   (char **)nm_ip_config_get_nameservers(ip4_config);

                dhcp4_config = nm_active_connection_get_dhcp4_config(conn);
                dhcpserver = nm_dhcp_config_get_one_option (dhcp4_config,
                               "dhcp_server_identifier");
                if(!ipversion.empty())
                    result.ipversion  = ipversion.c_str();
                else
                    result.ipversion  = "IPv4";
                if(dhcpserver)
                    result.dhcpserver = dhcpserver;
                result.ula            = "";
                result.ipaddress      = nm_ip_address_get_address(address);
                result.prefix         = nm_ip_address_get_prefix(address);
                result.gateway        = gateway;
                if((*(&dns_arr[0]))!=NULL)
                    result.primarydns     = *(&dns_arr[0]);
                if((*(&dns_arr[1]))!=NULL )
                    result.secondarydns   = *(&dns_arr[1]);

                rc = Core::ERROR_NONE;
            }
            else if(nmUtils::caseInsensitiveCompare(ipversion,"IPV6"))
            {
                NMIPAddress *a;
                ip6_config = nm_active_connection_get_ip6_config(conn);
                if (ip6_config != NULL) {
                    const GPtrArray *p; 
                    int              i;
                    p = nm_ip_config_get_addresses(ip6_config);
                    for (i = 0; i < p->len; i++) {
                        a = static_cast<NMIPAddress*>(p->pdata[i]);
                        result.ipaddress = nm_ip_address_get_address(a);
                        NMLOG_DEBUG("\tinet6 %s/%d\n", nm_ip_address_get_address(a), nm_ip_address_get_prefix(a));
                    }
                    gateway = nm_ip_config_get_gateway(ip6_config);

                    dns_arr =   (char **)nm_ip_config_get_nameservers(ip6_config);

                    dhcp6_config = nm_active_connection_get_dhcp6_config(conn);
                    dhcpserver = nm_dhcp_config_get_one_option (dhcp6_config,
                               "dhcp_server_identifier");
                    result.ipversion = ipversion.c_str();
                    if(dhcpserver)
                        result.dhcpserver   = dhcpserver;
                    result.ula              = "";
                    result.prefix         = 0;
                    result.gateway        = gateway;
                    if((*(&dns_arr[0]))!=NULL)
                    result.primarydns     = *(&dns_arr[0]);
                    if((*(&dns_arr[1]))!=NULL )
                    result.secondarydns   = *(&dns_arr[1]);
                }
                rc = Core::ERROR_NONE;
            }
            else
                NMLOG_WARNING("ipversion is not IPV4 orIPV6");
            return rc;
        }

        // Callback for nm_client_deactivate_connection_async
        static void on_deactivate_complete(GObject *source_object, GAsyncResult *res, gpointer user_data) {
            GError *error = NULL;

            // Check if the operation was successful
            if (!nm_client_deactivate_connection_finish(NM_CLIENT(source_object), res, &error)) {
                NMLOG_DEBUG("Deactivating connection failed: %s", error->message);
                g_error_free(error);
            } else {
                NMLOG_DEBUG("Deactivating connection successful");
            }
        }

        // Callback for nm_client_activate_connection_async
        static void on_activate_complete(GObject *source_object, GAsyncResult *res, gpointer user_data) {
            GError *error = NULL;

            // Check if the operation was successful
            if (!nm_client_activate_connection_finish(NM_CLIENT(source_object), res, &error)) {
                NMLOG_DEBUG("Activating connection failed: %s", error->message);
                g_error_free(error);
            } else {
                NMLOG_DEBUG("Activating connection successful");
            }

            g_main_loop_quit((GMainLoop*)user_data);
        }


        /* @brief Set IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::SetIPSettings(const string& interface /* @in */, const IPAddress& address /* @in */)
        {
            GMainLoop *g_loop;
            g_loop = g_main_loop_new(NULL, FALSE);
            uint32_t rc = Core::ERROR_NONE;
            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null");
                return Core::ERROR_GENERAL;
            }
            const GPtrArray *connections = nm_client_get_connections(client);
            NMSettingIP4Config *s_ip4;
            NMSettingIP6Config *s_ip6;
            NMConnection *conn = NULL;
            NMSettingConnection *settings;
            NMRemoteConnection *remote_connection;
            NMSetting *setting;
            const char *uuid;
            NMDevice *device      = NULL;
            const char *spec_object;

            for (guint i = 0; i < connections->len; i++) {
                NMConnection *connection = NM_CONNECTION(connections->pdata[i]);
                settings = nm_connection_get_setting_connection(connection);

                /* Check if the interface name matches */
                if (g_strcmp0(nm_setting_connection_get_interface_name(settings), interface.c_str()) == 0) {
                    conn = connection;
                    break;
                }
            }
            if (!address.autoconfig)
            {
                if (nmUtils::caseInsensitiveCompare("IPv4", address.ipversion))
                {
                    NMSettingIPConfig *ip4_config = nm_connection_get_setting_ip4_config(conn);
                    if (ip4_config == NULL) 
                    {
                        ip4_config = (NMSettingIPConfig *)nm_setting_ip4_config_new();
                    }
                    NMIPAddress *ipAddress;
                    setting = nm_connection_get_setting_by_name(conn, "ipv4");
                    ipAddress = nm_ip_address_new(AF_INET, address.ipaddress.c_str(), address.prefix, NULL);
                    nm_setting_ip_config_clear_addresses(ip4_config);
                    nm_setting_ip_config_add_address(NM_SETTING_IP_CONFIG(setting), ipAddress);
                    nm_setting_ip_config_clear_dns(ip4_config);
                    nm_setting_ip_config_add_dns(ip4_config, address.primarydns.c_str());
                    nm_setting_ip_config_add_dns(ip4_config, address.secondarydns.c_str());

                    g_object_set(G_OBJECT(ip4_config),
                            NM_SETTING_IP_CONFIG_GATEWAY, address.gateway.c_str(),
                            NM_SETTING_IP_CONFIG_NEVER_DEFAULT,
                            FALSE,
                            NULL);
                }
                else
                {
                    //FIXME : Add IPv6 support here
                    printf("Setting IPv6 is not supported at this point in time. This is just a place holder\n");
                    rc = Core::ERROR_NOT_SUPPORTED;
                }
            }
            else
            {
                if (nmUtils::caseInsensitiveCompare("IPv4", address.ipversion))
                {
                    s_ip4 = (NMSettingIP4Config *)nm_setting_ip4_config_new();
                    g_object_set(G_OBJECT(s_ip4), NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL);
                    nm_connection_add_setting(conn, NM_SETTING(s_ip4));
                }
                else
                {
                    s_ip6 = (NMSettingIP6Config *)nm_setting_ip6_config_new();
                    g_object_set(G_OBJECT(s_ip6), NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_AUTO, NULL);
                    nm_connection_add_setting(conn, NM_SETTING(s_ip6));
                }
            }
            device = nm_client_get_device_by_iface(client, interface.c_str());
            uuid = nm_connection_get_uuid(conn);
            remote_connection = nm_client_get_connection_by_uuid(client, uuid);
            NMActiveConnection *active_connection = NULL;

            const GPtrArray *acv_connections = nm_client_get_active_connections(client);
            for (guint i = 0; i < acv_connections->len; i++) {
                NMActiveConnection *connection1 = NM_ACTIVE_CONNECTION(acv_connections->pdata[i]);
                settings = nm_connection_get_setting_connection(NM_CONNECTION(nm_active_connection_get_connection(connection1)));

                /* Check if the interface name matches */
                if (g_strcmp0(nm_setting_connection_get_interface_name(settings), interface.c_str()) == 0) {
                    active_connection = connection1;
                    break;
                }
            }

            spec_object = nm_object_get_path(NM_OBJECT(active_connection));
            nm_remote_connection_commit_changes(remote_connection, false, NULL, NULL);
            nm_client_deactivate_connection_async(client, active_connection, NULL, on_deactivate_complete, NULL);
            nm_client_activate_connection_async(client, conn, device, spec_object, NULL, on_activate_complete, g_loop);
            g_main_loop_run(g_loop);
            return rc;
        }

        uint32_t NetworkManagerImplementation::StartWiFiScan(const string& frequency /* @in */, IStringIterator* const ssids/* @in */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            (void) ssids;

            nmEvent->setwifiScanOptions(true, true);
            if(wifi->wifiScanRequest(frequency))
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::StopWiFiScan(void)
        {
            uint32_t rc = Core::ERROR_NONE;
            // TODO explore wpa_supplicant stop
            nmEvent->setwifiScanOptions(false); // This will stop periodic posting of onAvailableSSID event
            NMLOG_INFO ("StopWiFiScan is success");
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetKnownSSIDs(IStringIterator*& ssids /* @out */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
           // TODO Fix the RPC waring  [Process.cpp:78](Dispatch)<PID:16538><TID:16538><1>: We still have living object [1]
            std::list<string> ssidList;
            if(wifi->getKnownSSIDs(ssidList))
            {
                if (!ssidList.empty())
                {
                    ssids = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(ssidList);
                    rc = Core::ERROR_NONE;
                }
                else
                {
                    NMLOG_INFO("known ssids not found !");
                    rc = Core::ERROR_GENERAL;
                }
            }

            return rc;
        }

        uint32_t NetworkManagerImplementation::AddToKnownSSIDs(const WiFiConnectTo& ssid /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(wifi->addToKnownSSIDs(ssid))
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::RemoveKnownSSID(const string& ssid /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(wifi->removeKnownSSID(ssid))
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::WiFiConnect(const WiFiConnectTo& ssid /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(ssid.ssid.empty() || ssid.ssid.size() > 32)
            {
                NMLOG_WARNING("ssid is invalied");
                return rc;
            }
            // Check the last scanning time and if it exceeds 10 sec do a rescanning
            if(!wifi->isWifiScannedRecently())
            {
                nmEvent->setwifiScanOptions(false, true); // not notify scan result but print logs
                if(!wifi->wifiScanRequest("", ssid.ssid))
                {
                    NMLOG_WARNING("scanning failed but try to connect");
                }
            }
            if(wifi->wifiConnect(ssid))
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::WiFiDisconnect(void)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(wifi->wifiDisconnect())
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetConnectedSSID(WiFiSSIDInfo&  ssidInfo /* @out */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            if(wifi->wifiConnectedSSIDInfo(ssidInfo))
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetWiFiSignalStrength(string& ssid /* @out */, string& signalStrength /* @out */, WiFiSignalQuality& quality /* @out */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;

            WiFiSSIDInfo ssidInfo;
            if(wifi->wifiConnectedSSIDInfo(ssidInfo))
            {
                ssid = ssidInfo.ssid;
                signalStrength = ssidInfo.strength;

	            float signalStrengthFloat = 0.0f;
                if(!signalStrength.empty())
                    signalStrengthFloat = std::stof(signalStrength.c_str());

                if (signalStrengthFloat == 0)
                    quality = WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED;
                else if (signalStrengthFloat >= signalStrengthThresholdExcellent && signalStrengthFloat < 0)
                    quality = WiFiSignalQuality::WIFI_SIGNAL_EXCELLENT;
                else if (signalStrengthFloat >= signalStrengthThresholdGood && signalStrengthFloat < signalStrengthThresholdExcellent)
                    quality = WiFiSignalQuality::WIFI_SIGNAL_GOOD;
                else if (signalStrengthFloat >= signalStrengthThresholdFair && signalStrengthFloat < signalStrengthThresholdGood)
                    quality = WiFiSignalQuality::WIFI_SIGNAL_FAIR;
                else
                    quality = WiFiSignalQuality::WIFI_SIGNAL_WEAK;

                NMLOG_INFO ("GetWiFiSignalStrength success");
            
                rc = Core::ERROR_NONE;
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetWifiState(WiFiState &state)
        {
            uint32_t rc = Core::ERROR_NONE;
            if(wifi->isWifiConnected())
                state = Exchange::INetworkManager::WIFI_STATE_CONNECTED;
            else
                state = Exchange::INetworkManager::WIFI_STATE_DISCONNECTED;
            return rc;
        }

        uint32_t NetworkManagerImplementation::StartWPS(const WiFiWPS& method /* @in */, const string& wps_pin /* @in */)
        {
            uint32_t rc = Core::ERROR_NONE;
            if(wifi->initiateWPS())
                NMLOG_INFO ("startWPS success");
            else
                rc = Core::ERROR_RPC_CALL_FAILED;
            return rc;
        }

        uint32_t NetworkManagerImplementation::StopWPS(void)
        {
            uint32_t rc = Core::ERROR_NONE;
            if(wifi->cancelWPS())
                NMLOG_INFO ("cancelWPS success");
            else
                rc = Core::ERROR_RPC_CALL_FAILED;
            return rc;
        }

    }
}

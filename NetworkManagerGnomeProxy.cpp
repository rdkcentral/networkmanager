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
                NMLOG_FATAL("Error initializing NMClient: %s", error->message);
                g_error_free(error);
                return;
            }

            nmUtils::getInterfacesName(); // get interface name form '/etc/device.proprties'
            NMDeviceState ethState = nmUtils::ifaceState(client, nmUtils::ethIface());
            if(ethState > NM_DEVICE_STATE_DISCONNECTED && ethState < NM_DEVICE_STATE_DEACTIVATING)
                m_defaultInterface = nmUtils::ethIface();
            else
                m_defaultInterface = nmUtils::wlanIface();

            NMLOG_INFO("default interface is %s",  m_defaultInterface.c_str());
            nmEvent = GnomeNetworkManagerEvents::getInstance();
            nmEvent->startNetworkMangerEventMonitor();
            wifi = wifiManager::getInstance();
            return;
        }

        uint32_t NetworkManagerImplementation::GetAvailableInterfaces (Exchange::INetworkManager::IInterfaceDetailsIterator*& interfacesItr/* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            std::vector<Exchange::INetworkManager::InterfaceDetails> interfaceList;
            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(client == nullptr) {
                NMLOG_FATAL("client connection null:");
                return Core::ERROR_GENERAL;
            }

            GPtrArray *devices = const_cast<GPtrArray *>(nm_client_get_devices(client));
            if (devices == NULL) {
                NMLOG_ERROR("Failed to get device list.");
                return Core::ERROR_GENERAL;
            }

            for (guint j = 0; j < devices->len; j++)
            {
                NMDevice *device = NM_DEVICE(devices->pdata[j]);
                if(device != NULL)
                {
                    const char* ifacePtr =  nm_device_get_iface(device);
                    if(ifacePtr == nullptr)
                        continue;
                    std::string ifaceStr = ifacePtr;
                    if(ifaceStr == wifiname || ifaceStr == ethname) // only wifi and ethenet taking
                    {
                        NMDeviceState deviceState = NM_DEVICE_STATE_UNKNOWN;
                        Exchange::INetworkManager::InterfaceDetails interface;
                        interface.mac = nm_device_get_hw_address(device);
                        deviceState = nm_device_get_state(device);
                        interface.enabled = (deviceState >= NM_DEVICE_STATE_UNAVAILABLE)? true : false;
                        if(deviceState > NM_DEVICE_STATE_DISCONNECTED && deviceState < NM_DEVICE_STATE_DEACTIVATING)
                            interface.connected = true;
                        else
                            interface.connected = false;

                        if(ifaceStr == wifiname) {
                            interface.type = INTERFACE_TYPE_WIFI;
                            interface.name = wifiname;
                            m_wlanConnected = interface.connected;
                        }
                        if(ifaceStr == ethname) {
                            interface.type = INTERFACE_TYPE_ETHERNET;
                            interface.name = ethname;
                            m_ethConnected = interface.connected;
                        }

                        interfaceList.push_back(interface);
                        rc = Core::ERROR_NONE;
                    }
                }
            }

            using Implementation = RPC::IteratorType<Exchange::INetworkManager::IInterfaceDetailsIterator>;
            interfacesItr = Core::Service<Implementation>::Create<Exchange::INetworkManager::IInterfaceDetailsIterator>(interfaceList);
            return rc;
        }

        /* @brief Get the active Interface used for external world communication */
        uint32_t NetworkManagerImplementation::GetPrimaryInterface (string& interface /* @out */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            GError *error = NULL;
            NMActiveConnection *activeConn = NULL;
            NMRemoteConnection *remoteConn = NULL;
            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_GENERAL;
            }

            activeConn = nm_client_get_primary_connection(client);
            if (activeConn == NULL) {
                NMLOG_WARNING("no active activeConn Interface found");
                NMDeviceState ethState = nmUtils::ifaceState(client, nmUtils::ethIface());
                /* if ethernet is connected but not completely activate then ethernet is taken as primary else wifi */
                if(ethState > NM_DEVICE_STATE_DISCONNECTED && ethState < NM_DEVICE_STATE_DEACTIVATING)
                    m_defaultInterface = interface = ethname;
                else
                    m_defaultInterface = interface = wifiname; // default is wifi
                return Core::ERROR_NONE;
            }

            remoteConn = nm_active_connection_get_connection(activeConn);
            if(remoteConn == NULL)
            {
                NMLOG_ERROR("remote connection error");
                return Core::ERROR_GENERAL;
            }

            const char *ifacePtr = nm_connection_get_interface_name(NM_CONNECTION(remoteConn));
            if(ifacePtr == NULL)
            {
                NMLOG_ERROR("nm_connection_get_interface_name is failed");
                return Core::ERROR_GENERAL;
            }

            interface = ifacePtr;
            m_defaultInterface = interface;
            if(interface != wifiname && interface != ethname)
            {
                NMLOG_ERROR("primary interface is not eth/wlan");
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
            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            if(interface.empty() || (wifiname != interface && ethname != interface))
            {
                NMLOG_FATAL("interface is not valied %s", interface.c_str()!=nullptr? interface.c_str():"empty");
                return Core::ERROR_GENERAL;
            }

            NMDevice *device = nm_client_get_device_by_iface(client, interface.c_str());
            if (device == NULL) {
                NMLOG_FATAL("libnm doesn't have device corresponding to %s", interface.c_str());
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
                if (g_strcmp0(nm_setting_connection_get_interface_name(settings), interface.c_str()) == 0) {
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

            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            if(interface.empty() || (interface != nmUtils::wlanIface() && interface != nmUtils::ethIface()))
            {
                NMLOG_ERROR("interface: %s; not valied", interface.c_str()!=nullptr? interface.c_str():"empty");
                return Core::ERROR_GENERAL;
            }

            if(!wifi->setInterfaceState(interface, enabled))
            {
                NMLOG_ERROR("interface state change failed");
                return Core::ERROR_GENERAL;
            }

            NMLOG_INFO("interface %s state: %s", interface.c_str(), enabled ? "enabled" : "disabled");
            return Core::ERROR_NONE;
        }

        uint32_t NetworkManagerImplementation::GetInterfaceState(const string& interface/* @in */, bool& isEnabled /* @out */)
        {
            isEnabled = false;
            bool isIfaceFound = false;
            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            if(interface.empty() || (wifiname != interface && ethname != interface))
            {
                NMLOG_ERROR("interface: %s; not valied", interface.c_str()!=nullptr? interface.c_str():"empty");
                return Core::ERROR_GENERAL;
            }

            GPtrArray *devices = const_cast<GPtrArray *>(nm_client_get_devices(client));
            if (devices == NULL) {
                NMLOG_ERROR("Failed to get device list.");
                return Core::ERROR_GENERAL;
            }

            for (guint j = 0; j < devices->len; j++)
            {
                NMDevice *device = NM_DEVICE(devices->pdata[j]);
                if(device != NULL)
                {
                    const char* iface = nm_device_get_iface(device);
                    if(iface != NULL)
                    {
                        std::string ifaceStr;
                        ifaceStr.assign(iface);
                        NMDeviceState deviceState = NM_DEVICE_STATE_UNKNOWN;
                        if(ifaceStr == interface)
                        {
                            isIfaceFound = true;
                            deviceState = nm_device_get_state(device);
                            isEnabled = (deviceState > NM_DEVICE_STATE_UNAVAILABLE) ? true : false;
                            NMLOG_INFO("%s : %s", ifaceStr.c_str(), isEnabled?"enabled":"disabled");
                            break;
                        }
                    }
                }
            }

            if(isIfaceFound)
                return Core::ERROR_NONE;
            else
                NMLOG_ERROR("%s : not found", interface.c_str());
            return Core::ERROR_GENERAL;
        }

        bool static isAutoConnectEnabled(NMActiveConnection* activeConn)
        {
            NMConnection *connection = NM_CONNECTION(nm_active_connection_get_connection(activeConn));
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

        /* @brief Get IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::GetIPSettings(string& interface /* @inout */, const string &ipversion /* @in */, IPAddress& result /* @out */) 
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            NMActiveConnection *conn = NULL;
            NMIPConfig *ip4_config = NULL;
            NMIPConfig *ip6_config = NULL;
            const gchar *gateway = NULL;
            char **dnsArr = NULL;
            NMDhcpConfig *dhcp4_config = NULL;
            NMDhcpConfig *dhcp6_config = NULL;
            const char* dhcpserver;
            NMSettingConnection *settings;
            NMDevice *device = NULL;

            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            if(interface.empty() || interface == "null")
            {
                if(Core::ERROR_NONE != GetPrimaryInterface(interface))
                {
                    NMLOG_WARNING("default interface get failed");
                    return Core::ERROR_NONE;
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
                return Core::ERROR_GENERAL;
            }

            device = nm_client_get_device_by_iface(client, interface.c_str());
            if (device == NULL) {
                NMLOG_FATAL("libnm doesn't have device corresponding to %s", interface.c_str());
                return Core::ERROR_GENERAL;
            }

            NMDeviceState deviceState = NM_DEVICE_STATE_UNKNOWN;
            deviceState = nm_device_get_state(device);
            if(deviceState < NM_DEVICE_STATE_DISCONNECTED)
            {
                NMLOG_WARNING("Device state is not a valid state: (%d)", deviceState);
                return Core::ERROR_GENERAL;
            }

            if(ipversion.empty())
                NMLOG_WARNING("ipversion is empty default value IPv4");

            const GPtrArray *connections = nm_client_get_active_connections(client);
            if(connections == NULL)
            {
                NMLOG_WARNING("no active connection; ip is not assigned to interface");
                return Core::ERROR_GENERAL;
            }

            for (guint i = 0; i < connections->len; i++)
            {
                NMActiveConnection *connection = NM_ACTIVE_CONNECTION(connections->pdata[i]);
                if (connection == nullptr)
                    continue;
                settings = nm_connection_get_setting_connection(NM_CONNECTION(nm_active_connection_get_connection(connection)));

                if (g_strcmp0(nm_setting_connection_get_interface_name(settings), interface.c_str()) == 0) {
                    conn = connection;
                    break;
                }
            }

            if (conn == NULL) {
                NMLOG_WARNING("no active connection on %s interface", interface.c_str());
                return Core::ERROR_GENERAL;
            }

            result.autoconfig = isAutoConnectEnabled(conn);

            if(ipversion.empty() || nmUtils::caseInsensitiveCompare(ipversion, "IPV4")) // default ipversion ipv4
            {
                ip4_config = nm_active_connection_get_ip4_config(conn);
                NMIPAddress *ipAddr = NULL;
                std::string ipStr;
                if (ip4_config == nullptr) {
                    NMLOG_WARNING("no IPv4 configurtion on %s", interface.c_str());
                    return Core::ERROR_GENERAL;
                }

                const GPtrArray *ipByte = nullptr;
                ipByte = nm_ip_config_get_addresses(ip4_config);
                if (ipByte == nullptr) {
                    NMLOG_WARNING("No IPv4 data found on %s", interface.c_str());
                    return Core::ERROR_GENERAL;
                }

                for (int i = 0; i < ipByte->len; i++)
                {
                    ipAddr = static_cast<NMIPAddress*>(ipByte->pdata[i]);
                    if(ipAddr)
                        ipStr = nm_ip_address_get_address(ipAddr);
                    if(!ipStr.empty())
                    {
                        result.ipaddress = nm_ip_address_get_address(ipAddr);
                        result.prefix = nm_ip_address_get_prefix(ipAddr);
                        NMLOG_INFO("IPv4 addr: %s/%d", result.ipaddress.c_str(), result.prefix);
                        result.ipversion = "IPv4"; // if null add as default
                    }
                }

                gateway = nm_ip_config_get_gateway(ip4_config);

                dnsArr = (char **)nm_ip_config_get_nameservers(ip4_config);
                dhcp4_config = nm_active_connection_get_dhcp4_config(conn);
                if(dhcp4_config)
                    dhcpserver = nm_dhcp_config_get_one_option (dhcp4_config, "dhcp_server_identifier");
                if(dhcpserver)
                    result.dhcpserver = dhcpserver;
                result.ula = "";
                if(gateway)
                    result.gateway = gateway;
                if((*(&dnsArr[0]))!=NULL)
                    result.primarydns     = *(&dnsArr[0]);
                if((*(&dnsArr[1]))!=NULL )
                    result.secondarydns   = *(&dnsArr[1]);

                rc = Core::ERROR_NONE;
            }
            else if(nmUtils::caseInsensitiveCompare(ipversion, "IPV6"))
            {
                result.ipversion = ipversion.c_str();
                NMIPAddress *ipAddr = nullptr;
                ip6_config = nm_active_connection_get_ip6_config(conn);
                if(ip6_config == nullptr)
                {
                    NMLOG_WARNING("no IPv6 configurtion on %s", interface.c_str());
                    return Core::ERROR_GENERAL;
                }

                std::string ipStr;
                const GPtrArray *ipArray = nullptr;
                ipArray = nm_ip_config_get_addresses(ip6_config);
                for (int i = 0; i < ipArray->len; i++)
                {
                    ipAddr = static_cast<NMIPAddress*>(ipArray->pdata[i]);
                    if(ipAddr)
                        ipStr = nm_ip_address_get_address(ipAddr);
                    if(!ipStr.empty())
                    {
                        if (ipStr.compare(0, 5, "fe80:") == 0 || ipStr.compare(0, 6, "fe80::") == 0) {
                            result.ula = ipStr;
                            NMLOG_INFO("link-local ip: %s", result.ula.c_str());
                        }
                        else {
                            result.prefix = nm_ip_address_get_prefix(ipAddr);
                            if(result.ipaddress.empty()) // SLAAC mutiple ip not added
                                result.ipaddress = ipStr;
                            NMLOG_INFO("global ip %s/%d", ipStr.c_str(), result.prefix);
                        }
                    }
                }

                gateway = nm_ip_config_get_gateway(ip6_config);
                if(gateway)
                    result.gateway= gateway;
                dnsArr = (char **)nm_ip_config_get_nameservers(ip6_config);
                if((*(&dnsArr[0]))!= NULL)
                    result.primarydns = *(&dnsArr[0]);
                if((*(&dnsArr[1]))!=NULL )
                    result.secondarydns = *(&dnsArr[1]);

                dhcp6_config = nm_active_connection_get_dhcp6_config(conn);
                if(dhcp6_config)
                {
                    dhcpserver = nm_dhcp_config_get_one_option (dhcp6_config, "dhcp_server_identifier");
                    if(dhcpserver) {
                        result.dhcpserver = dhcpserver;
                    }
                }
                result.ipversion = "IPv6";
                rc = Core::ERROR_NONE;
            }
            else
                NMLOG_WARNING("ipversion error IPv4/IPv6");
            return rc;
        }

        /* @brief Set IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::SetIPSettings(const string& interface /* @in */, const IPAddress& address /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(wifi->setIpSettings(interface, address))
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::StartWiFiScan(const string& frequency /* @in */, IStringIterator* const ssids/* @in */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            (void) ssids;

            nmEvent->setwifiScanOptions(true);
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
            NMLOG_WARNING("ssid security %d", ssid.security);
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

           //  Check the last scanning time and if it exceeds 5 sec do a rescanning
            if(!wifi->isWifiScannedRecently())
            {
                nmEvent->setwifiScanOptions(false);
                if(!wifi->wifiScanRequest())
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

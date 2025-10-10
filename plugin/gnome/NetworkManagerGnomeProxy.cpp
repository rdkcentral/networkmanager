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
#include "NetworkManagerImplementation.h"
#include "NetworkManagerGnomeWIFI.h"
#include "NetworkManagerGnomeEvents.h"
#include "NetworkManagerGnomeUtils.h"
#include "NetworkManagerGnomeMfrMgr.h"
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
        NetworkManagerImplementation* _instance = nullptr;

        void NetworkManagerInternalEventHandler(const char *owner, int eventId, void *data, size_t len)
        {
            return;
        }

        static NMDeviceState ifaceState(NMClient *client, const char* interface)
        {
            NMDeviceState deviceState = NM_DEVICE_STATE_UNKNOWN;
            NMDevice *device = NULL;
            if(client == NULL)
                return deviceState;

            if(string("eth0_missing") == interface || string("wlan0_missing") == interface)
            {
                NMLOG_DEBUG("interface %s is not valid", interface);
                return deviceState;
            }

            device = nm_client_get_device_by_iface(client, interface);
            if (device == NULL) {
                NMLOG_FATAL("libnm doesn't have device corresponding to %s", interface);
                return deviceState;
            }

            deviceState = nm_device_get_state(device);
            return deviceState;
        }

        static bool setHostname(NMConnection *connection, const std::string& hostname)
        {
            GError *error = NULL;
            NMSettingIPConfig *sIPv4 = NULL;
            NMSettingIPConfig *sIPv6 = NULL;
            bool needChange = false;

            if(connection == NULL) {
                NMLOG_ERROR("Connection is NULL");
                return false;
            }

            sIPv4 = nm_connection_get_setting_ip4_config(connection);
            if (sIPv4) {
                const char* existingHostname = nm_setting_ip_config_get_dhcp_hostname(sIPv4);
                gboolean sendHostname = nm_setting_ip_config_get_dhcp_send_hostname(sIPv4);

                if (!sendHostname || !existingHostname || hostname != existingHostname) {
                    needChange = true;
                }
            }
            else
                needChange = true;

            sIPv6 = nm_connection_get_setting_ip6_config(connection);
            if (sIPv6) {
                const char* existingHostname = nm_setting_ip_config_get_dhcp_hostname(sIPv6);
                gboolean sendHostname = nm_setting_ip_config_get_dhcp_send_hostname(sIPv6);
                
                if (!sendHostname || !existingHostname || hostname != existingHostname) {
                    needChange = true;
                }
            }
            else
                needChange = true;

            if (!needChange) {
                NMLOG_DEBUG("Hostname already set to '%s', no changes needed", hostname.c_str());
                return true;
            }

            NMLOG_DEBUG("Setting hostname to '%s' for connection '%s'", hostname.c_str(), nm_connection_get_id(connection));

            if (!sIPv4) {
                sIPv4 = NM_SETTING_IP_CONFIG(nm_setting_ip4_config_new());
                nm_connection_add_setting(connection, NM_SETTING(sIPv4));
            }
            g_object_set(sIPv4,
                        NM_SETTING_IP_CONFIG_DHCP_HOSTNAME, hostname.c_str(),
                        NM_SETTING_IP_CONFIG_DHCP_SEND_HOSTNAME, TRUE,
                        NULL);

            if (!sIPv6) {
                sIPv6 = NM_SETTING_IP_CONFIG(nm_setting_ip6_config_new());
                nm_connection_add_setting(connection, NM_SETTING(sIPv6));
            }
            g_object_set(sIPv6,
                        NM_SETTING_IP_CONFIG_DHCP_HOSTNAME, hostname.c_str(),
                        NM_SETTING_IP_CONFIG_DHCP_SEND_HOSTNAME, TRUE,
                        NULL);

            nm_remote_connection_commit_changes(NM_REMOTE_CONNECTION(connection), TRUE, NULL, &error);
            if(error) {
                NMLOG_ERROR("Failed to commit changes: %s", error->message);
                g_error_free(error);
                return false;
            }

            return true;
        }

        static bool modifyDefaultConnConfig(NMClient *client)
        {
            GError *error = NULL;
            const GPtrArray *connections = NULL;
            NMConnection *connection = NULL;
            std::string hostname{};

            if (client == nullptr) {
                NMLOG_ERROR("NMClient is NULL");
                return false;
            }

            // read persistent hostname if exist
            if(!nmUtils::readPersistentHostname(hostname))
            {
                hostname = nmUtils::deviceHostname(); // default hostname as device name
            }

            connections = nm_client_get_connections(client);
            if (connections == NULL || connections->len == 0) {
                NMLOG_ERROR("Could not get nm connections");
                return false;
            }

            for (uint32_t i = 0; i < connections->len; i++)
            {
                connection = NM_CONNECTION(connections->pdata[i]);
                if(connection == NULL)
                {
                    NMLOG_ERROR("Connection at index %d is NULL", i);
                    continue;
                }

                const char *iface = nm_connection_get_interface_name(connection);
                if(!iface)
                {
                    NMLOG_WARNING("Failed to get connection interface name !");
                    continue;
                }

                std::string interface = iface;
                if(interface != nmUtils::ethIface() && interface != nmUtils::wlanIface()) {
                    NMLOG_DEBUG("Skipping non-ethernet/wifi connection type: %s", interface.c_str());
                    continue;
                }

                if(!setHostname(connection, hostname)) {
                    NMLOG_WARNING("Failed to set hostname for connection at index %d", i);
                }
            }

            return true;
        }

        /* @brief Set the dhcp hostname */
        uint32_t NetworkManagerImplementation::SetHostname(const string& hostname /* @in */)
        {
            const GPtrArray *connections = NULL;
            NMConnection *connection = NULL;

            if (client == nullptr) {
                NMLOG_ERROR("NMClient is NULL");
                return Core::ERROR_GENERAL;
            }

            if(hostname.length() < 1 || hostname.length() > 32)
            {
                NMLOG_ERROR("Invalid hostname length: %zu", hostname.length());
                return Core::ERROR_BAD_REQUEST;
            }

            connections = nm_client_get_connections(client);
            if (connections == NULL || connections->len == 0)
            {
                NMLOG_ERROR("Could not get nm connections");
                return Core::ERROR_GENERAL;
            }

            NMLOG_INFO("Setting DHCP hostname to %s", hostname.c_str());

            for (uint32_t i = 0; i < connections->len; i++)
            {
                connection = NM_CONNECTION(connections->pdata[i]);
                if(connection != NULL)
                {
                    const char *iface = nm_connection_get_interface_name(connection);
                    if(!iface)
                    {
                        NMLOG_WARNING("Failed to get connection interface name !");
                        continue;
                    }

                    std::string interface = iface;
                    if(interface != nmUtils::ethIface() && interface != nmUtils::wlanIface()) {
                        NMLOG_DEBUG("Skipping non-ethernet/wifi connection type: %s", interface.c_str());
                        continue;
                    }

                    if(!setHostname(connection, hostname))
                    {
                        NMLOG_ERROR("Failed to set hostname for connection at index %d", i);
                        return Core::ERROR_GENERAL;
                    }
                }
                else
                    NMLOG_ERROR("Connection at index %d is NULL", i);
            }

            // Write the hostname to persistent storage
            nmUtils::writePersistentHostname(hostname);

            return Core::ERROR_NONE;
        }

        void NetworkManagerImplementation::platform_logging(const NetworkManagerLogger::LogLevel& level)
        {
            /* set networkmanager daemon log level based on current plugin log level */
            if(NetworkManagerLogger::DEBUG_LEVEL != level)
                return;
            nmUtils::setNetworkManagerlogLevelToTrace();
        }

        void NetworkManagerImplementation::platform_init()
        {
            ::_instance = this;
            GError *error = NULL;

            // initialize the NMClient object
            client = nm_client_new(NULL, &error);
            if (client == NULL) {
                if (error) {
                    NMLOG_FATAL("Error initializing NMClient: %s", error->message);
                    g_error_free(error);
                }
                return;
            }

            nmUtils::getDeviceProperties(); // get interface name form '/etc/device.proprties'
            modifyDefaultConnConfig(client);
            NMDeviceState ethState = ifaceState(client, nmUtils::ethIface());
            if(ethState > NM_DEVICE_STATE_DISCONNECTED && ethState < NM_DEVICE_STATE_DEACTIVATING)
                m_defaultInterface = nmUtils::ethIface();
            else
                m_defaultInterface = nmUtils::wlanIface();

            NMLOG_INFO("default interface is %s",  m_defaultInterface.c_str());
            nmEvent = GnomeNetworkManagerEvents::getInstance();
            nmEvent->startNetworkMangerEventMonitor();
            wifi = wifiManager::getInstance();
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
                        Exchange::INetworkManager::InterfaceDetails interface{};
                        const char* macAddr = nm_device_get_hw_address(device);
                        if(macAddr != nullptr) {
                            interface.mac = macAddr;
                        }
                        deviceState = nm_device_get_state(device);
                        interface.enabled = (deviceState >= NM_DEVICE_STATE_UNAVAILABLE)? true : false;
                        if(deviceState > NM_DEVICE_STATE_DISCONNECTED && deviceState < NM_DEVICE_STATE_DEACTIVATING)
                            interface.connected = true;
                        else
                            interface.connected = false;

                        if(ifaceStr == wifiname) {
                            interface.type = INTERFACE_TYPE_WIFI;
                            interface.name = wifiname;
                            m_wlanConnected.store(interface.connected);
                        }
                        if(ifaceStr == ethname) {
                            interface.type = INTERFACE_TYPE_ETHERNET;
                            interface.name = ethname;
                            m_ethConnected.store(interface.connected);
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
                NMDeviceState ethState = ifaceState(client, nmUtils::ethIface());
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
                NMLOG_WARNING("primary connection but remote connection error");
                NMDeviceState ethState = ifaceState(client, nmUtils::ethIface());
                /* if ethernet is connected but not completely activate then ethernet is taken as primary else wifi */
                if(ethState > NM_DEVICE_STATE_DISCONNECTED && ethState < NM_DEVICE_STATE_DEACTIVATING)
                    m_defaultInterface = interface = ethname;
                else
                    m_defaultInterface = interface = wifiname; // default is wifi
                return Core::ERROR_NONE;
            }

            const char *ifacePtr = nm_connection_get_interface_name(NM_CONNECTION(remoteConn));
            if(ifacePtr == NULL)
            {
                NMLOG_ERROR("nm_connection_get_interface_name is failed");
                /* Temporary mitigation for nm_connection_get_interface_name failure */
                if(m_ethConnected.load())
                    interface = ethname;
                else // default always wifi
                    interface = wifiname;
                rc = Core::ERROR_NONE;
            }
            else
            {
                interface = ifacePtr;
                if(interface != wifiname && interface != ethname)
                {
                    NMLOG_ERROR("primary interface is not Ethernet or WiFi");
                    interface.clear();
                }
                else
                    rc = Core::ERROR_NONE;
            } 
            m_defaultInterface = interface;
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
            if(enabled)
            {
                sleep(1); // wait for 1 sec to change the device state
                if(interface == nmUtils::wlanIface() && _instance != NULL)
                {
                    NMLOG_INFO("Activating connection '%s' ...", _instance->m_lastConnectedSSID.c_str());
                    wifi->activateKnownConnection(nmUtils::wlanIface(), _instance->m_lastConnectedSSID);
                }
                else if(interface == nmUtils::ethIface())
                {
                    NMLOG_INFO("Activating connection 'Wired connection 1' ...");
                    // default wired connection name is 'Wired connection 1'
                    wifi->activateKnownConnection(nmUtils::ethIface(), "Wired connection 1");
                }
            }

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
            NMActiveConnection *conn = NULL;
            NMIPConfig *ip4_config = NULL;
            NMIPConfig *ip6_config = NULL;
            const gchar *gateway = NULL;
            char **dnsArr = NULL;
            NMDhcpConfig *dhcp4_config = NULL;
            NMDhcpConfig *dhcp6_config = NULL;
            const char* dhcpserver;
            NMSettingConnection *settings = NULL;
            NMDevice *device = NULL;

            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(client == nullptr)
            {
                NMLOG_WARNING("client connection null:");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            if(interface.empty())
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
            if (device == NULL)
            {
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

            // if(ipversion.empty())
            //     NMLOG_DEBUG("ipversion is empty default value IPv4");

            const GPtrArray *connections = nm_client_get_active_connections(client);
            if(connections == NULL)
            {
                NMLOG_WARNING("no active connection; ip is not assigned to interface");
                return Core::ERROR_GENERAL;
            }

            for (guint i = 0; i < connections->len; i++)
            {
                if(connections->pdata[i] == NULL)
                    continue;

                NMActiveConnection *connection = NM_ACTIVE_CONNECTION(connections->pdata[i]);
                if (connection == nullptr)
                    continue;

                NMRemoteConnection* retConn = nm_active_connection_get_connection(connection);
                if(retConn == NULL)
                {
                    NMLOG_INFO("remote connection is null");
                    continue;
                }

                settings = nm_connection_get_setting_connection(NM_CONNECTION(retConn));
                if(settings == NULL)
                    continue;
                if (g_strcmp0(nm_setting_connection_get_interface_name(settings), interface.c_str()) == 0)
                {
                    conn = connection;
                    break;
                }
            }

            if (conn == NULL)
            {
                NMLOG_WARNING("no active connection on %s interface", interface.c_str());
                return Core::ERROR_GENERAL;
            }

            result.autoconfig = isAutoConnectEnabled(conn);

            if(ipversion.empty() || nmUtils::caseInsensitiveCompare(ipversion, "IPV4"))
            {
                const GPtrArray *ipByte = nullptr;
                result.ipversion = "IPv4";
                ip4_config = nm_active_connection_get_ip4_config(conn);
                NMIPAddress *ipAddr = NULL;
                std::string ipStr;
                if (ip4_config)
                    ipByte = nm_ip_config_get_addresses(ip4_config);
                else
                    NMLOG_WARNING("no IPv4 configurtion on %s", interface.c_str());
                if(ipByte)
                {
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
                        }
                    }
                    gateway = nm_ip_config_get_gateway(ip4_config);
                    if(gateway)
                        result.gateway = gateway;
                    dnsArr = (char **)nm_ip_config_get_nameservers(ip4_config);
                    if(dnsArr)
                    {
                        if(dnsArr[0])
                            result.primarydns = std::string(dnsArr[0]);
                        if(dnsArr[1])
                            result.secondarydns = std::string(dnsArr[1]);
                    }
                    dhcp4_config = nm_active_connection_get_dhcp4_config(conn);
                    if(dhcp4_config)
                    {
                        dhcpserver = nm_dhcp_config_get_one_option (dhcp4_config, "dhcp_server_identifier");
                        if(dhcpserver)
                            result.dhcpserver = dhcpserver;
                    }
                    result.ula = "";
                }
            }
            if((result.ipaddress.empty() && !(nmUtils::caseInsensitiveCompare(ipversion, "IPV4"))) || nmUtils::caseInsensitiveCompare(ipversion, "IPV6"))
            {
                std::string ipStr;
                const GPtrArray *ipArray = nullptr;
                result.ipversion = "IPv6";
                NMIPAddress *ipAddr = nullptr;
                ip6_config = nm_active_connection_get_ip6_config(conn);
                if(ip6_config)
                    ipArray = nm_ip_config_get_addresses(ip6_config);
                else
                    NMLOG_WARNING("no IPv6 configurtion on %s", interface.c_str());
                if(ipArray)
                {
                    for (int i = 0; i < ipArray->len; i++)
                    {
                        ipAddr = static_cast<NMIPAddress*>(ipArray->pdata[i]);
                        if(ipAddr)
                            ipStr = nm_ip_address_get_address(ipAddr);
                        if(!ipStr.empty())
                        {
                            if (ipStr.compare(0, 5, "fe80:") == 0 || ipStr.compare(0, 6, "fe80::") == 0)
                            {
                                result.ula = ipStr;
                                NMLOG_INFO("link-local ip: %s", result.ula.c_str());
                            }
                            else
                            {
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
                    if(dnsArr)
                    {
                        if(dnsArr[0])
                            result.primarydns = std::string(dnsArr[0]);
                        if(dnsArr[1])
                            result.secondarydns = std::string(dnsArr[1]);
                    }
                    dhcp6_config = nm_active_connection_get_dhcp6_config(conn);
                    if(dhcp6_config)
                    {
                        dhcpserver = nm_dhcp_config_get_one_option (dhcp6_config, "dhcp_server_identifier");
                        if(dhcpserver)
                            result.dhcpserver = dhcpserver;
                    }
                }
            }
            if(result.ipaddress.empty())
            {
                result.autoconfig = true;
                if(ipversion.empty())
                    result.ipversion = "IPv4";
            }
            return Core::ERROR_NONE;
        }

        /* @brief Set IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::SetIPSettings(const string& interface /* @in */, const IPAddress& address /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if (("IPv4" != address.ipversion) && ("IPv6" != address.ipversion))
            {
                return Core::ERROR_BAD_REQUEST;
            }
            if(wifi->setIpSettings(interface, address))
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::StartWiFiScan(const string& frequency /* @in */, IStringIterator* const ssids/* @in */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;

            //Cleared the Existing Store filterred SSID list
            m_filterSsidslist.clear();
            m_filterfrequency.clear();

            if(ssids)
            {
                string tmpssidlist{};
                while (ssids->Next(tmpssidlist) == true)
                {
                    m_filterSsidslist.push_back(tmpssidlist.c_str());
                    NMLOG_DEBUG("%s added to SSID filtering", tmpssidlist.c_str());
                }
            }

            if (!frequency.empty())
            {
                m_filterfrequency = frequency;
                NMLOG_DEBUG("Scan SSIDs of frequency %s", m_filterfrequency.c_str());
            }

            nmEvent->setwifiScanOptions(true);
            if(wifi->wifiScanRequest(m_filterSsidslist.size() == 1 ? m_filterSsidslist[0] : ""))
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

           //  Check the last scanning time and if it exceeds 5 sec do a rescanning
            if(!wifi->isWifiScannedRecently())
            {
                nmEvent->setwifiScanOptions(false);
                if(!wifi->wifiScanRequest())
                    NMLOG_WARNING("scanning failed but try to connect");
            }

            if(ssid.ssid.empty() && _instance != NULL)
            {
                NMLOG_WARNING("ssid is empty activating last connectd ssid !");
                if(wifi->activateKnownConnection(nmUtils::wlanIface(), _instance->m_lastConnectedSSID))
                    rc = Core::ERROR_NONE;
            }
            else if(ssid.ssid.size() <= 32)
            {
                if(wifi->wifiConnect(ssid))
                    rc = Core::ERROR_NONE;
            }
            else
                NMLOG_WARNING("SSID is invalid");

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

#if 0
        uint32_t NetworkManagerImplementation::GetWiFiSignalQuality(string& ssid /* @out */, string& strength /* @out */, WiFiSignalQuality& quality /* @out */)
        {
            float rssi = 0.0f;
            float noise = 0.0f;
            float floatSignalStrength = 0.0f;
            WiFiSSIDInfo ssidInfo;
            if(wifi->wifiConnectedSSIDInfo(ssidInfo))
            {
                ssid              = ssidInfo.ssid;
                if (!ssidInfo.strength.empty())
                    rssi          = std::stof(ssidInfo.strength.c_str());
                if (!ssidInfo.noise.empty())
                    noise         = std::stof(ssidInfo.noise.c_str());
                floatSignalStrength = (rssi - noise);
                if (floatSignalStrength < 0)
                    floatSignalStrength = 0.0;

                strengthOut = static_cast<unsigned int>(floatSignalStrength);
                NMLOG_INFO ("WiFiSignalQuality in dB = %u",strengthOut);

                if (strengthOut == 0)
                {
                    quality = WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED;
                    signalStrength = "0";
                }
                else if (strengthOut > 0 && strengthOut < NM_WIFI_SNR_THRESHOLD_FAIR)
                {
                    quality = WiFiSignalQuality::WIFI_SIGNAL_WEAK;
                }
                else if (strengthOut > NM_WIFI_SNR_THRESHOLD_FAIR && strengthOut < NM_WIFI_SNR_THRESHOLD_GOOD)
                {
                    quality = WiFiSignalQuality::WIFI_SIGNAL_FAIR;
                }
                else if (strengthOut > NM_WIFI_SNR_THRESHOLD_GOOD && strengthOut < NM_WIFI_SNR_THRESHOLD_EXCELLENT)
                {
                    quality = WiFiSignalQuality::WIFI_SIGNAL_GOOD;
                }
                else
                {
                    quality = WiFiSignalQuality::WIFI_SIGNAL_EXCELLENT;
                }

                signalStrength = std::to_string(strengthOut);

                NMLOG_INFO ("GetWiFiSignalQuality success");
            
                rc = Core::ERROR_NONE;
            }
            return rc;
        }
#endif

        uint32_t NetworkManagerImplementation::GetWifiState(WiFiState &state)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(wifi->getWifiState(state))
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::StartWPS(const WiFiWPS& method /* @in */, const string& wps_pin /* @in */)
        {
            uint32_t rc = Core::ERROR_NONE;
            if(method == WIFI_WPS_SERIALIZED_PIN || method == WIFI_WPS_PIN)
            {
                NMLOG_ERROR("WPS PIN method is not supported as of now");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            if(!wifi->wifiScanRequest())
            {
                NMLOG_WARNING("scanning request failed; trying to connect wps");
            }

            if(wifi->startWPS())
                NMLOG_INFO ("start WPS success");
            else {
                rc = Core::ERROR_GENERAL;
                NMLOG_ERROR("start WPS failed");
            }

            return rc;
        }

        uint32_t NetworkManagerImplementation::StopWPS(void)
        {
            uint32_t rc = Core::ERROR_NONE;
            if(wifi->stopWPS())
                NMLOG_INFO ("cancelWPS success");
            else
                rc = Core::ERROR_RPC_CALL_FAILED;
            return rc;
        }

    }
}

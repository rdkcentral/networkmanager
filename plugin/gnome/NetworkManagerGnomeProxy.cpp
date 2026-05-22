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
#include <fstream>
#include <sstream>
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
                hostname = nmUtils::deviceHostname(); // default hostname as default hostname
            }

            // Validate hostname is non-empty regardless of source (persistent or default)
            if(hostname.empty())
            {
                NMLOG_WARNING("Hostname is empty. No modification will be made to NM connections.");
                return false;
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

            if (m_nmClient == nullptr) {
                NMLOG_ERROR("NMClient is NULL");
                return Core::ERROR_GENERAL;
            }

            if(hostname.length() < 1 || hostname.length() > 32)
            {
                NMLOG_ERROR("Invalid hostname length: %zu", hostname.length());
                return Core::ERROR_BAD_REQUEST;
            }

            connections = nm_client_get_connections(m_nmClient);
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

        void NetworkManagerImplementation::platform_deinit()
        {
            if(m_nmClient) { g_object_unref(m_nmClient); m_nmClient = nullptr; }
            if(m_nmContext) { g_main_context_unref(m_nmContext); m_nmContext = nullptr; }
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
            // Create an isolated GMainContext so this m_nmClient's D-Bus socket is NOT a
            // source on the global default context.  The event thread runs the default
            // context via g_main_loop_run(); without isolation it would own and mutate
            // this m_nmClient's GObjects concurrently with the RPC thread.
            m_nmContext = g_main_context_new();
            g_main_context_push_thread_default(m_nmContext);
            m_nmClient = nm_client_new(NULL, &error);
            g_main_context_pop_thread_default(m_nmContext);
            if (m_nmClient == NULL) {
                if (error) {
                    NMLOG_FATAL("Error initializing NMClient: %s", error->message);
                    g_error_free(error);
                }
                if (m_nmContext) {
                    g_main_context_unref(m_nmContext);
                    m_nmContext = nullptr;
                }
                return;
            }

            nmUtils::getDeviceProperties(); // get interface name form '/etc/device.proprties'
            modifyDefaultConnConfig(m_nmClient);
            NMDeviceState ethState = ifaceState(m_nmClient, nmUtils::ethIface());
            if(ethState > NM_DEVICE_STATE_DISCONNECTED && ethState < NM_DEVICE_STATE_DEACTIVATING)
                setDefaultInterface(nmUtils::ethIface());
            else
                setDefaultInterface(nmUtils::wlanIface());

            NMLOG_INFO("default interface is %s",  getDefaultInterface().c_str());
            // getInitialConnectionState function not called here, as event monitor will report the initial state
            nmEvent = GnomeNetworkManagerEvents::getInstance();
            nmEvent->startNetworkMangerEventMonitor();
            wifi = wifiManager::getInstance();
        }

        uint32_t NetworkManagerImplementation::GetAvailableInterfaces (Exchange::INetworkManager::IInterfaceDetailsIterator*& interfacesItr/* @out */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            std::vector<Exchange::INetworkManager::InterfaceDetails> interfaceList;
            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(m_nmClient == nullptr) {
                NMLOG_FATAL("NMClient is null");
                return Core::ERROR_GENERAL;
            }

            if (m_nmContext) {
                for (int i = 0; i < 100 && g_main_context_iteration(m_nmContext, FALSE); ++i){
                    // Intentional empty body: just flushing the event queue
                }
            }
            GPtrArray *devices = const_cast<GPtrArray *>(nm_client_get_devices(m_nmClient));
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
                            m_wlanEnabled.store(interface.enabled);
                        }
                        else if(ifaceStr == ethname) {
                            interface.type = INTERFACE_TYPE_ETHERNET;
                            interface.name = ethname;
                            m_ethConnected.store(interface.connected);
                            m_ethEnabled.store(interface.enabled);
                        }

                        interfaceList.push_back(interface);
                        rc = Core::ERROR_NONE;
                    }
                }
            }

            using Implementation = RPC::IteratorType<Exchange::INetworkManager::IInterfaceDetailsIterator>;
            interfacesItr = Core::Service<Implementation>::Create<Exchange::INetworkManager::IInterfaceDetailsIterator>(interfaceList);
            if(interfacesItr == nullptr) {
                return Core::ERROR_GENERAL;
            }
            return rc;
        }
#if 0
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

            if(!m_wlanEnabled.load() && !m_ethEnabled.load())
            {
                NMLOG_INFO("Both iface disabled state, returning no primary interface");
                interface.clear();
                m_defaultInterface = interface;
                return Core::ERROR_NONE;
            }

            activeConn = nm_client_get_primary_connection(client);
            if (activeConn != NULL)
            {
                remoteConn = nm_active_connection_get_connection(activeConn);
                if(remoteConn != NULL)
                {
                    const char *ifacePtr = nm_connection_get_interface_name(NM_CONNECTION(remoteConn));
                    if(ifacePtr != NULL)
                    {
                        interface = ifacePtr;
                        if(interface != nmUtils::wlanIface() && interface != nmUtils::ethIface())
                        {
                            NMLOG_WARNING("primary interface is not Ethernet or WiFi");
                            interface.clear();
                        }
                        else
                        {
                            NMLOG_DEBUG("primary interface is %s", interface.c_str());
                            rc = Core::ERROR_NONE;
                        }
                    }
                    else
                        NMLOG_WARNING("interface name is missing form connection");
                }
                else
                    NMLOG_WARNING("primary connection but remote connection error");
            }
            else
                NMLOG_WARNING("No primary connection");

            if(rc != Core::ERROR_NONE)
            {
                // if no active connection or libnm error, choose the default interface based on the connection state 
                NMDeviceState ethState = ifaceState(client, nmUtils::ethIface());
                /* if ethernet is connected but not completely activate then ethernet is taken as primary else wifi */
                if((ethState > NM_DEVICE_STATE_DISCONNECTED && ethState < NM_DEVICE_STATE_DEACTIVATING) || m_ethConnected.load())
                    interface = nmUtils::ethIface();
                else
                    interface = nmUtils::wlanIface(); // default is wifi
                rc = Core::ERROR_NONE;
                // TODO: if no proimary connection, should we return empty string? RDK V return empty string
            }

            m_defaultInterface = interface;
            return rc;
        }
#endif

        uint32_t NetworkManagerImplementation::SetInterfaceState(const string& interface/* @in */, const bool enabled /* @in */)
        {

            if(m_nmClient == nullptr)
            {
                NMLOG_WARNING("NMClient is null");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            if(interface.empty() || (interface != nmUtils::wlanIface() && interface != nmUtils::ethIface()))
            {
                NMLOG_ERROR("interface: %s; not valied", interface.c_str()!=nullptr? interface.c_str():"empty");
                return Core::ERROR_GENERAL;
            }

            // For ethernet enable: run BOOT_MIGRATION cleanup first, then setInterfaceState
            if(enabled && interface == nmUtils::ethIface())
            {
                // Check boot type and delete all ethernet NM connections if BOOT_MIGRATION
                {
                    const char* bootFile = "/tmp/bootType";
                    std::ifstream file(bootFile);

                    if(file.is_open())
                    {
                        std::string line, bootTypeValue;
                        while(std::getline(file, line))
                        {
                            const std::string key = "BOOT_TYPE=";
                            auto pos = line.find(key);
                            if(pos != std::string::npos)
                            {
                                bootTypeValue = line.substr(pos + key.size());
                                break;
                            }
                        }

                        if(bootTypeValue == "BOOT_MIGRATION")
                        {
                            NMLOG_INFO("BOOT_MIGRATION detected, deleting all wired NM connections");

                            // Bring down the ethernet interface before wiping its connections
                            // so NM doesn't immediately re-activate them during deletion.
                            NMDevice *ethDev = nm_client_get_device_by_iface(m_nmClient, interface.c_str());
                            if(ethDev)
                            {
                                GError *discError = nullptr;
                                if(!nm_device_disconnect(ethDev, nullptr, &discError))
                                {
                                    NMLOG_WARNING("Failed to disconnect %s before migration cleanup: %s",
                                            interface.c_str(),
                                            discError ? discError->message : "unknown error");
                                    if(discError) g_error_free(discError);
                                }
                            }

                            const GPtrArray *connections = nm_client_get_connections(m_nmClient);
                            if(connections && connections->len > 0)
                            {
                                /* Snapshot the list before iterating: nm_client_get_connections()
                                 * returns an internal array that can be mutated as connections
                                 * are removed, so we must not iterate it while deleting. */
                                GPtrArray *snapshot = g_ptr_array_new_full(connections->len, g_object_unref);
                                for(guint i = 0; i < connections->len; ++i)
                                {
                                    NMRemoteConnection *conn = NM_REMOTE_CONNECTION(connections->pdata[i]);
                                    if(!conn) continue;
                                    NMSettingConnection *sCon = nm_connection_get_setting_connection(NM_CONNECTION(conn));
                                    if(!sCon) continue;
                                    const char *connType = nm_setting_connection_get_connection_type(sCon);
                                    if(g_strcmp0(connType, NM_SETTING_WIRED_SETTING_NAME) != 0)
                                    {
                                        NMLOG_DEBUG("Skipping non-wired connection type: %s", connType ? connType : "null");
                                        continue;
                                    }
                                    g_ptr_array_add(snapshot, g_object_ref(conn));
                                }

                                for(guint i = 0; i < snapshot->len; ++i)
                                {
                                    NMRemoteConnection *conn = NM_REMOTE_CONNECTION(snapshot->pdata[i]);
                                    GError *error = nullptr;
                                    if(!nm_remote_connection_delete(conn, nullptr, &error))
                                    {
                                        const char *connId = nm_connection_get_id(NM_CONNECTION(conn));
                                        NMLOG_ERROR("Failed to delete connection %s: %s",
                                                    connId ? connId : "<unknown>",
                                                    error ? error->message : "unknown error");
                                        if(error) g_error_free(error);
                                    }
                                }
                                g_ptr_array_unref(snapshot);
                            }
                        }
                    }
                }

                NMLOG_INFO("Adding minimal ethernet connection profile ...");
                wifi->addMinimalEthernetConnection(nmUtils::ethIface());

                if(!wifi->setInterfaceState(interface, enabled))
                {
                    NMLOG_ERROR("interface state change failed");
                    return Core::ERROR_GENERAL;
                }

                NMLOG_INFO("interface %s state: %s", interface.c_str(), enabled ? "enabled" : "disabled");
                if(_instance != NULL)
                    _instance->m_ethEnabled.store(enabled);

                sleep(1); // wait for 1 sec to change the device state
                NMLOG_INFO("Activating connection 'Wired connection 1' ...");
                // default wired connection name is 'Wired connection 1'
                wifi->activateKnownConnection(nmUtils::ethIface(), "Wired connection 1");
            }
            else
            {
                if(!wifi->setInterfaceState(interface, enabled))
                {
                    NMLOG_ERROR("interface state change failed");
                    return Core::ERROR_GENERAL;
                }

                NMLOG_INFO("interface %s state: %s", interface.c_str(), enabled ? "enabled" : "disabled");
                // update the interface global cache state
                if(interface == nmUtils::wlanIface() && _instance != NULL)
                    _instance->m_wlanEnabled.store(enabled);
                else if(interface == nmUtils::ethIface() && _instance != NULL)
                    _instance->m_ethEnabled.store(enabled);

                if(enabled && interface == nmUtils::wlanIface() && _instance != NULL)
                {
                    sleep(1); // wait for 1 sec to change the device state
                    NMLOG_INFO("Activating connection '%s' ...", _instance->m_lastConnectedSSID.c_str());
                    wifi->activateKnownConnection(nmUtils::wlanIface(), _instance->m_lastConnectedSSID);
                }
            }

            return Core::ERROR_NONE;
        }

        uint32_t NetworkManagerImplementation::GetInterfaceState(const string& interface/* @in */, bool& isEnabled /* @out */)
        {
            isEnabled = false;
            bool isIfaceFound = false;
            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(interface.empty() || (wifiname != interface && ethname != interface))
            {
                NMLOG_ERROR("interface: %s; not valied", interface.c_str()!=nullptr? interface.c_str():"empty");
                return Core::ERROR_GENERAL;
            }

            if(m_nmClient == nullptr)
            {
                NMLOG_WARNING("NMClient is null");
                return Core::ERROR_RPC_CALL_FAILED;
            }

            if (m_nmContext) {
                for (int i = 0; i < 100 && g_main_context_iteration(m_nmContext, FALSE); ++i){
                    // Intentional empty body: just flushing the event queue
                }
            }

            GPtrArray *devices = const_cast<GPtrArray *>(nm_client_get_devices(m_nmClient));

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

        /* @brief Get IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::GetIPSettings(string& interface /* @inout */, const string &ipversion /* @in */, IPAddress& result /* @out */)
        {
            std::string wifiname = nmUtils::wlanIface(), ethname = nmUtils::ethIface();

            if(interface.empty())
            {
                if(Core::ERROR_NONE != GetPrimaryInterface(interface))
                {
                    NMLOG_WARNING("default interface get failed");
                    return Core::ERROR_NONE;
                }

                if(interface.empty())
                {
                    NMLOG_WARNING("default interface is empty, no active interface");
                    return Core::ERROR_GENERAL;
                }
            }
            else if(wifiname != interface && ethname != interface)
            {
                NMLOG_ERROR("interface: %s; not valid", interface.c_str());
                return Core::ERROR_GENERAL;
            }

            string ipversionStr = ipversion.empty() ? "IPv4" : ipversion;
            std::string family = nmUtils::caseInsensitiveCompare(ipversionStr, "IPv6") ? "IPv6" : "IPv4";

            result = IPAddress{};
            result.ipversion = family;

            // Serve from event-driven cache
            if (lookupIpCache(interface, family, result))
            {
                NMLOG_DEBUG("%s %s address from cache", interface.c_str(), family.c_str());
                result.ipversion = family;
            }
            else
            {
                NMLOG_DEBUG("no %s address on %s", family.c_str(), interface.c_str());
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

        uint32_t NetworkManagerImplementation::StartWiFiScan(IStringIterator* const frequencies /* @in */, IStringIterator* const ssids/* @in */)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;

            //Cleared the Existing Store filterred SSID list
            m_filterSsidslist.clear();
            m_filterFrequencies.clear();

            if(ssids)
            {
                string tmpssidlist{};
                while (ssids->Next(tmpssidlist) == true)
                {
                    if (!tmpssidlist.empty())
                    {
                        m_filterSsidslist.push_back(tmpssidlist.c_str());
                        NMLOG_DEBUG("%s added to SSID filtering", tmpssidlist.c_str());
                    }
                    else 
                    {
                        NMLOG_DEBUG("Empty SSID encountered in input list; skipping.");
                    }
                }
            }

            if (frequencies)
            {
                string frequency{};
                while (frequencies->Next(frequency) == true)
                {
                    if (!frequency.empty())
                    {
                        Core::JSON::EnumType<Exchange::INetworkManager::WIFIFrequency> parsedFrequency;
                        parsedFrequency.FromString(frequency);
                        const string normalizedFrequency = parsedFrequency.Data();
                        if ((!normalizedFrequency.empty()) && (normalizedFrequency == frequency))
                        {
                            m_filterFrequencies.push_back(normalizedFrequency);
                            NMLOG_DEBUG("Frequency %s added to scan filtering", normalizedFrequency.c_str());
                        }
                        else
                        {
                            NMLOG_ERROR("Invalid frequency value: %s", frequency.c_str());
                            return Core::ERROR_BAD_REQUEST;
                        }
                    }
                    else
                    {
                        NMLOG_DEBUG("Empty frequency encountered in input list; skipping.");
                    }
                }
            }

            nmEvent->setwifiScanOptions(true);
            if(wifi->wifiScanRequest(m_filterSsidslist))
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
                    if(ssids == nullptr) {
                        return Core::ERROR_GENERAL;
                    }
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

        uint32_t NetworkManagerImplementation::ConnectToKnownSSID(const string& ssid /* @in */)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(wifi->connectToKnownSSID(ssid))
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

            if(ssid.ssid.empty())
            {
                NMLOG_WARNING("ssid is empty activating last connected ssid !");
                if(_instance != NULL && wifi->activateKnownConnection(nmUtils::wlanIface(), _instance->m_lastConnectedSSID))
                {
                    rc = Core::ERROR_NONE;
                }
                else
                {
                    NMLOG_ERROR("activating last connected ssid failed");
                }
                return rc;
            }

            if(ssid.ssid.size() > 32)
            {
                NMLOG_WARNING("SSID is invalid");
                return rc;
            }

            if(!ssid.bssid.empty() && !nmUtils::isValidBSSID(ssid.bssid))
            {
                return Core::ERROR_GENERAL;
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

        uint32_t NetworkManagerImplementation::EthernetDeactivate(void)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(wifi->ethernetDeactivate())
                rc = Core::ERROR_NONE;
            return rc;
        }

        uint32_t NetworkManagerImplementation::ReacquireDHCPLease(const string& iface)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            if(wifi->reacquireDhcpLease(iface))
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

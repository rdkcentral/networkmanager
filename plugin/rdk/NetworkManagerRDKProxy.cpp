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
#include "NetworkManagerConnectivity.h"
#include "NetworkManagerRDKProxy.h"
#include "libIBus.h"
#include <chrono>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using namespace std;

namespace WPEC = WPEFramework::Core;
namespace WPEJ = WPEFramework::Core::JSON;

namespace WPEFramework
{
    namespace Plugin
    {
        NetworkManagerImplementation* _instance = nullptr;

        Exchange::INetworkManager::WiFiState to_wifi_state(WiFiStatusCode_t code) {
            switch (code)
            {
                case WIFI_DISCONNECTED:
                    return Exchange::INetworkManager::WIFI_STATE_DISCONNECTED;
                case WIFI_PAIRING:
                    return Exchange::INetworkManager::WIFI_STATE_PAIRING;
                case WIFI_CONNECTING:
                    return Exchange::INetworkManager::WIFI_STATE_CONNECTING;
                case WIFI_CONNECTED:
                    return Exchange::INetworkManager::WIFI_STATE_CONNECTED;
                case WIFI_FAILED:
                    return Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED;
                case WIFI_UNINSTALLED:
                    return Exchange::INetworkManager::WIFI_STATE_UNINSTALLED;
                case WIFI_DISABLED:
                    return Exchange::INetworkManager::WIFI_STATE_DISABLED;
            }
            return Exchange::INetworkManager::WIFI_STATE_INVALID;
        }

        Exchange::INetworkManager::WiFiState errorcode_to_wifi_state(WiFiErrorCode_t code) {
            switch (code)
            {
                case WIFI_SSID_CHANGED:
                    return Exchange::INetworkManager::WIFI_STATE_SSID_CHANGED;
                case WIFI_CONNECTION_LOST:
                    return Exchange::INetworkManager::WIFI_STATE_CONNECTION_LOST;
                case WIFI_CONNECTION_FAILED:
                    return Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED;
                case WIFI_CONNECTION_INTERRUPTED:
                    return Exchange::INetworkManager::WIFI_STATE_CONNECTION_INTERRUPTED;
                case WIFI_INVALID_CREDENTIALS:
                    return Exchange::INetworkManager::WIFI_STATE_INVALID_CREDENTIALS;
                case WIFI_AUTH_FAILED:
                    return Exchange::INetworkManager::WIFI_STATE_AUTHENTICATION_FAILED;
                case WIFI_NO_SSID:
                    return Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND;
                case WIFI_UNKNOWN:
                    return Exchange::INetworkManager::WIFI_STATE_ERROR;
            }
            return Exchange::INetworkManager::WIFI_STATE_INVALID;
        }

        static inline uint32_t mapToLegacySecurityMode(const uint32_t securityMode)
        {
            if (securityMode == 0)
                return 0; /* NET_WIFI_SECURITY_NONE */
            else if (securityMode == 1)
                return 6; /* NET_WIFI_SECURITY_WPA2_PSK_AES */
            else if (securityMode == 2)
                return 14; /* NET_WIFI_SECURITY_WPA3_SAE */
            else if (securityMode == 3)
                return 12; /* NET_WIFI_SECURITY_WPA_WPA2_ENTERPRISE */

            return 0; /* NET_WIFI_SECURITY_NONE */
        }

        static inline uint32_t mapToNewSecurityMode(const uint32_t legacyMode)
        {
            if ((legacyMode == NET_WIFI_SECURITY_NONE)      ||
                (legacyMode == NET_WIFI_SECURITY_WEP_64)    ||
                (legacyMode == NET_WIFI_SECURITY_WEP_128))
            {
                return 0; /* WIFI_SECURITY_NONE */
            }
            else if ((legacyMode == NET_WIFI_SECURITY_WPA_PSK_TKIP)  ||
                     (legacyMode == NET_WIFI_SECURITY_WPA_PSK_AES)   ||
                     (legacyMode == NET_WIFI_SECURITY_WPA2_PSK_TKIP) ||
                     (legacyMode == NET_WIFI_SECURITY_WPA2_PSK_AES)  ||
                     (legacyMode == NET_WIFI_SECURITY_WPA_WPA2_PSK)  ||
                     (legacyMode == NET_WIFI_SECURITY_WPA3_PSK_AES))
            {
                return 1; /* WIFI_SECURITY_WPA_PSK */
            }
            else if (legacyMode == NET_WIFI_SECURITY_WPA3_SAE)
            {
                return 2; /* WIFI_SECURITY_SAE */
            }
            else if ((legacyMode == NET_WIFI_SECURITY_WPA_ENTERPRISE_TKIP)  ||
                     (legacyMode == NET_WIFI_SECURITY_WPA_ENTERPRISE_AES)   ||
                     (legacyMode == NET_WIFI_SECURITY_WPA2_ENTERPRISE_TKIP) ||
                     (legacyMode == NET_WIFI_SECURITY_WPA2_ENTERPRISE_AES)  ||
                     (legacyMode == NET_WIFI_SECURITY_WPA_WPA2_ENTERPRISE))
            {
                return 3; /* WIFI_SECURITY_EAP */
            }

            return 0; /* WIFI_SECURITY_NONE */
        }

        static bool alreadySentRecently(std::string &interface)
        {
            // Use std::chrono::steady_clock for measuring elapsed time.
            // steady_clock is monotonic and not affected by system time changes (e.g., NTP synchronization),
            // ensuring timing is reliable even if the system clock is adjusted.
            static std::chrono::steady_clock::time_point eth_last_time = std::chrono::steady_clock::time_point{};
            static std::chrono::steady_clock::time_point wlan_last_time = std::chrono::steady_clock::time_point{};
            auto now = std::chrono::steady_clock::now();

            if (interface == "eth0")
            {
                if (eth_last_time != std::chrono::steady_clock::time_point{} &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - eth_last_time).count() < 500) { // 500 milliseconds threshold
                    return true;
                }
                eth_last_time = now;
            }
            else if (interface == "wlan0")
            {
                if (wlan_last_time != std::chrono::steady_clock::time_point{} &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - wlan_last_time).count() < 500) { // 500 milliseconds threshold
                    return true;
                }
                wlan_last_time = now;
            }

            return false;
        }

        void NetworkManagerInternalEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
        {
            LOG_ENTRY_FUNCTION();
            string interface;
            if (_instance)
            {
 //               ::_instance->Event();
                if (strcmp(owner, IARM_BUS_NM_SRV_MGR_NAME) != 0)
                {
                    NMLOG_ERROR("ERROR - unexpected event: owner %s, eventId: %d, data: %p, size: %d.", owner, (int)eventId, data, (int)len);
                    return;
                }
                if (data == nullptr || len == 0)
                {
                    NMLOG_ERROR("ERROR - event with NO DATA: eventId: %d, data: %p, size: %d.", (int)eventId, data, (int)len);
                    return;
                }

                switch (eventId)
                {
                    case IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS:
                    {
                        IARM_BUS_NetSrvMgr_Iface_EventInterfaceEnabledStatus_t *e = (IARM_BUS_NetSrvMgr_Iface_EventInterfaceEnabledStatus_t*) data;
                        interface = e->interface;
                        NMLOG_INFO ("IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS :: %s", interface.c_str());
                        if(interface == "eth0" || interface == "wlan0")
                        {
                            if (e->status)
                                ::_instance->ReportInterfaceStateChange(Exchange::INetworkManager::INTERFACE_ADDED, interface);
                            else
                                ::_instance->ReportInterfaceStateChange(Exchange::INetworkManager::INTERFACE_REMOVED, interface);
                        }
                        break;
                    }
                    case IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS:
                    {
                        IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t *e = (IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t*) data;
                        interface = e->interface;
                        NMLOG_INFO ("IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS :: %s", interface.c_str());
                        if(interface == "eth0" || interface == "wlan0") {
                            if (e->status)
                                ::_instance->ReportInterfaceStateChange(Exchange::INetworkManager::INTERFACE_LINK_UP, interface);
                            else
                               ::_instance->ReportInterfaceStateChange(Exchange::INetworkManager::INTERFACE_LINK_DOWN, interface);
                        }
                        break;
                    }
                    case IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS:
                    {
                        IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t *e = (IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t*) data;
                        interface = e->interface;
                        NMLOG_INFO ("IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS: %s - %s - %s", interface.c_str(), e->ip_address, e->acquired?"Acquired":"Lost");

                        if(interface == "eth0" || interface == "wlan0") {
                            string ipversion("IPv4");
                            Exchange::INetworkManager::IPStatus status = Exchange::INetworkManager::IP_LOST;
                            if(!e->is_ipv6 && alreadySentRecently(interface))
                            {
                                NMLOG_INFO("Already sent recently, skipping IP address change event for %s", interface.c_str());
                                break;
                            }

                            if (e->is_ipv6)
                                ipversion = "IPv6";
                            if (e->acquired)
                                status = Exchange::INetworkManager::IP_ACQUIRED;

                            ::_instance->ReportIPAddressChange(interface, ipversion, string(e->ip_address), status);
                        }
                        break;
                    }
                    case IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE:
                    {
                        string oldInterface;
                        string newInterface;
                        IARM_BUS_NetSrvMgr_Iface_EventDefaultInterface_t *e = (IARM_BUS_NetSrvMgr_Iface_EventDefaultInterface_t*) data;
                        oldInterface = e->oldInterface;
                        newInterface = e->newInterface;
                        NMLOG_INFO ("IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE %s :: %s..", oldInterface.c_str(), newInterface.c_str());
                        if(oldInterface != "eth0" && oldInterface != "wlan0")
                            oldInterface = ""; /* assigning "null" if the interface is not eth0 or wlan0 */
                        if(newInterface != "eth0" && newInterface != "wlan0")
                            newInterface = ""; /* assigning "null" if the interface is not eth0 or wlan0 */

                        ::_instance->ReportActiveInterfaceChange(oldInterface, newInterface);
                        break;
                    }
                    case IARM_BUS_WIFI_MGR_EVENT_onAvailableSSIDs:
                    {
                        IARM_BUS_WiFiSrvMgr_EventData_t *e = (IARM_BUS_WiFiSrvMgr_EventData_t*) data;
                        NMLOG_INFO ("IARM_BUS_WIFI_MGR_EVENT_onAvailableSSIDs");
                        std::string serialized(e->data.wifiSSIDList.ssid_list);
                        JsonObject eventDocument;
                        JsonArray ssidsUpdated;
                        uint32_t security;
                        WPEC::OptionalType<WPEJ::Error> error;
                        if (!WPEJ::IElement::FromString(serialized, eventDocument, error)) {
                            NMLOG_ERROR("Failed to parse JSON document containing SSIDs. Due to: %s", WPEJ::ErrorDisplayMessage(error).c_str());
                            break;
                        }
                        if ((!eventDocument.HasLabel("getAvailableSSIDs")) || (eventDocument["getAvailableSSIDs"].Content() != WPEJ::Variant::type::ARRAY)) {
                            NMLOG_ERROR("JSON document does not have key 'getAvailableSSIDs' as array");
                            break;
                        }

                        JsonArray ssids = eventDocument["getAvailableSSIDs"].Array();

                        for (int i = 0; i < ssids.Length(); i++)
                        {
                            JsonObject object = ssids[i].Object();
                            security = object["security"].Number();
                            object["security"] = mapToNewSecurityMode(security);
                            ssidsUpdated.Add(object);
                        }
                        ::_instance->ReportAvailableSSIDs(ssidsUpdated);
                        break;
                    }
                    case IARM_BUS_WIFI_MGR_EVENT_onWIFIStateChanged:
                    {
                        IARM_BUS_WiFiSrvMgr_EventData_t* e = (IARM_BUS_WiFiSrvMgr_EventData_t *) data;
                        Exchange::INetworkManager::WiFiState state = Exchange::INetworkManager::WIFI_STATE_DISCONNECTED;
                        NMLOG_INFO("Event IARM_BUS_WIFI_MGR_EVENT_onWIFIStateChanged received; state=%d", e->data.wifiStateChange.state);
                        state = to_wifi_state(e->data.wifiStateChange.state);
                        ::_instance->ReportWiFiStateChange(state);
                        break;
                    }
                    case IARM_BUS_WIFI_MGR_EVENT_onError:
                    {
                        IARM_BUS_WiFiSrvMgr_EventData_t* e = (IARM_BUS_WiFiSrvMgr_EventData_t *) data;
                        Exchange::INetworkManager::WiFiState state = errorcode_to_wifi_state(e->data.wifiError.code);
                        NMLOG_INFO("Event IARM_BUS_WIFI_MGR_EVENT_onError received; code=%d", e->data.wifiError.code);
                        ::_instance->ReportWiFiStateChange(state);
                        break;
                    }
                    default:
                    {
                        NMLOG_INFO("Event %d received; Unhandled", eventId);
                        break;
                    }
                }
            }
            else
                NMLOG_WARNING("WARNING - cannot handle IARM events without a Network plugin instance!");
        }

        void  NetworkManagerImplementation::threadEventRegistration(bool iarmInit, bool iarmConnect)
        {
            uint32_t retry = 0; 
            char c;
            IARM_Result_t retInit = IARM_RESULT_IPCCORE_FAIL;
            IARM_Result_t retConnect = IARM_RESULT_IPCCORE_FAIL;
            IARM_Result_t retIPC = IARM_RESULT_IPCCORE_FAIL;
            do  
            {
                if(iarmInit != true)
                {
                    retInit= IARM_Bus_Init("netsrvmgr-thunder"); 
                    if(retInit != IARM_RESULT_SUCCESS && retInit != IARM_RESULT_INVALID_STATE)
                    {
                        NMLOG_ERROR("IARM_Bus_Init failure, retrying. %d: %d", retry, retInit);
                        usleep(500 * 1000);
                        retry++;
                        continue;
                    }
                    iarmInit = true;
                }

                if(iarmConnect != true)
                {
                    retConnect = IARM_Bus_Connect();
                    if(retConnect != IARM_RESULT_SUCCESS)
                    {
                        NMLOG_ERROR("IARM_Bus_Connect failure, retrying. %d: %d", retry, retConnect);
                        usleep(500 * 1000);
                        retry++;
                        continue;
                    }
                     iarmConnect = true;
                }

                retIPC = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isAvailable, (void *)&c, sizeof(c), (1000*10)); 
                if (retIPC != IARM_RESULT_SUCCESS)
                {
                    NMLOG_ERROR("NetSrvMgr is not available. Failed to activate NetworkManager Plugin.%d: %d", retry, retIPC);
                    usleep(500 * 1000);
                    retry++;
                    continue;
                }
            } while(retIPC != IARM_RESULT_SUCCESS);

            IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS, NetworkManagerInternalEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, NetworkManagerInternalEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, NetworkManagerInternalEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE, NetworkManagerInternalEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERNET_CONNECTION_CHANGED, NetworkManagerInternalEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onWIFIStateChanged, NetworkManagerInternalEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onError, NetworkManagerInternalEventHandler);
            IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onAvailableSSIDs, NetworkManagerInternalEventHandler);

            NMLOG_INFO("threadEventRegistration successfully subscribed to IARM event for NetworkManager Plugin");
            /*
            * Read current network state and post the event.
            * Useful if NetworkManager plugin or WPEFramework is restarted
            * or netsrvmgr misses to post iarm events during bootup.
            */
            getInitialConnectionState();
        }

        void NetworkManagerImplementation::getInitialConnectionState()
        {
            // check the connection state and post event
            Exchange::INetworkManager::IInterfaceDetailsIterator* _interfaces{};
            uint32_t rc = GetAvailableInterfaces(_interfaces);
            size_t interfaceCount = 0;
            Exchange::INetworkManager::InterfaceDetails iface{};

            if (Core::ERROR_NONE == rc)
            {
                if (_interfaces != nullptr)
                {
                    while (_interfaces->Next(iface))
                    {
                        if(iface.enabled && iface.connected)
                        {
                            interfaceCount++;
                        }
                    }
                    _interfaces->Reset(0);
                    while (_interfaces->Next(iface) == true)
                    {
                        if((interfaceCount == 2 && "eth0" == iface.name) || interfaceCount == 1)
                        {
                            Core::JSON::EnumType<Exchange::INetworkManager::InterfaceType> type{iface.type};
                            if(iface.enabled)
                            {
                                NMLOG_INFO("'%s' interface is enabled", iface.name.c_str());
                                // ReportInterfaceStateChange(Exchange::INetworkManager::INTERFACE_ADDED, iface.name);
                                if(iface.connected)
                                {
                                    NMLOG_INFO("'%s' interface is connected", iface.name.c_str());
                                    if(m_defaultInterface != iface.name)
                                        ReportActiveInterfaceChange(m_defaultInterface, iface.name);
                                    Exchange::INetworkManager::IPAddress addrv4;
                                    Exchange::INetworkManager::IPAddress addrv6;
                                    std::string ipversion = "IPv4";
                                    rc = GetIPSettings(iface.name, ipversion, addrv4);
                                    if (Core::ERROR_NONE == rc)
                                    {
                                        if(!addrv4.ipaddress.empty()) {
                                            NMLOG_INFO("'%s' interface have ip '%s'", iface.name.c_str(), addrv4.ipaddress.c_str());
                                            ReportIPAddressChange(iface.name, addrv4.ipversion, addrv4.ipaddress, Exchange::INetworkManager::IP_ACQUIRED);
                                        }
                                    }
                                    ipversion = "IPv6";
                                    rc = GetIPSettings(iface.name, ipversion, addrv6);
                                    if (Core::ERROR_NONE == rc)
                                    {
                                        if(!addrv6.ipaddress.empty()) {
                                            NMLOG_INFO("'%s' interface have ip '%s'", iface.name.c_str(), addrv6.ipaddress.c_str());
                                            ReportIPAddressChange(iface.name, addrv6.ipversion, addrv6.ipaddress, Exchange::INetworkManager::IP_ACQUIRED);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    _interfaces->Release();
                }
            }
        }

        void NetworkManagerImplementation::platform_logging(const NetworkManagerLogger::LogLevel& level)
        {
            // TODO set the netsrvmgr logLevel
            return;
        }

                /* @brief Set the dhcp hostname */
        uint32_t NetworkManagerImplementation::SetHostname(const string& hostname /* @in */)
        {
            // TODO: Implement setting the DHCP hostname for netsrvmgr
            return Core::ERROR_NONE;
        }

        void NetworkManagerImplementation::platform_init()
        {
            LOG_ENTRY_FUNCTION();
            char c;

            ::_instance = this;
            uint32_t retry = 0;
            bool iarmInit = false;
            bool iarmConnect = false;
         
            IARM_Result_t retInit = IARM_RESULT_IPCCORE_FAIL;
            IARM_Result_t retConnect = IARM_RESULT_IPCCORE_FAIL;
            IARM_Result_t retIPC = IARM_RESULT_IPCCORE_FAIL;

            do
            {
                if(iarmInit != true)
                {
                    retInit = IARM_Bus_Init("netsrvmgr-thunder"); 
                    if(retInit != IARM_RESULT_SUCCESS && retInit != IARM_RESULT_INVALID_STATE)
                    {
                        NMLOG_ERROR("IARM_Bus_Init failure,retrying.%d: %d", retry, retInit);
                        usleep(500 * 1000);
                        retry++;
                        continue;
                    }
                    retInit = IARM_RESULT_SUCCESS;
                    iarmInit = true;
                }
                if(iarmConnect != true)
                {
                    retConnect  = IARM_Bus_Connect();
                    if(retConnect  != IARM_RESULT_SUCCESS)
                    {
                        NMLOG_ERROR("IARM_Bus_Connect failed,retrying. %d: %d", retry, retConnect);
                        usleep(500 * 1000);
                        retry++;
                        continue;
                    }
                    iarmConnect = true;
                }

                retIPC  = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isAvailable, (void *)&c, sizeof(c), (1000*10));
                if (retIPC  != IARM_RESULT_SUCCESS)
                {
                    NMLOG_ERROR("NetSrvMgr is not available. Failed to activate NetworkManager Plugin. %d: %d", retry, retIPC);
                    usleep(500 * 1000);
                    retry++;
                    continue;
                }
                if(retIPC == IARM_RESULT_SUCCESS && retConnect == IARM_RESULT_SUCCESS && retInit == IARM_RESULT_SUCCESS)
                {
                    break;
                }
            }while(retry < 3);

            if(retIPC != IARM_RESULT_SUCCESS)
            {
                string msg = "NetSrvMgr is not available";
                NMLOG_ERROR("NETWORK_NOT_READY: The NetSrvMgr Component is not available.Retrying in separate thread ::%s::", msg.c_str());
                m_registrationThread = thread(&NetworkManagerImplementation::threadEventRegistration, this, iarmInit, iarmConnect);
            }
            else
            {
                IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS, NetworkManagerInternalEventHandler);
                IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, NetworkManagerInternalEventHandler);
                IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, NetworkManagerInternalEventHandler);
                IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE, NetworkManagerInternalEventHandler);
                IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERNET_CONNECTION_CHANGED, NetworkManagerInternalEventHandler);
                IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onWIFIStateChanged, NetworkManagerInternalEventHandler);
                IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onError, NetworkManagerInternalEventHandler);
                IARM_Bus_RegisterEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onAvailableSSIDs, NetworkManagerInternalEventHandler);
                /*
                * Read current network state and post the event.
                * Useful if NetworkManager plugin or WPEFramework is restarted
                * or netsrvmgr misses to post iarm events during bootup.
                */
                std::thread connStateThread = std::thread(&NetworkManagerImplementation::getInitialConnectionState, this);
                connStateThread.join(); // seprate thread will not use the wpeframework thread pool
            }
        }

        uint32_t NetworkManagerImplementation::GetAvailableInterfaces (Exchange::INetworkManager::IInterfaceDetailsIterator*& interfacesItr/* @out */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_BUS_NetSrvMgr_InterfaceList_t list{};
            if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getInterfaceList, (void*)&list, sizeof(list)))
            {
                std::vector<InterfaceDetails> interfaceList;
                for (int i = 0; i < list.size; i++)
                {
                    NMLOG_DEBUG("Interface Name = %s", list.interfaces[i].name);
                    string interfaceName(list.interfaces[i].name);
                    if (("eth0" == interfaceName) || ("wlan0" == interfaceName))
                    {
                        InterfaceDetails tmp;
                        /* Update the interface as per RDK NetSrvMgr */
                        tmp.name         = interfaceName;
                        tmp.mac          = string(list.interfaces[i].mac);
                        tmp.enabled    = ((list.interfaces[i].flags & IFF_UP) != 0);
                        tmp.connected  = ((list.interfaces[i].flags & IFF_RUNNING) != 0);
                        if ("eth0" == interfaceName) {
                            tmp.type = Exchange::INetworkManager::INTERFACE_TYPE_ETHERNET;
                            m_ethConnected.store(tmp.connected);
                        }
                        else if ("wlan0" == interfaceName) {
                            tmp.type = Exchange::INetworkManager::INTERFACE_TYPE_WIFI;
                            m_wlanConnected.store(tmp.connected);
                        }

                        interfaceList.push_back(tmp);
                    }
                }
                using Implementation = RPC::IteratorType<Exchange::INetworkManager::IInterfaceDetailsIterator>;
                interfacesItr = Core::Service<Implementation>::Create<Exchange::INetworkManager::IInterfaceDetailsIterator>(interfaceList);

                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, __FUNCTION__);
            }

            return rc;
        }

        /* @brief Get the active Interface used for external world communication */
        uint32_t NetworkManagerImplementation::GetPrimaryInterface (string& interface /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_BUS_NetSrvMgr_DefaultRoute_t defaultRoute = {0};
            if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getDefaultInterface, (void*)&defaultRoute, sizeof(defaultRoute)))
            {
                NMLOG_INFO ("Call to %s for %s returned interface = %s, gateway = %s", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getDefaultInterface, defaultRoute.interface, defaultRoute.gateway);
                interface = m_defaultInterface = defaultRoute.interface;
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getDefaultInterface);
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::SetInterfaceState(const string& interface/* @in */, const bool enable /* @in */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_BUS_NetSrvMgr_Iface_EventData_t iarmData = { 0 };

            /* Netsrvmgr returns eth0 & wlan0 as primary interface but when we want to set., we must set ETHERNET or WIFI*/
            //TODO: Fix netsrvmgr to accept eth0 & wlan0
            if ("wlan0" == interface)
                strncpy(iarmData.setInterface, "WIFI", INTERFACE_SIZE);
            else if ("eth0" == interface)
                strncpy(iarmData.setInterface, "ETHERNET", INTERFACE_SIZE);
            else
            {
                rc = Core::ERROR_BAD_REQUEST;
                return rc;
            }

            iarmData.isInterfaceEnabled = enable;
            iarmData.persist = true;
            if (IARM_RESULT_SUCCESS == IARM_Bus_Call (IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setInterfaceEnabled, (void *)&iarmData, sizeof(iarmData)))
            {
                NMLOG_INFO ("Call to %s for %s success", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setInterfaceEnabled);
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setInterfaceEnabled);
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetInterfaceState(const string& interface/* @in */, bool &isEnabled /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_BUS_NetSrvMgr_Iface_EventData_t iarmData = { 0 };

            /* Netsrvmgr returns eth0 & wlan0 as primary interface but when we want to set., we must set ETHERNET or WIFI*/
            //TODO: Fix netsrvmgr to accept eth0 & wlan0
            if ("wlan0" == interface)
                strncpy(iarmData.setInterface, "WIFI", INTERFACE_SIZE);
            else if ("eth0" == interface)
                strncpy(iarmData.setInterface, "ETHERNET", INTERFACE_SIZE);
            else
            {
                rc = Core::ERROR_BAD_REQUEST;
                return rc;
            }

            if (IARM_RESULT_SUCCESS == IARM_Bus_Call (IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isInterfaceEnabled, (void *)&iarmData, sizeof(iarmData)))
            {
                NMLOG_DEBUG("Call to %s for %s success", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_isInterfaceEnabled);
                isEnabled = iarmData.isInterfaceEnabled;
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("Call to %s for %s failed", IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setInterfaceEnabled);
            }
            return rc;
        }

        /* function to convert netmask to prefix */
        uint32_t NetmaskToPrefix (const char* netmask_str)
        {
            uint32_t prefix_len = 0;
            uint32_t netmask1 = 0;
            uint32_t netmask2 = 0;
            uint32_t netmask3 = 0;
            uint32_t netmask4 = 0;
            uint32_t netmask = 0;
            sscanf(netmask_str, "%d.%d.%d.%d", &netmask1, &netmask2, &netmask3, &netmask4);
            netmask = netmask1 << 24;
            netmask |= netmask2 << 16;
            netmask |= netmask3 << 8;
            netmask |= netmask4;
            while (netmask)
            {
                if (netmask & 0x80000000)
                {
                    prefix_len++;
                    netmask <<= 1;
                } else
                {
                    break;
                }
            }
            return prefix_len;
        }


        /* @brief Get IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::GetIPSettings(string& interface /* @inout */, const string &ipversion /* @in */, IPAddress& address /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_BUS_NetSrvMgr_Iface_Settings_t iarmData = { 0 };
            /* Netsrvmgr returns eth0 & wlan0 as primary interface but when we want to set., we must set ETHERNET or WIFI*/
            //TODO: Fix netsrvmgr to accept eth0 & wlan0
            if ("wlan0" == interface)
                strncpy(iarmData.interface, "WIFI", INTERFACE_SIZE);
            else if ("eth0" == interface)
                strncpy(iarmData.interface, "ETHERNET", INTERFACE_SIZE);
            else if (!interface.empty())
            {
                NMLOG_ERROR("Given interface (%s) is NOT supported", interface.c_str());
                return Core::ERROR_NOT_SUPPORTED;
            }

            if (("IPv4" == ipversion) || ("IPv6" == ipversion))
                sprintf(iarmData.ipversion,"%s", ipversion.c_str());

            iarmData.isSupported = true;

            if (IARM_RESULT_SUCCESS == IARM_Bus_Call (IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_getIPSettings, (void *)&iarmData, sizeof(iarmData)))
            {
                address.autoconfig     = iarmData.autoconfig;
                address.dhcpserver     = string(iarmData.dhcpserver);
                address.ula            = string("");
                address.ipaddress      = string(iarmData.ipaddress);
                address.gateway        = string(iarmData.gateway);
                address.primarydns     = string(iarmData.primarydns);
                address.secondarydns   = string(iarmData.secondarydns);
                if (0 == strcasecmp("ipv4", iarmData.ipversion))
                {
                    address.ipversion = string ("IPv4");
                    address.prefix = NetmaskToPrefix(iarmData.netmask);
                }
                else if (0 == strcasecmp("ipv6", iarmData.ipversion))
                {
                    address.ipversion = string ("IPv6");
                    address.prefix = std::atoi(iarmData.netmask);
                }

                rc = Core::ERROR_NONE;
                /* Return the default interface information */
                if (interface.empty())
                {
                    string tmpInterface = string(iarmData.interface);
                    if ("ETHERNET" == tmpInterface)
                        interface = "eth0";
                    else if ("WIFI" == tmpInterface)
                        interface = "wlan0";
                    else
                        rc = Core::ERROR_BAD_REQUEST;
                }
            }
            else
            {
                NMLOG_ERROR("NetworkManagerImplementation::GetIPSettings - Calling IARM Failed");
            }

            return rc;
        }

const string CIDR_PREFIXES[CIDR_NETMASK_IP_LEN+1] = {
                                                     "0.0.0.0",
                                                     "128.0.0.0",
                                                     "192.0.0.0",
                                                     "224.0.0.0",
                                                     "240.0.0.0",
                                                     "248.0.0.0",
                                                     "252.0.0.0",
                                                     "254.0.0.0",
                                                     "255.0.0.0",
                                                     "255.128.0.0",
                                                     "255.192.0.0",
                                                     "255.224.0.0",
                                                     "255.240.0.0",
                                                     "255.248.0.0",
                                                     "255.252.0.0",
                                                     "255.254.0.0",
                                                     "255.255.0.0",
                                                     "255.255.128.0",
                                                     "255.255.192.0",
                                                     "255.255.224.0",
                                                     "255.255.240.0",
                                                     "255.255.248.0",
                                                     "255.255.252.0",
                                                     "255.255.254.0",
                                                     "255.255.255.0",
                                                     "255.255.255.128",
                                                     "255.255.255.192",
                                                     "255.255.255.224",
                                                     "255.255.255.240",
                                                     "255.255.255.248",
                                                     "255.255.255.252",
                                                     "255.255.255.254",
                                                     "255.255.255.255",
                                                   };

        /* @brief Set IP Address Of the Interface */
        uint32_t NetworkManagerImplementation::SetIPSettings(const string& interface /* @in */, const IPAddress& address /* @in */)
        {
            uint32_t rc = Core::ERROR_NONE;
            if (("IPv4" != address.ipversion) && ("IPv6" != address.ipversion))
            {
                return Core::ERROR_BAD_REQUEST;
            }
            if ("IPv4" == address.ipversion)
            {
                IARM_BUS_NetSrvMgr_Iface_Settings_t iarmData = {0};
                /* Netsrvmgr returns eth0 & wlan0 as primary interface but when we want to set., we must set ETHERNET or WIFI*/
                //TODO: Fix netsrvmgr to accept eth0 & wlan0
                if ("wlan0" == interface)
                    strncpy(iarmData.interface, "WIFI", INTERFACE_SIZE);
                else if ("eth0" == interface)
                    strncpy(iarmData.interface, "ETHERNET", INTERFACE_SIZE);
                else
                {
                    rc = Core::ERROR_BAD_REQUEST;
                    return rc;
                }

                /* IP version */
                sprintf(iarmData.ipversion, "IPv4");

                if (!address.autoconfig)
                {
                    //Lets validate the prefix Address
                    if (address.prefix > CIDR_NETMASK_IP_LEN)
                    {
                        rc = Core::ERROR_BAD_REQUEST;
                    }
                    else
                    {
                        string netmask = CIDR_PREFIXES[address.prefix];

                        //Lets validate the IP Address
                        struct in_addr tmpIPAddress, tmpGWAddress, tmpNetmask;
                        struct in_addr bcastAddr1, bcastAddr2;

                        if (inet_pton(AF_INET, address.ipaddress.c_str(), &tmpIPAddress) == 1 &&
                            inet_pton(AF_INET, netmask.c_str(), &tmpNetmask) == 1 &&
                            inet_pton(AF_INET, address.gateway.c_str(), &tmpGWAddress) == 1)
                        {
                            bcastAddr1.s_addr = tmpIPAddress.s_addr | ~tmpNetmask.s_addr;
                            bcastAddr2.s_addr = tmpGWAddress.s_addr | ~tmpNetmask.s_addr;

                            if (tmpIPAddress.s_addr == tmpGWAddress.s_addr)
                            {
                                NMLOG_INFO("Interface and Gateway IP are same , return false");
                                rc = Core::ERROR_BAD_REQUEST;
                            }
                            if (bcastAddr1.s_addr != bcastAddr2.s_addr)
                            {
                                NMLOG_INFO("Interface and Gateway IP is not in same broadcast domain, return false");
                                rc = Core::ERROR_BAD_REQUEST;
                            }
                            if (tmpIPAddress.s_addr == bcastAddr1.s_addr)
                            {
                                NMLOG_INFO("Interface and Broadcast IP is same, return false");
                                rc = Core::ERROR_BAD_REQUEST;
                            }
                            if (tmpGWAddress.s_addr == bcastAddr2.s_addr)
                            {
                                NMLOG_INFO("Gateway and Broadcast IP is same, return false");
                                rc = Core::ERROR_BAD_REQUEST;
                            }
                        }
                        else
                        {
                            rc = Core::ERROR_BAD_REQUEST;
                            NMLOG_INFO ("Given Input Address are not appropriate");
                        }

                        if (Core::ERROR_NONE == rc)
                        {
                            strncpy(iarmData.ipaddress, address.ipaddress.c_str(), 16);
                            strncpy(iarmData.netmask, netmask.c_str(), 16);
                            strncpy(iarmData.gateway, address.gateway.c_str(), 16);
                            strncpy(iarmData.primarydns, address.primarydns.c_str(), 16);
                            strncpy(iarmData.secondarydns, address.secondarydns.c_str(), 16);
                        }
                    }
                }
                else
                {
                    iarmData.autoconfig = address.autoconfig;
                }
                if (Core::ERROR_NONE == rc)
                {
                    if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETSRVMGR_API_setIPSettings, (void *) &iarmData, sizeof(iarmData)))
                    {
                        NMLOG_INFO("Set IP Successfully");
                    }
                    else
                    {
                        NMLOG_ERROR("Setting IP Failed");
                        rc = Core::ERROR_RPC_CALL_FAILED;
                    }
                }
            }
            else
            {
                //FIXME : Add IPv6 support here
                NMLOG_WARNING ("Setting IPv6 is not supported at this point in time. This is just a place holder");
                rc = Core::ERROR_NOT_SUPPORTED;
            }

            return rc;
        }

        uint32_t NetworkManagerImplementation::StartWiFiScan(const string& frequency /* @in */, IStringIterator* const ssids/* @in */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Bus_WiFiSrvMgr_SsidList_Param_t param{};
            IARM_Result_t retVal = IARM_RESULT_SUCCESS;

            //Cleared the Existing Store filterred SSID list
            m_filterSsidslist.clear();
            m_filterfrequency.clear();

            if(ssids)
            {
                string ssidlist{};
                while (ssids->Next(ssidlist) == true)
                {
                    m_filterSsidslist.push_back(ssidlist.c_str());
                    NMLOG_DEBUG("%s added to SSID filtering", ssidlist.c_str());
                }
            }

            if (!frequency.empty())
            {
                m_filterfrequency = frequency;
                NMLOG_DEBUG("Scan SSIDs of frequency %s", m_filterfrequency.c_str());
            }

            memset(&param, 0, sizeof(param));

            retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_getAvailableSSIDsAsync, (void *)&param, sizeof(IARM_Bus_WiFiSrvMgr_SsidList_Param_t));

            if(retVal == IARM_RESULT_SUCCESS) {
                NMLOG_INFO ("Scan started");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("StartScan failed");
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::StopWiFiScan(void)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Bus_WiFiSrvMgr_Param_t param{};
            memset(&param, 0, sizeof(param));

            if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_stopProgressiveWifiScanning, (void*) &param, sizeof(IARM_Bus_WiFiSrvMgr_Param_t)))
            {
                NMLOG_INFO ("StopScan Success");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("StopScan failed");
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetKnownSSIDs(IStringIterator*& ssids /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Result_t retVal = IARM_RESULT_SUCCESS;
            IARM_Bus_WiFiSrvMgr_Param_t param{};

            memset(&param, 0, sizeof(param));

            /* Must add new method to get all the known SSIDs but for now RDK-NM supports only one active SSID. So we repurpose this method */
            retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_getConnectedSSID, (void *)&param, sizeof(param));

            if(retVal == IARM_RESULT_SUCCESS)
            {
                auto &connectedSsid = param.data.getConnectedSSID;
                std::list<string> ssidList;
                ssidList.push_back(string(connectedSsid.ssid));
                NMLOG_INFO ("GetKnownSSIDs Success");

                ssids = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(ssidList);
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("GetKnownSSIDs failed");
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::AddToKnownSSIDs(const WiFiConnectTo& ssid /* @in */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Bus_WiFiSrvMgr_Param_t param{};
            memset(&param, 0, sizeof(param));

            strncpy(param.data.connect.ssid, ssid.ssid.c_str(), SSID_SIZE - 1);
            strncpy(param.data.connect.passphrase, ssid.passphrase.c_str(), PASSPHRASE_BUFF - 1);
            param.data.connect.security_mode = (SsidSecurity) mapToLegacySecurityMode(ssid.security);

            IARM_Result_t retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_saveSSID, (void *)&param, sizeof(param));
            if((retVal == IARM_RESULT_SUCCESS) && param.status)
            {
                NMLOG_INFO ("AddToKnownSSIDs Success");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("AddToKnownSSIDs failed");
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::RemoveKnownSSID(const string& ssid /* @in */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Bus_WiFiSrvMgr_Param_t param{};
            memset(&param, 0, sizeof(param));

            /* Currently RDK-NM supports only one saved SSID. So when you say clear, it jsut clears it. No need to pass input at this point in time.
               Will be updated to pass specific ssid when we support more than 1 ssid.
             */
            (void)ssid;

            IARM_Result_t retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_clearSSID, (void *)&param, sizeof(param));
            if((retVal == IARM_RESULT_SUCCESS) && param.status)
            {
                NMLOG_INFO ("RemoveKnownSSID Success");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("RemoveKnownSSID failed");
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::WiFiConnect(const WiFiConnectTo& ssid /* @in */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Result_t retVal = IARM_RESULT_SUCCESS;
            IARM_Bus_WiFiSrvMgr_Param_t param{};
            memset(&param, 0, sizeof(param));

            if(ssid.ssid.length() || ssid.passphrase.length())
            {
                ssid.ssid.copy(param.data.connect.ssid, sizeof(param.data.connect.ssid) - 1);
                ssid.passphrase.copy(param.data.connect.passphrase, sizeof(param.data.connect.passphrase) - 1);
                param.data.connect.security_mode = (SsidSecurity)mapToLegacySecurityMode(ssid.security);
                if(!ssid.eap_identity.empty())
                    ssid.eap_identity.copy(param.data.connect.eapIdentity, sizeof(param.data.connect.eapIdentity) - 1);
                if(!ssid.ca_cert.empty())
                    ssid.ca_cert.copy(param.data.connect.carootcert, sizeof(param.data.connect.carootcert) - 1);
                if(!ssid.client_cert.empty())
                    ssid.client_cert.copy(param.data.connect.clientcert, sizeof(param.data.connect.clientcert) - 1);
                if(!ssid.private_key.empty())
                    ssid.private_key.copy(param.data.connect.privatekey, sizeof(param.data.connect.privatekey) - 1);
                if(!ssid.private_key_passwd.empty())
                {
                    ssid.private_key_passwd.copy(param.data.connect.privatekeypasswd, sizeof(param.data.connect.privatekeypasswd) - 1);
                }
                param.data.connect.persistSSIDInfo = ssid.persist;
            }

            retVal = IARM_Bus_Call( IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_connect, (void *)&param, sizeof(param));

            if((retVal == IARM_RESULT_SUCCESS) && param.status)
            {
                NMLOG_INFO ("WiFiConnect Success");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("WiFiConnect failed");
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::WiFiDisconnect(void)
        {
            LOG_ENTRY_FUNCTION();
            IARM_Result_t retVal = IARM_RESULT_SUCCESS;
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Bus_WiFiSrvMgr_Param_t param{};
            memset(&param, 0, sizeof(param));

            retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_disconnectSSID, (void *)&param, sizeof(param));
            if ((retVal == IARM_RESULT_SUCCESS) && param.status)
            {
                NMLOG_INFO ("WiFiDisconnect started");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("WiFiDisconnect failed");
            }
            return rc;
        }

        uint32_t NetworkManagerImplementation::GetConnectedSSID(WiFiSSIDInfo&  ssidInfo /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Result_t retVal = IARM_RESULT_SUCCESS;
            IARM_Bus_WiFiSrvMgr_Param_t param{};

            memset(&param, 0, sizeof(param));

            /* Must add new method to get all the known SSIDs but for now RDK-NM supports only one active SSID. So we repurpose this method */
            retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_getConnectedSSID, (void *)&param, sizeof(param));

            if(retVal == IARM_RESULT_SUCCESS)
            {
                auto &connectedSsid = param.data.getConnectedSSID;

                ssidInfo.ssid             = string(connectedSsid.ssid);
                ssidInfo.bssid            = string(connectedSsid.bssid);
                ssidInfo.security         = (WIFISecurityMode)mapToNewSecurityMode(connectedSsid.securityMode);
                ssidInfo.strength         = to_string((int)connectedSsid.signalStrength);
                ssidInfo.rate             = to_string((int)connectedSsid.rate);
                if(connectedSsid.noise <= 0 && connectedSsid.noise >= DEFAULT_NOISE)
                    ssidInfo.noise        = to_string((int)connectedSsid.noise);
                else
                    ssidInfo.noise        = to_string(0);

                std::string freqStr       = to_string((double)connectedSsid.frequency/1000);
                ssidInfo.frequency        = freqStr.substr(0, 5);

                NMLOG_INFO ("GetConnectedSSID Success");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("GetConnectedSSID failed");
            }
            return rc;
        }

#if 0
        uint32_t NetworkManagerImplementation::GetWiFiSignalQuality(string& ssid /* @out */, string& strength /* @out */, WiFiSignalQuality& quality /* @out */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            WiFiSSIDInfo  ssidInfo{};
            float rssi = 0.0f;
            float noise = 0.0f;
            float floatStrength = 0.0f;
            unsigned int strengthOut = 0;

            if (Core::ERROR_NONE == GetConnectedSSID(ssidInfo))
            {
                ssid              = ssidInfo.ssid;
                if (!ssidInfo.strength.empty())
                    rssi          = std::stof(ssidInfo.strength.c_str());
                if (!ssidInfo.noise.empty())
                    noise         = std::stof(ssidInfo.noise.c_str());
                floatStrength = (rssi - noise);
                if (floatStrength < 0)
                    floatStrength = 0.0;

                strengthOut = static_cast<unsigned int>(floatStrength);
                NMLOG_INFO ("WiFiSignalQuality in dB = %u",strengthOut);

                if (strengthOut == 0)
                {
                    quality = WIFI_SIGNAL_DISCONNECTED;
                    strength = "0";
                }
                else if (strengthOut > 0 && strengthOut < NM_WIFI_SNR_THRESHOLD_FAIR)
                {
                    quality = WIFI_SIGNAL_WEAK;
                }
                else if (strengthOut > NM_WIFI_SNR_THRESHOLD_FAIR && strengthOut < NM_WIFI_SNR_THRESHOLD_GOOD)
                {
                    quality = WIFI_SIGNAL_FAIR;
                }
                else if (strengthOut > NM_WIFI_SNR_THRESHOLD_GOOD && strengthOut < NM_WIFI_SNR_THRESHOLD_EXCELLENT)
                {
                    quality = WIFI_SIGNAL_GOOD;
                }
                else
                {
                    quality = WIFI_SIGNAL_EXCELLENT;
                }

                strength = std::to_string(strengthOut);
                NMLOG_INFO ("GetWiFiSignalQuality success");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("GetWiFiSignalQuality failed");
            }
            return rc;
        }
#endif

        uint32_t NetworkManagerImplementation::StartWPS(const WiFiWPS& method /* @in */, const string& wps_pin /* @in */)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Bus_WiFiSrvMgr_WPS_Parameters_t wps_parameters{};
            if (method == WIFI_WPS_PBC)
            {
                wps_parameters.pbc = true;
            }
            else if (method == WIFI_WPS_PIN)
            {
                snprintf(wps_parameters.pin, sizeof(wps_parameters.pin), "%s", wps_pin.c_str());
                wps_parameters.pbc = false;
            }
            else if (method == WIFI_WPS_SERIALIZED_PIN)
            {
                snprintf(wps_parameters.pin, sizeof(wps_parameters.pin), "xxxxxxxx");
                wps_parameters.pbc = false;
            }
            wps_parameters.status = false;

            if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_initiateWPSPairing2, (void *)&wps_parameters, sizeof(wps_parameters)))
            {
                if (wps_parameters.status)
                {
                    NMLOG_INFO ("StartWPS is success");
                    rc = Core::ERROR_NONE;
                }
                else
                    NMLOG_ERROR ("StartWPS: Failed");
            }

            if (Core::ERROR_NONE != rc)
            {
                NMLOG_ERROR ("StartWPS: Failed");
            }

            return rc;
        }

        uint32_t NetworkManagerImplementation::StopWPS(void)
        {
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Bus_WiFiSrvMgr_Param_t param;
            memset(&param, 0, sizeof(param));

            if (IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_cancelWPSPairing, (void *)&param, sizeof(param)))
            {
                NMLOG_INFO ("StopWPS is success");
                rc = Core::ERROR_NONE;
            }
            else
            {
                NMLOG_ERROR ("StopWPS: Failed");
            }

            return rc;
        }

        uint32_t NetworkManagerImplementation::GetWifiState(WiFiState &state)
        {
            LOG_ENTRY_FUNCTION();
            uint32_t rc = Core::ERROR_RPC_CALL_FAILED;
            IARM_Bus_WiFiSrvMgr_Param_t param;
            memset(&param, 0, sizeof(param));

            if(IARM_RESULT_SUCCESS == IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_getCurrentState, (void *)&param, sizeof(param)))
            {
                state = to_wifi_state(param.data.wifiStatus);
                rc = Core::ERROR_NONE;
            }
            return rc;
        }
    }
}

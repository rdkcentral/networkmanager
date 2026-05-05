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
#include <glib.h>
#include <thread>
#include <string>
#include <map>
#include <NetworkManager.h>
#include "Module.h"
#include "NetworkManagerGnomeEvents.h"
#include "NetworkManagerLogger.h"
#include "NetworkManagerGnomeUtils.h"
#include "NetworkManagerImplementation.h"
#include "INetworkManager.h"
#include <set>
#include <arpa/inet.h>
#ifndef IN_IS_ADDR_LINKLOCAL
#define IN_IS_ADDR_LINKLOCAL(a) ((((uint32_t)ntohl(a)) & 0xffff0000U) == 0xa9fe0000U)
#endif
#ifdef ENABLE_MIGRATION_MFRMGR_SUPPORT
#include "NetworkManagerGnomeMfrMgr.h"
#endif

namespace WPEFramework
{
    namespace Plugin
    {

    extern NetworkManagerImplementation* _instance;
    static GnomeNetworkManagerEvents *_nmEventInstance = nullptr;

    static void primaryConnectionCb(NMClient *client, GParamSpec *param, NMEvents *nmEvents)
    {
        NMActiveConnection *primaryConn;
        const char *activeConnId = NULL;
        const char *connectionTyp = NULL;
        primaryConn = nm_client_get_primary_connection(client);
        nmEvents->activeConn = primaryConn;
        std::string newIface ="unknown";
        if (primaryConn)
        {
            activeConnId = nm_active_connection_get_id(primaryConn);
            connectionTyp = nm_active_connection_get_connection_type(primaryConn);
            NMLOG_INFO("active connection - %s (%s)", activeConnId, connectionTyp);

            if (0 == strncmp("802-3-ethernet", connectionTyp, sizeof("802-3-ethernet")) && string("eth0_missing") != nmUtils::ethIface())
                newIface = nmUtils::ethIface();
            else if(0 == strncmp("802-11-wireless", connectionTyp, sizeof("802-11-wireless")) && string("wlan0_missing") != nmUtils::wlanIface())
                newIface = nmUtils::wlanIface();
            else {
                NMLOG_WARNING("Active connection not valid: Ethernet/WiFi ID: %s", connectionTyp);
                return; // if not good don't report the evnet
            }

            GnomeNetworkManagerEvents::onActiveInterfaceChangeCb(newIface);
        }
        else
        {
            GnomeNetworkManagerEvents::onActiveInterfaceChangeCb(newIface);
            NMLOG_WARNING("now there's no active connection");
        }
    }

    void GnomeNetworkManagerEvents::deviceStateChangeCb(NMDevice *device, GParamSpec *pspec, NMEvents *nmEvents)
    {
        static bool isEthDisabled = false;
        static bool isWlanDisabled = false;
        if(device == nullptr)
            return;
        NMDeviceState deviceState;
        deviceState = nm_device_get_state(device);
        std::string ifname = nm_device_get_iface(device);
        NMDeviceStateReason reason = nm_device_get_state_reason(device);
        if(ifname == nmUtils::wlanIface())
        {
            if(!NM_IS_DEVICE_WIFI(device)) {
                NMLOG_FATAL("not a wifi device !");
                return;
            }
            std::string wifiState;
            switch (reason)
            {
                case NM_DEVICE_STATE_REASON_SUPPLICANT_AVAILABLE:
                    wifiState = "WIFI_STATE_UNINSTALLED";
                    GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_UNINSTALLED);
                    break;
                case NM_DEVICE_STATE_REASON_SSID_NOT_FOUND:
                    wifiState = "WIFI_STATE_SSID_NOT_FOUND";
                    GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
                    break;
                case NM_DEVICE_STATE_REASON_SUPPLICANT_TIMEOUT:         // supplicant took too long to authenticate
                case NM_DEVICE_STATE_REASON_NO_SECRETS:
                    wifiState = "WIFI_STATE_AUTHENTICATION_FAILED";
                    GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_AUTHENTICATION_FAILED);
                    break;
                case NM_DEVICE_STATE_REASON_SUPPLICANT_FAILED:          //  802.1x supplicant failed
                    wifiState = "WIFI_STATE_ERROR";
                    GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_ERROR);
                    break;
                case NM_DEVICE_STATE_REASON_SUPPLICANT_CONFIG_FAILED:   // 802.1x supplicant configuration failed
                    wifiState = "WIFI_STATE_CONNECTION_INTERRUPTED";
                    GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTION_INTERRUPTED);
                    break;
                case NM_DEVICE_STATE_REASON_SUPPLICANT_DISCONNECT:      // 802.1x supplicant disconnected
                    wifiState = "WIFI_STATE_INVALID_CREDENTIALS";
                    GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_INVALID_CREDENTIALS);
                    break;
                default:
                {
                    switch (deviceState)
                    {
                    case NM_DEVICE_STATE_UNKNOWN:
                        wifiState = "WIFI_STATE_UNINSTALLED";
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_UNINSTALLED);
                        GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, nmUtils::wlanIface());
                        break;
                    case NM_DEVICE_STATE_UNMANAGED:
                        wifiState = "WIFI_STATE_DISABLED";
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_DISABLED);
                        GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, nmUtils::wlanIface());
                        isWlanDisabled = true;
                        break;
                    case NM_DEVICE_STATE_UNAVAILABLE:
                    case NM_DEVICE_STATE_DISCONNECTED:
                        wifiState = "WIFI_STATE_DISCONNECTED";
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_DISCONNECTED);
                        GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_DOWN, nmUtils::wlanIface());
                        break;
                    case NM_DEVICE_STATE_PREPARE:
                        wifiState = "WIFI_STATE_PAIRING";
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_PAIRING);
                        break;
                    case NM_DEVICE_STATE_CONFIG:
                        wifiState = "WIFI_STATE_CONNECTING";
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTING);
                        break;
                    case NM_DEVICE_STATE_IP_CONFIG:
                        wifiState = "NM_DEVICE_STATE_IP_CONFIG";
                        GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_UP, nmUtils::wlanIface());
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTED);
                        break;
                    case NM_DEVICE_STATE_IP_CHECK:
                        wifiState = "NM_DEVICE_STATE_IP_CHECK";
                        //GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ACQUIRING_IP, nmUtils::wlanIface());
                        break;
                    case NM_DEVICE_STATE_SECONDARIES:
                        wifiState = "NM_DEVICE_STATE_SECONDARIES";
                        GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ACQUIRING_IP, nmUtils::wlanIface());
                        break;
                    case NM_DEVICE_STATE_ACTIVATED:
                        wifiState = "WIFI_STATE_CONNECTED";
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTED);
#if USE_TELEMETRY
                        {
                            static std::string lastWlanGatewayMac;
                            std::string gatewayMac = nmUtils::getGatewayMacAddress(device);
                            if (!gatewayMac.empty() && lastWlanGatewayMac != gatewayMac) {
                                lastWlanGatewayMac = gatewayMac;
                                NMLOG_INFO("NM_WIFI_GW_MAC = %s", gatewayMac.c_str());
                                if (_instance != nullptr)
                                {
                                    _instance->logTelemetry("NM_WIFI_GW_MAC", gatewayMac);
                                }
                            }
                        }
#endif
                        break;
                    case NM_DEVICE_STATE_DEACTIVATING:
                        wifiState = "WIFI_STATE_CONNECTION_LOST";
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTION_LOST);
                        break;
                    case NM_DEVICE_STATE_FAILED:
                        wifiState = "WIFI_STATE_CONNECTION_FAILED";
                        GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED);
                        break;
                    case NM_DEVICE_STATE_NEED_AUTH:
                        //GnomeNetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTION_INTERRUPTED);
                        wifiState = "WIFI_STATE_CONNECTION_INTERRUPTED";
                        break;
                    default:
                        wifiState = "Un handiled: " ;
                        wifiState += std::to_string(deviceState);
                    }

                    if(isWlanDisabled && deviceState > NM_DEVICE_STATE_UNMANAGED)
                    {
                        isWlanDisabled = false;
                        GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ADDED, nmUtils::wlanIface());
                    }

                    break;
                }
            }
            NMLOG_INFO("wifi state: %s; reason: %d", wifiState.c_str(), static_cast<int>(reason));
        }
        else if(ifname == nmUtils::ethIface())
        {
            switch (deviceState)
            {
                case NM_DEVICE_STATE_UNKNOWN:
                case NM_DEVICE_STATE_UNMANAGED:
                    GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, nmUtils::ethIface());
                    isEthDisabled = true;
                    break;
                case NM_DEVICE_STATE_UNAVAILABLE:
                case NM_DEVICE_STATE_DISCONNECTED:
                    GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_DOWN, nmUtils::ethIface());
                break;
                case NM_DEVICE_STATE_PREPARE:
                    GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_UP, nmUtils::ethIface());
                break;
                case NM_DEVICE_STATE_IP_CONFIG:
                    GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ACQUIRING_IP, nmUtils::ethIface());
                break;
                case NM_DEVICE_STATE_ACTIVATED:
#if USE_TELEMETRY
                    {
                        static std::string lastEthGatewayMac;
                        std::string gatewayMac = nmUtils::getGatewayMacAddress(device);
                        if (!gatewayMac.empty() && lastEthGatewayMac != gatewayMac) {
                            lastEthGatewayMac = gatewayMac;
                            NMLOG_INFO("NM_ETHERNET_GW_MAC = %s", gatewayMac.c_str());
                            if (_instance != nullptr)
                                _instance->logTelemetry("NM_ETHERNET_GW_MAC", gatewayMac);
                        }
                    }
#endif
                break;
                case NM_DEVICE_STATE_NEED_AUTH:
                case NM_DEVICE_STATE_SECONDARIES:
                case NM_DEVICE_STATE_DEACTIVATING:
                default:
                    NMLOG_WARNING("Unhandiled state change %d", deviceState);
            }

            /*
            * Post interface added event:
            * When the Ethernet interface is removed, its state goes to UNKNOWN or UNMANAGED.
            * normaly interface is always up and we are not removing it
            */
            if(isEthDisabled && deviceState > NM_DEVICE_STATE_UNMANAGED)
            {
                isEthDisabled = false;
                GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ADDED, nmUtils::ethIface());
            }

            NMLOG_INFO("eth0 state: %d; reason: %d", static_cast<int>(deviceState), static_cast<int>(reason));
        }
    }

    /* Build a fresh IpFamilyCache from current libnm state for one device/family,
       swap it into _instance under the cache mutex, then emit acquired/lost events
       for any address-set differences outside the lock. */
    void refreshIpFamilyCache(NMDevice* device, bool isIPv6)
    {
        if (!device || !NM_IS_DEVICE(device) || !_instance)
            return;

        const char* iface = nm_device_get_iface(device);
        if (!iface) return;
        std::string ifname = iface;

        bool isEth  = (ifname == nmUtils::ethIface());
        bool isWlan = (ifname == nmUtils::wlanIface());
        if (!isEth && !isWlan) return;

        /* Build the new snapshot locally (no locks held during NM calls). */
        IpFamilyCache newCache;
        NMActiveConnection* conn = nm_device_get_active_connection(device);
        if (conn) {
            /* autoconfig: method "auto" or "dhcp" → true */
            NMConnection* nmConn = NM_CONNECTION(nm_active_connection_get_connection(conn));
            if (nmConn) {
                NMSettingIPConfig* ipSetting = isIPv6
                    ? NM_SETTING_IP_CONFIG(nm_connection_get_setting_ip6_config(nmConn))
                    : NM_SETTING_IP_CONFIG(nm_connection_get_setting_ip4_config(nmConn));
                if (ipSetting) {
                    const char* method = nm_setting_ip_config_get_method(ipSetting);
                    newCache.autoconfig = method &&
                        (g_strcmp0(method, "auto") == 0 || g_strcmp0(method, "dhcp") == 0);
                }
            }

            NMIPConfig* ipConfig = isIPv6
                ? nm_device_get_ip6_config(device)
                : nm_device_get_ip4_config(device);

            if (ipConfig) {
                GPtrArray* addresses = nm_ip_config_get_addresses(ipConfig);
                if (addresses) {
                    for (guint i = 0; i < addresses->len; i++) {
                        NMIPAddress* addr = (NMIPAddress*)g_ptr_array_index(addresses, i);
                        if (!addr) continue;
                        const char* addrStr = nm_ip_address_get_address(addr);
                        if (!addrStr) continue;
                        std::string addrString = addrStr;
                        if (isIPv6) {
                            if (isIPv6LinkLocal(addrString)) {
                                newCache.linkLocalAddress = addrString;
                            } else {
                                newCache.globalAddresses.emplace(addrString, nm_ip_address_get_prefix(addr));
                            }
                        } else {
                            struct sockaddr_in sa{};
                            if (inet_pton(AF_INET, addrString.c_str(), &sa.sin_addr) == 1 &&
                                IN_IS_ADDR_LINKLOCAL(sa.sin_addr.s_addr)) {
                                newCache.linkLocalAddress = addrString;
                            } else {
                                newCache.globalAddresses.emplace(addrString, nm_ip_address_get_prefix(addr));
                            }
                        }
                    }
                }

                const char* gw = nm_ip_config_get_gateway(ipConfig);
                if (gw) newCache.gateway = gw;

                const char* const* dnsArr = nm_ip_config_get_nameservers(ipConfig);
                if (dnsArr && dnsArr[0]) {
                    newCache.primarydns = dnsArr[0];
                    if (dnsArr[1]) newCache.secondarydns = dnsArr[1];
                }

                NMDhcpConfig* dhcpConfig = isIPv6
                    ? nm_active_connection_get_dhcp6_config(conn)
                    : nm_active_connection_get_dhcp4_config(conn);
                if (dhcpConfig) {
                    const char* server = nm_dhcp_config_get_one_option(dhcpConfig, "dhcp_server_identifier");
                    if (server) newCache.dhcpserver = server;
                }

                newCache.valid = true;
            }
        }

        /* Swap new snapshot into instance cache under mutex; collect old address keys. */
        std::set<std::string> oldKeys;
        {
            std::lock_guard<std::mutex> lock(_instance->m_ipCacheMutex);
            IpFamilyCache* cache = nullptr;
            if (isEth)
                cache = isIPv6 ? &_instance->m_ethIPv6Cache : &_instance->m_ethIPv4Cache;
            else
                cache = isIPv6 ? &_instance->m_wlanIPv6Cache : &_instance->m_wlanIPv4Cache;
            for (const auto& kv : cache->globalAddresses) oldKeys.insert(kv.first);
            *cache = newCache;
        }

        /* Emit address acquired/lost events from map-key diff (outside the lock). */
        std::string family = isIPv6 ? "IPv6" : "IPv4";
        for (const auto& kv : newCache.globalAddresses) {
            if (oldKeys.find(kv.first) == oldKeys.end()) {
                NMLOG_INFO("IP acquired: %s %s %s", ifname.c_str(), family.c_str(), kv.first.c_str());
                _instance->ReportIPAddressChange(ifname, family, kv.first, Exchange::INetworkManager::IP_ACQUIRED);
            }
        }
        for (const auto& key : oldKeys) {
            if (newCache.globalAddresses.find(key) == newCache.globalAddresses.end()) {
                NMLOG_INFO("IP lost: %s %s %s", ifname.c_str(), family.c_str(), key.c_str());
                _instance->ReportIPAddressChange(ifname, family, key, Exchange::INetworkManager::IP_LOST);
            }
        }
    }

    static void ip4ChangedCb(NMIPConfig *ipConfig, GParamSpec *pspec, gpointer userData)
    {
        NMDevice *device = (NMDevice*)userData;
        if (!device || !NM_IS_DEVICE(device)) return;
        refreshIpFamilyCache(device, false);
    }

    static void ip6ChangedCb(NMIPConfig *ipConfig, GParamSpec *pspec, gpointer userData)
    {
        NMDevice *device = (NMDevice*)userData;
        if (!device || !NM_IS_DEVICE(device)) return;
        refreshIpFamilyCache(device, true);
    }

    /* Called when DHCP options change mid-lease (e.g. renewed with different server/options). */
    static void dhcp4OptionsCb(NMDhcpConfig *dhcpConfig, GParamSpec *pspec, gpointer userData)
    {
        NMDevice *device = (NMDevice*)userData;
        if (!device || !NM_IS_DEVICE(device)) return;
        refreshIpFamilyCache(device, false);
    }

    static void dhcp6OptionsCb(NMDhcpConfig *dhcpConfig, GParamSpec *pspec, gpointer userData)
    {
        NMDevice *device = (NMDevice*)userData;
        if (!device || !NM_IS_DEVICE(device)) return;
        refreshIpFamilyCache(device, true);
    }

    /* Called when the ip4-config or ip6-config object on a device is replaced
       (e.g. after reconnect).  Re-attaches notify handlers to the new object. */
    static void ip4ConfigChangedCb(NMDevice *device, GParamSpec *pspec, gpointer userData)
    {
        if (!device || !NM_IS_DEVICE(device)) return;
        NMIPConfig* ipv4Config = nm_device_get_ip4_config(device);
        if (ipv4Config) {
            g_signal_handlers_disconnect_by_func(ipv4Config, (gpointer)ip4ChangedCb, device);
            g_signal_connect(ipv4Config, "notify::addresses",   G_CALLBACK(ip4ChangedCb), device);
            g_signal_connect(ipv4Config, "notify::gateway",     G_CALLBACK(ip4ChangedCb), device);
            g_signal_connect(ipv4Config, "notify::nameservers", G_CALLBACK(ip4ChangedCb), device);
        }
        /* Re-attach DHCP options handler to the (possibly new) DHCP config object. */
        NMActiveConnection* conn4 = nm_device_get_active_connection(device);
        if (conn4) {
            NMDhcpConfig* dhcp4 = nm_active_connection_get_dhcp4_config(conn4);
            if (dhcp4) {
                g_signal_handlers_disconnect_by_func(dhcp4, (gpointer)dhcp4OptionsCb, device);
                g_signal_connect(dhcp4, "notify::options", G_CALLBACK(dhcp4OptionsCb), device);
            }
        }
        refreshIpFamilyCache(device, false);
    }

    static void ip6ConfigChangedCb(NMDevice *device, GParamSpec *pspec, gpointer userData)
    {
        if (!device || !NM_IS_DEVICE(device)) return;
        NMIPConfig* ipv6Config = nm_device_get_ip6_config(device);
        if (ipv6Config) {
            g_signal_handlers_disconnect_by_func(ipv6Config, (gpointer)ip6ChangedCb, device);
            g_signal_connect(ipv6Config, "notify::addresses",   G_CALLBACK(ip6ChangedCb), device);
            g_signal_connect(ipv6Config, "notify::gateway",     G_CALLBACK(ip6ChangedCb), device);
            g_signal_connect(ipv6Config, "notify::nameservers", G_CALLBACK(ip6ChangedCb), device);
        }
        /* Re-attach DHCP options handler to the (possibly new) DHCP config object. */
        NMActiveConnection* conn6 = nm_device_get_active_connection(device);
        if (conn6) {
            NMDhcpConfig* dhcp6 = nm_active_connection_get_dhcp6_config(conn6);
            if (dhcp6) {
                g_signal_handlers_disconnect_by_func(dhcp6, (gpointer)dhcp6OptionsCb, device);
                g_signal_connect(dhcp6, "notify::options", G_CALLBACK(dhcp6OptionsCb), device);
            }
        }
        refreshIpFamilyCache(device, true);
    }

    static void deviceAddedCB(NMClient *client, NMDevice *device, NMEvents *nmEvents)
    {
        if( ((device != NULL) && NM_IS_DEVICE(device)) )
        {
            std::string ifname = nm_device_get_iface(device);
            if(ifname == nmUtils::wlanIface()) {
                GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ADDED, nmUtils::wlanIface());
                NMLOG_INFO("WIFI device added: %s", ifname.c_str());
            }
            else if(ifname == nmUtils::ethIface()) {
                GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ADDED, nmUtils::ethIface());
                NMLOG_INFO("ETHERNET device added: %s", ifname.c_str());
            }

            /* ip events added only for eth0 and wlan0 */
            if(ifname == nmUtils::ethIface() || ifname == nmUtils::wlanIface())
            {
                g_signal_connect(device, "notify::" NM_DEVICE_STATE, G_CALLBACK(GnomeNetworkManagerEvents::deviceStateChangeCb), nmEvents);
                g_signal_connect(device, "notify::ip4-config", G_CALLBACK(ip4ConfigChangedCb), nmEvents);
                g_signal_connect(device, "notify::ip6-config", G_CALLBACK(ip6ConfigChangedCb), nmEvents);
                NMIPConfig *ipv4Config = nm_device_get_ip4_config(device);
                NMIPConfig *ipv6Config = nm_device_get_ip6_config(device);
                if (ipv4Config) {
                    g_signal_connect(ipv4Config, "notify::addresses",   G_CALLBACK(ip4ChangedCb), device);
                    g_signal_connect(ipv4Config, "notify::gateway",     G_CALLBACK(ip4ChangedCb), device);
                    g_signal_connect(ipv4Config, "notify::nameservers", G_CALLBACK(ip4ChangedCb), device);
                }

                if (ipv6Config) {
                    g_signal_connect(ipv6Config, "notify::addresses",   G_CALLBACK(ip6ChangedCb), device);
                    g_signal_connect(ipv6Config, "notify::gateway",     G_CALLBACK(ip6ChangedCb), device);
                    g_signal_connect(ipv6Config, "notify::nameservers", G_CALLBACK(ip6ChangedCb), device);
                }

                /* Subscribe to DHCP option changes so dhcpserver stays current mid-lease. */
                NMActiveConnection* connAdded = nm_device_get_active_connection(device);
                if (connAdded) {
                    NMDhcpConfig* dhcp4Added = nm_active_connection_get_dhcp4_config(connAdded);
                    NMDhcpConfig* dhcp6Added = nm_active_connection_get_dhcp6_config(connAdded);
                    if (dhcp4Added)
                        g_signal_connect(dhcp4Added, "notify::options", G_CALLBACK(dhcp4OptionsCb), device);
                    if (dhcp6Added)
                        g_signal_connect(dhcp6Added, "notify::options", G_CALLBACK(dhcp6OptionsCb), device);
                }

                if (NM_IS_DEVICE_WIFI(device))
                {
                    // Register signal handler for WiFi scanning events to detect when scan operations complete
                    nmEvents->wifiDevice = NM_DEVICE_WIFI(device);
                    NMLOG_INFO("WIFI device added, adding signal handler for last-scan");
                    g_signal_connect(nmEvents->wifiDevice, "notify::" NM_DEVICE_WIFI_LAST_SCAN, G_CALLBACK(GnomeNetworkManagerEvents::onAvailableSSIDsCb), nmEvents);
                }
            }
        }
        else
            NMLOG_DEBUG("device error null");
    }

    static void deviceRemovedCB(NMClient *client, NMDevice *device, NMEvents *nmEvents)
    {
        if( ((device != NULL) && NM_IS_DEVICE(device)) )
        {
            std::string ifname = nm_device_get_iface(device);
            if(ifname == nmUtils::wlanIface()) {
                GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, nmUtils::wlanIface());
                g_signal_handlers_disconnect_by_func(device, (gpointer)GnomeNetworkManagerEvents::deviceStateChangeCb, nmEvents);
                NMLOG_INFO("WIFI device removed: %s", ifname.c_str());
            }
            else if(ifname == nmUtils::ethIface()) {
                GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, nmUtils::ethIface());
                g_signal_handlers_disconnect_by_func(device, (gpointer)GnomeNetworkManagerEvents::deviceStateChangeCb, nmEvents);
                NMLOG_INFO("ETHERNET device removed: %s", ifname.c_str());
            }
        }

        //     guint disconnected_count = g_signal_handlers_disconnect_matched( _nmEventInstance->activeConn,
        //                                                                     G_SIGNAL_MATCH_FUNC,
        //                                                                     0, 0, NULL,
        //                                                                     (gpointer)onActiveConnectionStateChanged,
        //                                                                     NULL );
        //     NMLOG_ERROR("Disconnected %u signal handlers\n", disconnected_count);
    }

    static void clientStateChangedCb (NMClient *client, GParamSpec *pspec, gpointer user_data)
    {

        switch (nm_client_get_state (client)) {
        case NM_STATE_DISCONNECTED:
            NMLOG_WARNING("internet connection down");
            break;
        case NM_STATE_CONNECTED_GLOBAL:
            NMLOG_DEBUG("global internet connection success");
            break;
        default:
            break;
	    }
    }

    static void managerRunningCb (NMClient *client, GParamSpec *pspec, gpointer user_data)
    {
        if (nm_client_get_nm_running (client)) {
            NMLOG_INFO("network manager daemon is running");
        } else {
            NMLOG_FATAL("network manager daemon not running !");
            // TODO  check need any client reconnection or not ?
        }
    }

    void* GnomeNetworkManagerEvents::networkMangerEventMonitor(void *arg)
    {
        if(arg == nullptr)
        {
            NMLOG_FATAL("function argument error: nm event monitor failed");
            return nullptr;
        }

        NMEvents *nmEvents = static_cast<NMEvents *>(arg);
        primaryConnectionCb(nmEvents->client, NULL, nmEvents);
        g_signal_connect (nmEvents->client, "notify::" NM_CLIENT_NM_RUNNING,G_CALLBACK (managerRunningCb), nmEvents);
        g_signal_connect(nmEvents->client, "notify::" NM_CLIENT_STATE, G_CALLBACK (clientStateChangedCb),nmEvents);
        g_signal_connect(nmEvents->client, "notify::" NM_CLIENT_PRIMARY_CONNECTION, G_CALLBACK(primaryConnectionCb), nmEvents);

        const GPtrArray *devices = nullptr;
        devices = nm_client_get_devices(nmEvents->client);

        g_signal_connect(nmEvents->client, NM_CLIENT_DEVICE_ADDED, G_CALLBACK(deviceAddedCB), nmEvents);
        g_signal_connect(nmEvents->client, NM_CLIENT_DEVICE_REMOVED, G_CALLBACK(deviceRemovedCB), nmEvents);

        if(devices == nullptr)
        {
            NMLOG_ERROR("Failed to get device list.");
            return nullptr;
        }

        for (u_int count = 0; count < devices->len; count++)
        {
            NMDevice *device = NM_DEVICE(g_ptr_array_index(devices, count));
            if( ((device != NULL) && NM_IS_DEVICE(device)) )
            {
                std::string ifname = nm_device_get_iface(device);
                if((ifname == nmUtils::ethIface()) || (ifname == nmUtils::wlanIface()))
                {
                    NMDeviceState devState =  nm_device_get_state(device);

                    if(devState > NM_DEVICE_STATE_DISCONNECTED && devState <= NM_DEVICE_STATE_ACTIVATED)
                    {
                        // posting device state change event if interface already connected
                        GnomeNetworkManagerEvents::deviceStateChangeCb(device, nullptr, nullptr);
                    }

                    /* Register device state change event */
                    g_signal_connect(device, "notify::" NM_DEVICE_STATE, G_CALLBACK(GnomeNetworkManagerEvents::deviceStateChangeCb), nmEvents);
                    g_signal_connect(device, "notify::ip4-config", G_CALLBACK(ip4ConfigChangedCb), nmEvents);
                    g_signal_connect(device, "notify::ip6-config", G_CALLBACK(ip6ConfigChangedCb), nmEvents);
                    if(NM_IS_DEVICE_WIFI(device)) {
                        nmEvents->wifiDevice = NM_DEVICE_WIFI(device);
                        g_signal_connect(nmEvents->wifiDevice, "notify::" NM_DEVICE_WIFI_LAST_SCAN, G_CALLBACK(GnomeNetworkManagerEvents::onAvailableSSIDsCb), nmEvents);
                    }

                    NMIPConfig *ipv4Config = nm_device_get_ip4_config(device);
                    NMIPConfig *ipv6Config = nm_device_get_ip6_config(device);
                    if (ipv4Config) {
                        g_signal_connect(ipv4Config, "notify::addresses",   G_CALLBACK(ip4ChangedCb), device);
                        g_signal_connect(ipv4Config, "notify::gateway",     G_CALLBACK(ip4ChangedCb), device);
                        g_signal_connect(ipv4Config, "notify::nameservers", G_CALLBACK(ip4ChangedCb), device);
                    }
                    else
                        NMLOG_WARNING("IPv4 config is null for device: %s, No IPv4 monitor", ifname.c_str());

                    if (ipv6Config) {
                        g_signal_connect(ipv6Config, "notify::addresses",   G_CALLBACK(ip6ChangedCb), device);
                        g_signal_connect(ipv6Config, "notify::gateway",     G_CALLBACK(ip6ChangedCb), device);
                        g_signal_connect(ipv6Config, "notify::nameservers", G_CALLBACK(ip6ChangedCb), device);
                    }
                    else
                        NMLOG_WARNING("IPv6 config is null for device: %s, No IPv6 monitor", ifname.c_str());

                    /* Subscribe to DHCP option changes so dhcpserver stays current mid-lease. */
                    NMActiveConnection* connInit = nm_device_get_active_connection(device);
                    if (connInit) {
                        NMDhcpConfig* dhcp4Init = nm_active_connection_get_dhcp4_config(connInit);
                        NMDhcpConfig* dhcp6Init = nm_active_connection_get_dhcp6_config(connInit);
                        if (dhcp4Init)
                            g_signal_connect(dhcp4Init, "notify::options", G_CALLBACK(dhcp4OptionsCb), device);
                        if (dhcp6Init)
                            g_signal_connect(dhcp6Init, "notify::options", G_CALLBACK(dhcp6OptionsCb), device);
                    }

                    /* Seed the IP cache from current state for already-connected devices. */
                    refreshIpFamilyCache(device, false);
                    refreshIpFamilyCache(device, true);
                }
                else
                    NMLOG_DEBUG("device type not eth/wifi %s", ifname.c_str());
            }
            else
                NMLOG_WARNING("device error null");
        }

        NMLOG_INFO("registered all networkmnager dbus events");
        g_main_loop_run(nmEvents->loop);
        // Clean up all signal handlers after thread has stopped
        if(_nmEventInstance != nullptr)
        {
            _nmEventInstance->cleanupSignalHandlers();
        }
        //g_main_loop_unref(nmEvents->loop);
        return nullptr;
    }

    bool GnomeNetworkManagerEvents::startNetworkMangerEventMonitor()
    {
        NMLOG_DEBUG("starting gnome event monitor");
        if (NULL == nmEvents.client) {
            NMLOG_ERROR("Client Connection NULL DBUS event Failed!");
            return false;
        }
        if(!isEventThrdActive) {
            isEventThrdActive = true;
            // Create event monitor thread
            eventThrdID = g_thread_new("nm_event_thrd", GnomeNetworkManagerEvents::networkMangerEventMonitor, &nmEvents);
        }
        return true;
    }

    void GnomeNetworkManagerEvents::stopNetworkMangerEventMonitor()
    {
        // g_signal_handlers_disconnect_by_func(client, G_CALLBACK(primaryConnectionCb), NULL);
        if (!isEventThrdActive) {
            return;
        }
        if (nmEvents.loop != NULL) {
            g_main_loop_quit(nmEvents.loop);
        }

        if (eventThrdID) {
            g_thread_join(eventThrdID);  // Wait for the thread to finish
            eventThrdID = NULL;  // Reset the thread ID
            NMLOG_WARNING("gnome event monitor stopped");
        }
        isEventThrdActive = false;

    }

    void GnomeNetworkManagerEvents::cleanupSignalHandlers()
    {
        if(nmEvents.client == nullptr) {
            return; // Already cleaned up
        }

        NMLOG_DEBUG("Cleaning up signal handlers");

        // Disconnect all client signals
        g_signal_handlers_disconnect_by_data(nmEvents.client, &nmEvents);

        // Clean up device signals
        const GPtrArray *devices = nm_client_get_devices(nmEvents.client);
        if (devices) {
            for (guint i = 0; i < devices->len; i++) {
                NMDevice *device = NM_DEVICE(g_ptr_array_index(devices, i));
                if (device && NM_IS_DEVICE(device)) {
                    g_signal_handlers_disconnect_by_data(device, &nmEvents);

                    // Clean up IP config signals
                    NMIPConfig *ipv4Config = nm_device_get_ip4_config(device);
                    NMIPConfig *ipv6Config = nm_device_get_ip6_config(device);
                    if (ipv4Config) {
                        g_signal_handlers_disconnect_by_func(ipv4Config, (gpointer)ip4ChangedCb, device);
                    }
                    if (ipv6Config) {
                        g_signal_handlers_disconnect_by_func(ipv6Config, (gpointer)ip6ChangedCb, device);
                    }
                }
            }
        }

        NMLOG_DEBUG("Signal handlers cleanup complete");
    }

    GnomeNetworkManagerEvents::~GnomeNetworkManagerEvents()
    {
        NMLOG_INFO("~GnomeNetworkManagerEvents");
        stopNetworkMangerEventMonitor();
        if(nmEvents.client != nullptr) {
            g_object_unref(nmEvents.client);
            nmEvents.client = nullptr;
        }
        if (nmEvents.loop != NULL) {
            g_main_loop_unref(nmEvents.loop);
            nmEvents.loop = NULL;
        }
    }

    GnomeNetworkManagerEvents* GnomeNetworkManagerEvents::getInstance()
    {
        static GnomeNetworkManagerEvents instance;
        return &instance;
    }

    GnomeNetworkManagerEvents::GnomeNetworkManagerEvents()
    {
        NMLOG_DEBUG("GnomeNetworkManagerEvents");
        GError *error = NULL;
        eventThrdID = nullptr;
        isEventThrdActive = false;
        doScanNotify = false;

        nmEvents.client = nm_client_new(NULL, &error);
        if(!nmEvents.client || error )
        {
            if (error) {
                NMLOG_ERROR("Could not connect to NetworkManager: %s", error->message);
                g_error_free(error);
            }
            NMLOG_INFO("networkmanger client connection failed");
            return;
        }

        NMLOG_INFO("libnm client connection success version: %s", nm_client_get_version(nmEvents.client));
        nmEvents.loop = g_main_loop_new(NULL, FALSE);
        if(nmEvents.loop == NULL) {
            NMLOG_FATAL("GMain loop failed Fatal Error: Event will not work");
            return;
        }
        _nmEventInstance = this;
    }

    /* Gnome networkmanger new events */

    void GnomeNetworkManagerEvents::onActiveInterfaceChangeCb(std::string newIface)
    {
        static std::string oldIface = "Unknown";

        if(oldIface != newIface)
        {
            if(_instance != nullptr)
                _instance->ReportActiveInterfaceChange(oldIface, newIface);
            NMLOG_INFO("old interface - %s new interface - %s", oldIface.c_str(), newIface.c_str());
            oldIface = newIface;
        }
    }

    void GnomeNetworkManagerEvents::onInterfaceStateChangeCb(uint8_t newState, std::string iface)
    {
        std::string state {};
        switch (newState)
        {
            case Exchange::INetworkManager::INTERFACE_ADDED:
                state = "INTERFACE_ADDED";
                break;
            case Exchange::INetworkManager::INTERFACE_LINK_UP:
                state = "INTERFACE_LINK_UP";
                break;
            case Exchange::INetworkManager::INTERFACE_LINK_DOWN:
                state = "INTERFACE_LINK_DOWN";
                break;
            case Exchange::INetworkManager::INTERFACE_ACQUIRING_IP:
                state = "INTERFACE_ACQUIRING_IP";
                break;
            case Exchange::INetworkManager::INTERFACE_REMOVED:
                state = "INTERFACE_REMOVED";
                break;
            case Exchange::INetworkManager::INTERFACE_DISABLED:
                state = "INTERFACE_DISABLED";
                break;
            default:
                state = "Unknown";
        }

        NMLOG_DEBUG("%s interface state changed - %s", iface.c_str(), state.c_str());
        if(_instance != nullptr && (iface == nmUtils::wlanIface() || iface == nmUtils::ethIface()))
            _instance->ReportInterfaceStateChange(static_cast<Exchange::INetworkManager::InterfaceState>(newState), iface);
    }

    void GnomeNetworkManagerEvents::onWIFIStateChanged(uint8_t state)
    {
        if(_instance != nullptr)
        {
            _instance->ReportWiFiStateChange(static_cast<Exchange::INetworkManager::WiFiState>(state));
#ifdef ENABLE_MIGRATION_MFRMGR_SUPPORT
            // Handle WiFi state changes for MfrMgr integration
            NetworkManagerMfrManager* mfrManager = NetworkManagerMfrManager::getInstance();
            if(mfrManager != nullptr) {
                if(state == Exchange::INetworkManager::WIFI_STATE_CONNECTED)
                {
                    NMLOG_DEBUG("WiFi connected - triggering MfrMgr save");
                    mfrManager->saveWiFiSettingsToMfr();
                }
            }
#endif
        }
    }

    bool GnomeNetworkManagerEvents::apToJsonObject(NMAccessPoint *ap, JsonObject& ssidObj)
    {
         GBytes *ssid = NULL;
         int strength = 0;
         double freq;
         int security;
         guint32 flags, wpaFlags, rsnFlags, apFreq;
         if(ap == nullptr)
             return false;
         ssid = nm_access_point_get_ssid(ap);
         if (ssid)
         {
             char *ssidStr = nullptr;
             ssidStr = nm_utils_ssid_to_utf8((const guint8*)g_bytes_get_data(ssid, NULL), g_bytes_get_size(ssid));
             string ssidString = "---";
             if(ssidStr)
             {
                ssidString = ssidStr;
                free(ssidStr);
             }
             ssidObj["ssid"] = ssidString;
             const char *bssidPtr = nm_access_point_get_bssid(ap);
             std::string bssid = bssidPtr ? bssidPtr : "";
             if (!bssid.empty()) {
                 ssidObj["bssid"] = bssid;
             }
             else
             {
                NMLOG_WARNING("BSSID is null for SSID: %s", ssidString.c_str());
                ssidObj["bssid"] = "";
             }
             strength = nm_access_point_get_strength(ap);
             apFreq   = nm_access_point_get_frequency(ap);
             flags    = nm_access_point_get_flags(ap);
             wpaFlags = nm_access_point_get_wpa_flags(ap);
             rsnFlags = nm_access_point_get_rsn_flags(ap);
             freq = nmUtils::wifiFrequencyFromAp(apFreq);
             security = nmUtils::wifiSecurityModeFromAp(ssidString, flags, wpaFlags, rsnFlags, false);

             ssidObj["security"] = security;
             ssidObj["strength"] = nmUtils::convertPercentageToSignalStrength(strength);
             ssidObj["frequency"] = freq;
             return true;
         }
         // else
         //     NMLOG_DEBUG("hidden ssid found, bssid: %s", nm_access_point_get_bssid(ap));
         return false;
    }

    void GnomeNetworkManagerEvents::onAvailableSSIDsCb(NMDeviceWifi *wifiDevice, GParamSpec *pspec, gpointer userData)
    {
        if(!NM_IS_DEVICE_WIFI(wifiDevice))
        {
            NMLOG_ERROR("Not a wifi object ");
            return;
        }

        const GPtrArray *accessPoints = nm_device_wifi_get_access_points(wifiDevice);

        if (accessPoints == nullptr) {
            NMLOG_ERROR("scanning result No access points found !");
            _nmEventInstance->doScanNotify = false;
            return;
        }

        if(!_nmEventInstance->doScanNotify)
        {
            NMLOG_DEBUG("scan result received; notify disabled, skipping");
            return;
        }
        
        NMLOG_INFO("No of AP Available = %d", static_cast<int>(accessPoints->len));

        JsonArray ssidList = JsonArray();
        for (guint i = 0; i < accessPoints->len; i++)
        {
            JsonObject ssidObj{};
            NMAccessPoint *ap = static_cast<NMAccessPoint*>(accessPoints->pdata[i]);
            if(GnomeNetworkManagerEvents::apToJsonObject(ap, ssidObj))
                ssidList.Add(ssidObj);
        }

        if(_instance != nullptr) {
            _nmEventInstance->doScanNotify = false;
            _instance->ReportAvailableSSIDs(ssidList);
        }
    }

    void GnomeNetworkManagerEvents::setwifiScanOptions(bool doNotify)
    {
        doScanNotify.store(doNotify);
        if(!doScanNotify)
        {
            NMLOG_DEBUG("stop periodic wifi scan result notify");
        }
    }

    }   // Plugin
}   // WPEFramework

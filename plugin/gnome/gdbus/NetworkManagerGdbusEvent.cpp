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


#include <libnm/nm-dbus-interface.h>
#include <gio/gio.h>
#include <thread>
#include <string>
#include <map>

#include "NetworkManagerGdbusEvent.h"
#include "NetworkManagerGdbusUtils.h"
#include "NetworkManagerImplementation.h"
#include "NetworkManagerLogger.h"
#include "INetworkManager.h"

namespace WPEFramework
{
    namespace Plugin
    {

    static NetworkManagerEvents *_NetworkManagerEvents = nullptr;
    extern NetworkManagerImplementation* _instance;

    static std::string getInterafaceNameFormDevicePath(const char* DevicePath)
    {
        GDBusProxy *deviceProxy = NULL;
        std::string ifname;
        deviceProxy =_NetworkManagerEvents->eventDbus.getNetworkManagerDeviceProxy(DevicePath);
        if (deviceProxy == NULL)
            return ifname;
        GVariant *ifaceVariant = g_dbus_proxy_get_cached_property(deviceProxy, "Interface");
        if (ifaceVariant == NULL) {
            NMLOG_WARNING("Interface property get error");
            g_object_unref(deviceProxy);
            return ifname;
        }

        const gchar *ifaceName = g_variant_get_string(ifaceVariant, NULL);
        if(ifaceName != nullptr)
            ifname = ifaceName;

        g_variant_unref(ifaceVariant);
        g_object_unref(deviceProxy);
        return ifname;
    }

    static void lastScanPropertiesChangedCB(GDBusProxy *proxy, GVariant *changedProps,
                                                const gchar *invalidProps[], gpointer userData) {

        if (proxy == NULL) {
            NMLOG_FATAL("cb doesn't have proxy ");
            return;
        }

        GVariant *lastScanVariant = g_variant_lookup_value(changedProps, "LastScan", NULL);
        if (lastScanVariant == NULL)
            return;

        gint64 lastScanTime = g_variant_get_int64(lastScanVariant);
        NMLOG_DEBUG("LastScan value changed: %" G_GINT64_FORMAT, lastScanTime);
        g_variant_unref(lastScanVariant);

        const gchar *objectPath = g_dbus_proxy_get_object_path(proxy);
        if (objectPath == NULL) {
            NMLOG_WARNING("Failed to get the proxy object path");
            return;
        }

        NetworkManagerEvents::onAvailableSSIDsCb(objectPath);
    }

    static bool subscribeForlastScanPropertyEvent(const gchar *wirelessPath)
    {
        // Todo clean the proxy first
        NMLOG_DEBUG("Monitoring lastScan time property: %s", wirelessPath);
        _NetworkManagerEvents->nmEvents.wirelessProxy = _NetworkManagerEvents->eventDbus.getNetworkManagerWirelessProxy(wirelessPath);
        if (_NetworkManagerEvents->nmEvents.wirelessProxy == nullptr) {
            return false;
        }
        g_signal_connect(_NetworkManagerEvents->nmEvents.wirelessProxy, "g-properties-changed", G_CALLBACK(lastScanPropertiesChangedCB), NULL);
        return true;
    }

    static void primaryConnectionChangedCB(GDBusProxy *proxy, GVariant *changedPropes, GStrv invalidatPropes, gpointer userData)
    {
        if (changedPropes == NULL) {
            NMLOG_FATAL("cb doesn't have changed_properties ");
            return;
        }

        GVariant *primaryConnPathVariant = g_variant_lookup_value(changedPropes, "PrimaryConnection", NULL);
        if (primaryConnPathVariant != NULL)
        {
            const gchar *primaryConnPath = NULL;
            std::string newIface = "";
            primaryConnPath = g_variant_get_string(primaryConnPathVariant, NULL);
            if(primaryConnPath && g_strcmp0(primaryConnPath, "/") == 0)
            {
                NMLOG_DEBUG("no active primary connection");
                // sending empty string if no active interface
                NetworkManagerEvents::onActiveInterfaceChangeCb(std::string("Unknown"));
                g_variant_unref(primaryConnPathVariant);
                return;
            }
            else if(primaryConnPath)
            {
                GDBusProxy *activeProxy = NULL;
                NMLOG_DEBUG("PrimaryConnection changed: %s", primaryConnPath);
                activeProxy =_NetworkManagerEvents->eventDbus.getNetworkManagerActiveConnProxy(primaryConnPath);
                if(activeProxy != NULL)
                {
                    GVariant *typeVariant = g_dbus_proxy_get_cached_property(activeProxy, "Type");
                    if (typeVariant != NULL)
                    {
                        const gchar *connectionType = g_variant_get_string(typeVariant, NULL);
                        if(connectionType != NULL) {
                            if (0 == strncmp("802-3-ethernet", connectionType, sizeof("802-3-ethernet")) && string("eth0_missing") != GnomeUtils::getEthIfname())
                                NetworkManagerEvents::onActiveInterfaceChangeCb(std::string(GnomeUtils::getEthIfname()));
                            else if(0 == strncmp("802-11-wireless", connectionType, sizeof("802-11-wireless")) && string("wlan0_missing") != GnomeUtils::getWifiIfname())
                                NetworkManagerEvents::onActiveInterfaceChangeCb(std::string(GnomeUtils::getWifiIfname()));
                            else
                                NMLOG_WARNING("active connection not an ethernet/wifi %s", connectionType);
                        }
                        else
                            NMLOG_WARNING("connection type NULL");
                        g_variant_unref(typeVariant);
                    }
                    else
                        NMLOG_ERROR("PrimaryConnection Type property missing");
                    g_object_unref(activeProxy);
                }
            }
            else
                NMLOG_WARNING("PrimaryConnection object missing");
            g_variant_unref(primaryConnPathVariant);
        }
    }

    static void deviceAddRemoveCb(GDBusProxy *proxy, gchar *senderName, gchar *signalName,
                                                        GVariant *parameters, gpointer userData) {

        if (signalName == NULL) {
            NMLOG_FATAL("cb doesn't have signalName ");
            return;
        }

        const gchar *devicePath = NULL;
        std::string ifname;
        if (g_strcmp0(signalName, "DeviceAdded") == 0 || g_strcmp0(signalName, "DeviceRemoved") == 0)
        {
            if (g_variant_is_of_type(parameters, G_VARIANT_TYPE_TUPLE)) {
                GVariant *first_element = g_variant_get_child_value(parameters, 0);

                if (g_variant_is_of_type(first_element, G_VARIANT_TYPE_OBJECT_PATH)) {
                    devicePath = g_variant_get_string(first_element, NULL);
                    ifname = getInterafaceNameFormDevicePath(devicePath);
                } else {
                    NMLOG_ERROR("varient type not object path %s", g_variant_get_type_string(first_element));
                }
                g_variant_unref(first_element);
            }
            if (g_strcmp0(signalName, "DeviceAdded") == 0) {
                NMLOG_INFO("Device Added: %s", ifname.c_str());
                if(ifname == GnomeUtils::getWifiIfname() || ifname == GnomeUtils::getEthIfname() )
                {
                    NMLOG_INFO("monitoring device: %s", ifname.c_str());
                    NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ADDED, ifname.c_str());
                    // Only subscribe to LastScan if it's a WiFi device and not already monitored
                    if(ifname == GnomeUtils::getWifiIfname() && _NetworkManagerEvents->nmEvents.wirelessProxy == nullptr)
                    {
                        subscribeForlastScanPropertyEvent(devicePath);
                    }
                    // TODO monitor device 
                    // monitorDevice()
                }
            }
            else if(g_strcmp0(signalName, "DeviceRemoved")) {
                NMLOG_INFO("Device Removed: %s", ifname.c_str());
                if(ifname == GnomeUtils::getWifiIfname() || ifname == GnomeUtils::getEthIfname() )
                {
                    NMLOG_INFO("monitoring device: %s", ifname.c_str());
                    NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, ifname.c_str());
                }
            }
        }
    }

    static void deviceStateChangedCB(GDBusProxy *proxy, gchar *senderName, gchar *signalName,
                                                        GVariant *parameters, gpointer userData) {

        if (signalName == NULL || g_strcmp0(signalName, "StateChanged") != 0) {
            return;
        }

        static bool doPostEthUpEvent= false;
        static bool doPostWlanUpEvent= false;

        const gchar *devicePath = g_dbus_proxy_get_object_path(proxy);
        std::string ifname = getInterafaceNameFormDevicePath(devicePath);
        if(ifname.empty())
        {
            NMLOG_WARNING("interface name null");
            return;
        }
        guint32 oldState, newState, stateReason;
        g_variant_get(parameters, "(uuu)", &newState, &oldState, &stateReason);
        NMDeviceState deviceState = static_cast<NMDeviceState>(newState);
        NMDeviceStateReason deviceStateReason = static_cast<NMDeviceStateReason>(stateReason);

        NMLOG_DEBUG("%s: Old State: %u, New State: %u, Reason: %u", ifname.c_str(), oldState, newState, stateReason);
        if(ifname == GnomeUtils::getWifiIfname())
        {
            std::string wifiState;
            switch (deviceStateReason)
            {
                case NM_DEVICE_STATE_REASON_SUPPLICANT_AVAILABLE:
                    wifiState = "WIFI_STATE_UNINSTALLED";
                    NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_UNINSTALLED, wifiState);
                    break;
                case NM_DEVICE_STATE_REASON_SSID_NOT_FOUND:
                    wifiState = "WIFI_STATE_SSID_NOT_FOUND";
                    NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND, wifiState);
                    break;
                case NM_DEVICE_STATE_REASON_SUPPLICANT_TIMEOUT:         // supplicant took too long to authenticate
                case NM_DEVICE_STATE_REASON_NO_SECRETS:
                    wifiState = "WIFI_STATE_AUTHENTICATION_FAILED";
                    NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_AUTHENTICATION_FAILED, wifiState);
                    break;
                case NM_DEVICE_STATE_REASON_SUPPLICANT_FAILED:          //  802.1x supplicant failed
                    wifiState = "WIFI_STATE_ERROR";
                    NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_ERROR, wifiState);
                    break;
                case NM_DEVICE_STATE_REASON_SUPPLICANT_CONFIG_FAILED:   // 802.1x supplicant configuration failed
                    wifiState = "WIFI_STATE_CONNECTION_INTERRUPTED";
                    NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTION_INTERRUPTED, wifiState);
                    break;
                case NM_DEVICE_STATE_REASON_SUPPLICANT_DISCONNECT:      // 802.1x supplicant disconnected
                    wifiState = "WIFI_STATE_INVALID_CREDENTIALS";
                    NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_INVALID_CREDENTIALS, wifiState);
                    break;
                default:
                {
                    switch (deviceState)
                    {
                    case NM_DEVICE_STATE_UNKNOWN:
                        wifiState = "WIFI_STATE_UNINSTALLED";
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_UNINSTALLED, wifiState);
                        NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, GnomeUtils::getWifiIfname());
                        doPostWlanUpEvent= true;
                        break;
                    case NM_DEVICE_STATE_UNMANAGED:
                        wifiState = "WIFI_STATE_DISABLED";
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_DISABLED, wifiState);
                        NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, GnomeUtils::getWifiIfname());
                        doPostWlanUpEvent= true;
                        break;
                    case NM_DEVICE_STATE_UNAVAILABLE:
                    case NM_DEVICE_STATE_DISCONNECTED:
                        wifiState = "WIFI_STATE_DISCONNECTED";
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_DISCONNECTED, wifiState);
                        NetworkManagerEvents::onAddressChangeCb(GnomeUtils::getWifiIfname(), false, false);
                        NetworkManagerEvents::onAddressChangeCb(GnomeUtils::getWifiIfname(), false, true);
                        break;
                    case NM_DEVICE_STATE_PREPARE:
                        wifiState = "WIFI_STATE_PAIRING";
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_PAIRING, wifiState);
                        break;
                    case NM_DEVICE_STATE_CONFIG:
                        wifiState = "WIFI_STATE_CONNECTING";
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTING, wifiState);
                        break;
                    case NM_DEVICE_STATE_IP_CONFIG:
                        wifiState = "NM_DEVICE_STATE_IP_CONFIG";
                        NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_UP, GnomeUtils::getWifiIfname());
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTED, wifiState);
                        break;
                    case NM_DEVICE_STATE_IP_CHECK:
                        wifiState = "NM_DEVICE_STATE_IP_CHECK";
                        //NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ACQUIRING_IP, GnomeUtils::getWifiIfname());
                        break;
                    case NM_DEVICE_STATE_SECONDARIES:
                        wifiState = "NM_DEVICE_STATE_SECONDARIES";
                        NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ACQUIRING_IP, GnomeUtils::getWifiIfname());
                        break;
                    case NM_DEVICE_STATE_ACTIVATED:
                        wifiState = "WIFI_STATE_CONNECTED";
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTED, wifiState);
                        break;
                    case NM_DEVICE_STATE_DEACTIVATING:
                        wifiState = "WIFI_STATE_CONNECTION_LOST";
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTION_LOST, wifiState);
                        break;
                    case NM_DEVICE_STATE_FAILED:
                        wifiState = "WIFI_STATE_CONNECTION_FAILED";
                        NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED, wifiState);
                        break;
                    case NM_DEVICE_STATE_NEED_AUTH:
                        //NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WIFI_STATE_CONNECTION_INTERRUPTED);
                        wifiState = "WIFI_STATE_CONNECTION_INTERRUPTED";
                        break;
                    default:
                        wifiState = "Un handiled";
                    }

                    if(doPostWlanUpEvent && deviceState > NM_DEVICE_STATE_UNMANAGED)
                    {
                        doPostWlanUpEvent = false;
                        NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ADDED, GnomeUtils::getWifiIfname());
                    }
                }
            }
            NMLOG_INFO("wifi state: %s; reason: %d", wifiState.c_str(), static_cast<int>(deviceStateReason));
        } 
        else if(ifname == GnomeUtils::getEthIfname())
        {
            switch (deviceState)
            {
                case NM_DEVICE_STATE_UNKNOWN:
                case NM_DEVICE_STATE_UNMANAGED:
                    NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_DISABLED, GnomeUtils::getEthIfname());
                    doPostEthUpEvent= true;
                break;
                case NM_DEVICE_STATE_UNAVAILABLE:
                case NM_DEVICE_STATE_DISCONNECTED:
                    NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_DOWN,  GnomeUtils::getEthIfname());
                    NetworkManagerEvents::onAddressChangeCb(GnomeUtils::getEthIfname(), false, false);
                    NetworkManagerEvents::onAddressChangeCb(GnomeUtils::getEthIfname(), false, true);
                break;
                case NM_DEVICE_STATE_PREPARE:
                    NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_UP,  GnomeUtils::getEthIfname());
                break;
                case NM_DEVICE_STATE_IP_CONFIG:
                    NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ACQUIRING_IP, GnomeUtils::getEthIfname());
                case NM_DEVICE_STATE_NEED_AUTH:
                case NM_DEVICE_STATE_SECONDARIES:
                case NM_DEVICE_STATE_ACTIVATED:
                case NM_DEVICE_STATE_DEACTIVATING:
                default:
                    NMLOG_WARNING("Unhandiled state change eth0");

                if(doPostEthUpEvent && deviceState > NM_DEVICE_STATE_UNMANAGED)
                {
                    doPostEthUpEvent = false;
                    NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ADDED, GnomeUtils::getEthIfname());
                }
            }

            NMLOG_INFO("eth0 state: %d; reason: %d", static_cast<int>(deviceState), static_cast<int>(deviceStateReason));
        }
    }

    static void onConnectionSignalReceivedCB (GDBusProxy *proxy, gchar *senderName, gchar *signalName,
                                        GVariant *parameters, gpointer userData) {
        if (g_strcmp0(senderName, "NewConnection") == 0) {
            NMLOG_INFO("new connection added success");
            NMLOG_DEBUG("Parameters: %s", g_variant_print(parameters, TRUE));
        } else if (g_strcmp0(signalName, "ConnectionRemoved") == 0) {
            NMLOG_INFO("connection remove success");
            NMLOG_DEBUG("Parameters: %s", g_variant_print(parameters, TRUE));
        }
    }

    static void ipV4addressChangeCb(GDBusProxy *proxy, GVariant *changedProps, GStrv invalidProps, gpointer userData)
    {
        if (changedProps == NULL || proxy == NULL) {
            NMLOG_FATAL("cb doesn't have changed properties ");
            return;
        }

        GVariant *addressVariant = g_variant_lookup_value(changedProps, "Addresses", NULL);
        if (addressVariant != NULL)
        {
            const char* iface = static_cast<char*>(userData);
            if(iface != NULL )
            {
                std::string ipAddr; uint32_t prifix;
                if(GnomeUtils::getIPv4AddrFromIPv4ConfigProxy(proxy, ipAddr, prifix))
                    NetworkManagerEvents::onAddressChangeCb(std::string(iface), true, false, ipAddr);
            }
            g_variant_unref(addressVariant);
        }
    }

    static void ipV6addressChangeCb(GDBusProxy *proxy, GVariant *changedProps, GStrv invalidProps, gpointer userData)
    {
        if (changedProps == NULL || proxy == NULL) {
            NMLOG_FATAL("cb doesn't have changed properties ");
            return;
        }

        GVariant *addressVariant = g_variant_lookup_value(changedProps, "Addresses", NULL);
        if (addressVariant != NULL)
        {
            const char* iface = static_cast<char*>(userData);
            if(iface != NULL)
            {
                std::string ipAddr; uint32_t prifix;
                if(GnomeUtils::getIPv6AddrFromIPv6ConfigProxy(proxy, ipAddr, prifix))
                    NetworkManagerEvents::onAddressChangeCb(std::string(iface), true, true, ipAddr);
            }

            g_variant_unref(addressVariant);
        }
        else
        {
            GVariantIter *propsIter= NULL;
            const gchar *propertyName= NULL;
            GVariant *propertyValue= NULL;

            // Iterate over all changed properties
            g_variant_get(changedProps, "a{sv}", &propsIter);
            while (g_variant_iter_loop(propsIter, "{&sv}", &propertyName, &propertyValue)) {
                NMLOG_DEBUG("Other Property: %s", propertyName);
                // if(propertyValue)
                //     g_variant_unref(changedProps);
            }
            g_variant_iter_free(propsIter);
        }
    }

    static void monitorDevice(const gchar *devicePath) 
    {
        std::string ifname = getInterafaceNameFormDevicePath(devicePath);
        gpointer userdata = NULL;
        if(ifname.empty())
        {
            NMLOG_WARNING("interface is missing for the device(%s)", devicePath);
            return;
        }

        if(ifname != GnomeUtils::getEthIfname() && ifname != GnomeUtils::getWifiIfname())
        {
            NMLOG_DEBUG("interface is not eth0/wlan0: %s", ifname.c_str());
            return;
        }

        GDBusProxy *deviceProxy =_NetworkManagerEvents->eventDbus.getNetworkManagerDeviceProxy(devicePath);
        if(deviceProxy == nullptr)
            return;

        g_signal_connect(deviceProxy, "g-signal", G_CALLBACK(deviceStateChangedCB), NULL);
        NMLOG_DEBUG("Monitoring device: %s", devicePath);
        if(ifname == GnomeUtils::getWifiIfname())
        {
            _NetworkManagerEvents->nmEvents.wirelessDeviceProxy = deviceProxy;
            _NetworkManagerEvents->nmEvents.wifiDevicePath = devicePath;
            userdata = _NetworkManagerEvents->wlanIfname; // TODO change to GnomeUtils::getWifiIfname()
            subscribeForlastScanPropertyEvent(devicePath);
        }
        else if(ifname == GnomeUtils::getEthIfname())
        {
            _NetworkManagerEvents->nmEvents.wiredDeviceProxy = deviceProxy;
            _NetworkManagerEvents->nmEvents.ethDevicePath = devicePath;
            userdata = _NetworkManagerEvents->ethIfname;
        }

        const gchar *ipv4ConfigPath = NULL;
        const gchar *ipv6ConfigPath = NULL;
        GVariant *ip4Config = g_dbus_proxy_get_cached_property(deviceProxy, "Ip4Config");
        if (ip4Config)
        {
            ipv4ConfigPath = g_variant_get_string(ip4Config, NULL);
            NMLOG_DEBUG("Monitoring ip4_config_path: %s", ipv4ConfigPath);
        }

        GVariant *ip6Config = g_dbus_proxy_get_cached_property(deviceProxy, "Ip6Config");
        if(ip6Config)
        {
            ipv6ConfigPath = g_variant_get_string(ip6Config, NULL);
            NMLOG_DEBUG("Monitoring ip6_config_path: %s", ipv6ConfigPath);
        }

        if(ipv4ConfigPath)
        {
            GDBusProxy *ipV4Proxy = _NetworkManagerEvents->eventDbus.getNetworkManagerIpv4Proxy(ipv4ConfigPath);
            g_signal_connect(ipV4Proxy, "g-properties-changed", G_CALLBACK(ipV4addressChangeCb), userdata);
            if(ifname == GnomeUtils::getEthIfname())
                _NetworkManagerEvents->nmEvents.ethIPv4Proxy = ipV4Proxy;
            else if(ifname == GnomeUtils::getWifiIfname())
                _NetworkManagerEvents->nmEvents.wlanIPv4Proxy = ipV4Proxy;
        }

        if(ipv6ConfigPath)
        {
            GDBusProxy *ipV6Proxy = _NetworkManagerEvents->eventDbus.getNetworkManagerIpv6Proxy(ipv6ConfigPath);
            g_signal_connect(ipV6Proxy, "g-properties-changed", G_CALLBACK(ipV6addressChangeCb), userdata);
            if(ifname == GnomeUtils::getEthIfname())
                _NetworkManagerEvents->nmEvents.ethIPv6Proxy = ipV6Proxy;
            else if(ifname == GnomeUtils::getWifiIfname())
                _NetworkManagerEvents->nmEvents.wlanIPv6Proxy = ipV6Proxy;
        }

        if(ip4Config)
            g_variant_unref(ip4Config);
        if(ip6Config)
            g_variant_unref(ip6Config);
    }

    void* NetworkManagerEvents::networkMangerEventMonitor(void *arg)
    {
        if(arg == NULL)
        {
            NMLOG_FATAL("function argument error: nm event monitor failed");
            return nullptr;
        }

        NMEvents *nmEvents = static_cast<NMEvents *>(arg);
        nmEvents->networkManagerProxy = _NetworkManagerEvents->eventDbus.getNetworkManagerProxy();
        if (nmEvents->networkManagerProxy == NULL) {
            return nullptr;
        }

        g_signal_connect(nmEvents->networkManagerProxy, "g-signal", G_CALLBACK(deviceAddRemoveCb), NULL);
        g_signal_connect(nmEvents->networkManagerProxy, "g-properties-changed", G_CALLBACK(primaryConnectionChangedCB), NULL);

        GVariant *devices = g_dbus_proxy_get_cached_property(nmEvents->networkManagerProxy, "Devices");
        if (devices != NULL) {
            GVariantIter iter;
            gchar *devicePath = NULL;

            g_variant_iter_init(&iter, devices);
            while (g_variant_iter_loop(&iter, "&o", &devicePath)) {
                monitorDevice(devicePath);
            }

            g_variant_unref(devices);
        }

        nmEvents->settingsProxy = _NetworkManagerEvents->eventDbus.getNetworkManagerSettingsProxy();
        if (nmEvents->settingsProxy == NULL)
            NMLOG_WARNING("Settings proxy failed no connection event registerd");
        else
            g_signal_connect(nmEvents->settingsProxy, "g-signal", G_CALLBACK(onConnectionSignalReceivedCB), NULL);

        NMLOG_INFO("registered all networkmnager dbus events");
        g_main_loop_run(nmEvents->loop);

        if(nmEvents->wirelessDeviceProxy)
            g_object_unref(nmEvents->wirelessDeviceProxy);
        if(nmEvents->wirelessProxy)
            g_object_unref(nmEvents->wirelessProxy);
        if(nmEvents->wiredDeviceProxy)
            g_object_unref(nmEvents->wiredDeviceProxy);
        if(nmEvents->networkManagerProxy)
            g_object_unref(nmEvents->networkManagerProxy);
        if(nmEvents->settingsProxy)
            g_object_unref(nmEvents->settingsProxy);
        if(nmEvents->ethIPv4Proxy)
            g_object_unref(nmEvents->ethIPv4Proxy);
        if(nmEvents->ethIPv6Proxy)
            g_object_unref(nmEvents->ethIPv6Proxy);
        if(nmEvents->wlanIPv4Proxy)
            g_object_unref(nmEvents->wlanIPv4Proxy);
        if(nmEvents->wlanIPv6Proxy)
            g_object_unref(nmEvents->wlanIPv6Proxy);

         NMLOG_WARNING("unregistered all event monitor");
        return nullptr;
    }

    bool NetworkManagerEvents::startNetworkMangerEventMonitor()
    {
        NMLOG_DEBUG("starting gnome event monitor...");

        if(!isEventThrdActive) {
            isEventThrdActive = true;
            eventThrdID = g_thread_new("nm_event_thrd", NetworkManagerEvents::networkMangerEventMonitor, &nmEvents);
        }
        return true;
    }

    void NetworkManagerEvents::stopNetworkMangerEventMonitor()
    {

        if (nmEvents.loop != NULL) {
            g_main_loop_quit(nmEvents.loop);
        }
        if (eventThrdID) {
            g_thread_join(eventThrdID);
            eventThrdID = NULL;
            NMLOG_WARNING("gnome event monitor stoped");
        }
        isEventThrdActive = false;
    }

    NetworkManagerEvents::~NetworkManagerEvents()
    {
        NMLOG_INFO("~NetworkManagerEvents");
        stopNetworkMangerEventMonitor();
    }

    NetworkManagerEvents* NetworkManagerEvents::getInstance()
    {
        static NetworkManagerEvents instance;
        return &instance;
    }

    NetworkManagerEvents::NetworkManagerEvents()
    {
        NMLOG_DEBUG("NetworkManagerEvents");
        nmEvents.loop = g_main_loop_new(NULL, FALSE);
        if(nmEvents.loop == NULL) {
            NMLOG_FATAL("GMain loop failed Fatal Error: Event will not work");
            return;
        }
        strncpy(wlanIfname, GnomeUtils::getWifiIfname(), sizeof(wlanIfname));
        strncpy(ethIfname, GnomeUtils::getEthIfname(), sizeof(ethIfname));
        _NetworkManagerEvents = this;
        startNetworkMangerEventMonitor();
    }

    /* Gnome networkmanger events call backs */
    void NetworkManagerEvents::onActiveInterfaceChangeCb(std::string newIface)
    {
        static std::string oldIface = "Unknown";
        if(oldIface != newIface)
        {
            NMLOG_INFO("old interface - %s new interface - %s", oldIface.c_str(), newIface.c_str());
            oldIface = newIface;
            if(_instance != nullptr)
                _instance->ReportActiveInterfaceChange(oldIface, newIface);
        }
    }

    void NetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::InterfaceState newState, std::string iface)
    {
        std::string state;
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
        if(_instance != nullptr && (iface == GnomeUtils::getWifiIfname() || iface == GnomeUtils::getEthIfname()))
            _instance->ReportInterfaceStateChange(static_cast<Exchange::INetworkManager::InterfaceState>(newState), iface);
    }

    void NetworkManagerEvents::onWIFIStateChanged(Exchange::INetworkManager::WiFiState state, std::string& wifiStateStr)
    {
        NMLOG_DEBUG("wifi state changed: %d ; NM wifi: %s", state, wifiStateStr.c_str());
        if(_instance != nullptr)
            _instance->ReportWiFiStateChange(static_cast<Exchange::INetworkManager::WiFiState>(state));
    }

    void NetworkManagerEvents::onAddressChangeCb(std::string iface, bool acquired, bool isIPv6, std::string ipAddress)
    {
        static std::map<std::string, std::string> ipv6Map;
        static std::map<std::string, std::string> ipv4Map;

        if (isIPv6)
        {
            if (acquired == false) { // ip lost event
                if(ipv6Map.size() < 1)
                {
                    return; // no ipv6 added yet no event posting
                }
                ipAddress = ipv6Map[iface];
                ipv6Map[iface].clear();
            }
            else // acquired
            {
                if (ipAddress.compare(0, 5, "fe80:") == 0 || 
                    ipAddress.compare(0, 6, "fe80::") == 0) {
                    NMLOG_DEBUG("It's link-local ip");
                    return; // It's link-local
                }
                /* same ip comes multiple time so avoding that */
                if (ipv6Map[iface].find(ipAddress) == std::string::npos && !ipAddress.empty()) {
                    ipv6Map[iface] = ipAddress; // SLAAC protocol may include multip ipv6 address last one will save
                }
                else
                    return; // skip same ip event posting if empty request
            }
        }
        else
        {
            if (acquired == false) { // ip lost event
                if(ipv4Map.size() < 1)
                {
                    return; // no ipv4 added yet no event posting
                }
                ipAddress = ipv4Map[iface];
                ipv4Map[iface].clear();
            }
            else
            {
                if(!ipAddress.empty())
                    ipv4Map[iface] = ipAddress;
                else
                    return;
            }
        }

        Exchange::INetworkManager::IPStatus ipStatus{};
        if(acquired)
            ipStatus = Exchange::INetworkManager::IP_ACQUIRED;

        if(_instance != nullptr)
            _instance->ReportIPAddressChange(iface, isIPv6?"IPv6":"IPv4", ipAddress, ipStatus);

        NMLOG_INFO("iface:%s - ipaddress:%s - %s - %s", iface.c_str(), ipAddress.c_str(), acquired?"acquired":"lost", isIPv6?"isIPv6":"isIPv4");
    }

    void NetworkManagerEvents::onAvailableSSIDsCb(const char* wifiDevicePath)
    {
        GDBusProxy* wProxy = nullptr;
        GError* error = nullptr;

        NMLOG_DEBUG("wifi scanning completed ...");
        if(_NetworkManagerEvents->doScanNotify == false) {
           return;
        }

        _NetworkManagerEvents->doScanNotify = false;
        wProxy = _NetworkManagerEvents->eventDbus.getNetworkManagerWirelessProxy(wifiDevicePath);
        if(wProxy == NULL)
            return;

        GVariant* result = g_dbus_proxy_call_sync(wProxy, "GetAllAccessPoints", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
        if (error) {
            NMLOG_ERROR("Error creating proxy: %s", error->message);
            g_error_free(error);
            g_object_unref(wProxy);
            return;
        }

        GVariantIter* iter = NULL;
        const gchar* apPath = NULL;
        JsonArray ssidList = JsonArray();
        bool oneSSIDFound = false;

        g_variant_get(result, "(ao)", &iter);
        if(iter == NULL)
        {
            NMLOG_ERROR("GetAllAccessPoints GVariantIter returned NULL");
            g_variant_unref(result);
            g_object_unref(wProxy);
            return;
        }
        while (g_variant_iter_loop(iter, "o", &apPath)) {
            Exchange::INetworkManager::WiFiSSIDInfo wifiInfo;
            // NMLOG_DEBUG("Access Point Path: %s", apPath);
            if(apPath != NULL && GnomeUtils::getApDetails(_NetworkManagerEvents->eventDbus, apPath, wifiInfo))
            {
                JsonObject ssidObj;
                if(GnomeUtils::convertSsidInfoToJsonObject(wifiInfo, ssidObj))
                {
                    ssidList.Add(ssidObj);
                    oneSSIDFound = true;
                }
            }
        }

        if(oneSSIDFound) {
            std::string ssids{};
            ssidList.ToString(ssids);
            if(_instance != nullptr)
                _instance->ReportAvailableSSIDs(ssidList);
            NMLOG_DEBUG("scanned ssids: %s", ssids.c_str());
        }

        g_variant_iter_free(iter);
        g_variant_unref(result);
        g_object_unref(wProxy);
    }

    void NetworkManagerEvents::setwifiScanOptions(bool doNotify)
    {
        doScanNotify.store(doNotify);
        if(!doScanNotify)
            NMLOG_DEBUG("stoped periodic wifi scan result notify");
    }

    }   // Plugin
}   // WPEFramework

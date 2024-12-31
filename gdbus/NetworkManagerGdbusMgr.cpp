/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include <glib.h>
#include <gio/gio.h>
#include <string>

#include "NetworkManagerGdbusMgr.h"
#include "NetworkManagerLogger.h"

namespace WPEFramework
{
    namespace Plugin
    {

        DbusMgr::DbusMgr() : connection(NULL)
        {
            GError* error = NULL;
            NMLOG_INFO("DbusMgr");
            connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
            if (!connection) {
                NMLOG_FATAL("Error connecting to system D-Bus bus: %s ", error->message);
                g_error_free(error);
            }
            flags = static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_NONE);
        }

        DbusMgr::~DbusMgr() {
            NMLOG_INFO("~DbusMgr");
            if (connection) {
                g_object_unref(connection);
                connection = NULL;
            }
        }

        GDBusConnection* DbusMgr::getConnection()
        {
            if (connection) {
                return connection;
            }

            GError* error = NULL;
            connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
            if (!connection && error) {
                NMLOG_FATAL("Error reconnecting to system bus: %s ", error->message);
                g_error_free(error);
                return NULL;
            }

            flags = static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START);
            return connection;
        }

        GDBusProxy* DbusMgr::getNetworkManagerProxy()
        {
            GError* error = NULL;
            nmProxy = NULL;
            nmProxy = g_dbus_proxy_new_sync(
                getConnection(), G_DBUS_PROXY_FLAGS_NONE, NULL, "org.freedesktop.NetworkManager",
                "/org/freedesktop/NetworkManager",
                "org.freedesktop.NetworkManager",
                NULL,
                &error
            );

            if (nmProxy == NULL || error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating D-Bus proxy: %s", error->message);
                g_error_free(error);
                return NULL;
            }

            return nmProxy;
        }

        GDBusProxy* DbusMgr::getNetworkManagerSettingsProxy()
        {
            GError* error = NULL;
            GDBusProxy* proxy = g_dbus_proxy_new_sync(
                getConnection(), flags, NULL, "org.freedesktop.NetworkManager",
                "/org/freedesktop/NetworkManager/Settings",
                "org.freedesktop.NetworkManager.Settings",
                NULL,
                &error
            );

            if (error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return NULL;
            }

            return proxy;
        }

        GDBusProxy* DbusMgr::getNetworkManagerSettingsConnectionProxy(const char* connPath)
        {
            GError* error = NULL;
            GDBusProxy* proxy = g_dbus_proxy_new_sync(
                getConnection(), flags, NULL, "org.freedesktop.NetworkManager",
                connPath,
                "org.freedesktop.NetworkManager.Settings.Connection",
                NULL,
                &error
            );

            if (error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return NULL;
            }

            return proxy;
        }

        GDBusProxy* DbusMgr::getNetworkManagerDeviceProxy(const char* devicePath)
        {
            GError* error = NULL;
            GDBusProxy* proxy = g_dbus_proxy_new_sync(
                getConnection(), flags, NULL, "org.freedesktop.NetworkManager",
                devicePath,
                "org.freedesktop.NetworkManager.Device",
                NULL,
                &error
            );

            if (error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return NULL;
            }

            return proxy;
        }

        GDBusProxy* DbusMgr::getNetworkManagerActiveConnProxy(const char* activePath)
        {
            GError* error = NULL;
            GDBusProxy* proxy = g_dbus_proxy_new_sync(
                getConnection(), flags, NULL, "org.freedesktop.NetworkManager",
                activePath,
                "org.freedesktop.NetworkManager.Connection.Active",
                NULL,
                &error
            );

            if (error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return NULL;
            }

            return proxy;
        }

        GDBusProxy* DbusMgr::getNetworkManagerAccessPointProxy(const char* apPath)
        {
            GError* error = NULL;
            GDBusProxy* proxy = g_dbus_proxy_new_sync(
                getConnection(), flags, NULL, "org.freedesktop.NetworkManager",
                apPath,
                "org.freedesktop.NetworkManager.AccessPoint",
                NULL,
                &error
            );

            if (error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return NULL;
            }

            return proxy;
        }

        GDBusProxy* DbusMgr::getNetworkManagerWirelessProxy(const char* wirelessDevPath)
        {
            GError* error = NULL;
            GDBusProxy* proxy = g_dbus_proxy_new_sync(
                getConnection(), flags, NULL, "org.freedesktop.NetworkManager",
                wirelessDevPath,
                "org.freedesktop.NetworkManager.Device.Wireless",
                NULL,
                &error
            );

            if (error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return NULL;
            }

            return proxy;
        }

        GDBusProxy* DbusMgr::getNetworkManagerIpv4Proxy(const char* ipConfigPath)
        {
            GError* error = NULL;
            GDBusProxy* proxy = g_dbus_proxy_new_sync(
                getConnection(), flags, NULL, "org.freedesktop.NetworkManager",
                ipConfigPath,
                "org.freedesktop.NetworkManager.IP4Config",
                NULL,
                &error
            );

            if (error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return NULL;
            }

            return proxy;
        }

        GDBusProxy* DbusMgr::getNetworkManagerIpv6Proxy(const char* ipConfigPath)
        {
            GError* error = NULL;
            GDBusProxy* proxy = g_dbus_proxy_new_sync(
                getConnection(), flags, NULL, "org.freedesktop.NetworkManager",
                ipConfigPath,
                "org.freedesktop.NetworkManager.IP6Config",
                NULL,
                &error
            );

            if (error != NULL) {
                g_dbus_error_strip_remote_error(error);
                NMLOG_FATAL("Error creating proxy: %s", error->message);
                g_clear_error(&error);
                return NULL;
            }

            return proxy;
        }

    } // Plugin
} // WPEFramework


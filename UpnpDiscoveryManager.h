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
#ifndef __UPNP_H__
#define __UPNP_H__

#include <libgupnp/gupnp.h>
#include <string>
#include <vector>

#define LOG_ERR(msg, ...)    g_printerr("[%s:%d] " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(msg, ...)   g_printerr("[%s:%d] " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)

class UpnpDiscoveryManager
{
public:
    static UpnpDiscoveryManager* getInstance()
    {
        static UpnpDiscoveryManager instance;
        return &instance;
    }
    void findGatewayDevice(const std::string& interface);
    UpnpDiscoveryManager(const UpnpDiscoveryManager&) = delete;
    void operator=(const UpnpDiscoveryManager&) = delete;

private:
    UpnpDiscoveryManager();
    ~UpnpDiscoveryManager();
    void initialiseUpnp(const std::string& interface);
    void clearUpnpExistingRequests();
    static void* runMainLoop(void *arg);
    static gboolean logTelemetry(void* arg);
    void stopSearchGatewayDevice();
    void on_device_proxy_available(GUPnPControlPoint *control_point, GUPnPDeviceProxy *proxy);
    static void deviceProxyAvailableCallback(GUPnPControlPoint *control_point, GUPnPDeviceProxy *proxy, gpointer user_data) { 
        auto *self = static_cast<UpnpDiscoveryManager *>(user_data);
        self->on_device_proxy_available(control_point, proxy);
    }
    GUPnPContext*       m_context;
    GUPnPControlPoint*  m_controlPoint;
    GMainLoop*          m_mainLoop;
    GThread *           m_thread_upnp;
    std::string         m_apMake;
    std::string         m_apModelName;
    std::string         m_apModelNumber;
    std::ostringstream  m_gatewayDetails;
    std::mutex          m_apMutex;
    static std::string const m_deviceInternetGateway;
    static const int    LOGGING_PERIOD_IN_SEC = 2; //15min * 60
};
#endif

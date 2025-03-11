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

#ifndef __UPNPDISCOVERYMANAGER_H__
#define __UPNPDISCOVERYMANAGER_H__

#include <libgupnp/gupnp.h>
#include <mutex>
#include <string>
#if USE_TELEMETRY
#include <telemetry_busmessage_sender.h>
#endif

#define LOG_ERR(msg, ...)    g_printerr( msg "\n", ##__VA_ARGS__)
#define LOG_INFO(msg, ...)   g_printerr( msg "\n", ##__VA_ARGS__)

#define MAX_CONTEXT_FAIL  15
#define T2_EVENT_DATA_LEN 128

class UpnpDiscoveryManager
{
public:
    UpnpDiscoveryManager();
    ~UpnpDiscoveryManager();
    bool findGatewayDevice(const std::string& interface);
    void enterWait();

private:
    void logTelemetry(std::string& message);
    static gboolean discoveryTimeout(void* arg);
    void exitWait();
    gboolean initialiseUpnp(const std::string& interface);
    void stopSearchGatewayDevice();
    void on_device_proxy_available(GUPnPControlPoint *control_point, GUPnPDeviceProxy *proxy);
    static void deviceProxyAvailableCallback(GUPnPControlPoint *control_point, GUPnPDeviceProxy *proxy, gpointer user_data) 
    { 
        auto* self = static_cast<UpnpDiscoveryManager *>(user_data);
        self->on_device_proxy_available(control_point, proxy);
    }
    GUPnPContext*       m_context;
    GUPnPControlPoint*  m_controlPoint;
    GMainLoop*          m_mainLoop;
    std::string         m_interface;
    std::string         m_apMake;
    std::string         m_apModelName;
    std::string         m_apModelNumber;
    std::ostringstream  m_gatewayDetails;
    static const std::string m_deviceInternetGateway;
    static const guint  DISCOVERY_PORT = 1901; 
    static const int    DISCOVERY_TIMEOUT_IN_SEC = 180; 
};
#endif /* __UPNPDISCOVERYMANAGER_H__ */

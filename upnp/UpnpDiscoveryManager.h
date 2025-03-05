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

#define MAX_CONTEXT_FAIL  5
#define T2_EVENT_DATA_LEN 128

class UpnpDiscoveryManager
{
public:
    UpnpDiscoveryManager();
    ~UpnpDiscoveryManager();
    bool findGatewayDevice(const std::string& interface);
    void enterWait();

private:
    void logTelemetry(std::string message);
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

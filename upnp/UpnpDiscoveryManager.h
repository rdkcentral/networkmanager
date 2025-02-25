#ifndef __UPNP_H__
#define __UPNP_H__

#include <libgupnp/gupnp.h>
#include <mutex>
#include <string>
#if USE_TELEMETRY
#include <telemetry_busmessage_sender.h>
#endif

#define LOG_ERR(msg, ...)    g_printerr("[%s:%d] " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(msg, ...)   g_printerr("[%s:%d] " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define MAX_CONTEXT_FAIL 3

class UpnpDiscoveryManager
{
public:
    UpnpDiscoveryManager();
    ~UpnpDiscoveryManager();
    void findGatewayDevice(const std::string& interface);

private:
    static gboolean logTelemetry(void* arg);
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
    std::mutex          m_apMutex;
    std::string         m_apMake;
    std::string         m_apModelName;
    std::string         m_apModelNumber;
    std::ostringstream  m_gatewayDetails;
    static std::string const m_deviceInternetGateway;
    static const int    LOGGING_PERIOD_IN_SEC = 30; //15min * 60
    bool                m_upnpRunStatus;
};
#endif

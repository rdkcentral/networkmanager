#include <sstream>
#include "UpnpDiscoveryManager.h"

std::string const UpnpDiscoveryManager::m_deviceInternetGateway = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";

UpnpDiscoveryManager::UpnpDiscoveryManager()
{
    m_context = NULL;
    m_controlPoint = NULL;

#if USE_TELEMETRY
    // Initialize Telemtry  
    t2_init("networkmanager-plugin");
#endif
    //Timer thread to handle telemetry logging
    g_timeout_add_seconds (LOGGING_PERIOD_IN_SEC, GSourceFunc(&UpnpDiscoveryManager::logTelemetry), this);
}

UpnpDiscoveryManager::~UpnpDiscoveryManager()
{
    if (m_controlPoint)
        g_object_unref(m_controlPoint);
    if (m_context)
        g_object_unref(m_context);
}

gboolean UpnpDiscoveryManager::initialiseUpnp(const std::string& interface)
{
    GError *error = NULL;
    uint8_t errCount = 0;
    
    do
    {
        // Create a gupnp context
        m_context = gupnp_context_new(NULL, interface.c_str(), 0, &error);
        if (!m_context) 
        {
            LOG_ERR("Error creating Upnp context: %s", error->message);
            g_clear_error(&error);
            errCount++;
            sleep(1);
        }
    } while ((m_context == NULL) && (errCount < MAX_CONTEXT_FAIL));
    if (errCount == MAX_CONTEXT_FAIL)
    {
        LOG_ERR("Context creation failed");
	    return false;
    }
    
    // Create a control point for InternetGatewayDevice
    m_controlPoint = gupnp_control_point_new(m_context, m_deviceInternetGateway.c_str());
    if (!m_controlPoint) 
    {
        LOG_ERR("Error creating control point");
	    return false;
    }

    // Connects a callback function to a signal for InternetGatewayDevice
    g_signal_connect(m_controlPoint, "device-proxy-available",
                         G_CALLBACK(&UpnpDiscoveryManager::deviceProxyAvailableCallback), this);
    return true;
}

gboolean UpnpDiscoveryManager::logTelemetry(void *arg)
{
#if USE_TELEMETRY 
    std::lock_guard<std::mutex> lock(((UpnpDiscoveryManager *)arg)->m_apMutex);
    LOG_INFO("Connected to Gateway: %s", ((UpnpDiscoveryManager *)arg)->m_gatewayDetails.str().c_str());
    //T2 telemtery logging
    T2ERROR t2error = t2_event_s("gateway_details_split", ((UpnpDiscoveryManager *)arg)->m_gatewayDetails.str().c_str());
    if (t2error != T2ERROR_SUCCESS)
    {
        LOG_ERR("t2_event_s(\"%s\", \"%s\") returned error code %d", "gateway_details_split", ((UpnpDiscoveryManager *)arg)->m_gatewayDetails.str().c_str(), t2error);
    }
#endif
    return true;
}

void UpnpDiscoveryManager::findGatewayDevice(const std::string& interface)
{
    //Initialise gupnp context
    if (true == initialiseUpnp(interface))
    {
        // Start discovery to find InternetGatewayDevice
        LOG_INFO("Searching for InternetGatewayDevice");
        gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(m_controlPoint), TRUE);
    }
    else
    {
        LOG_ERR("Failed in initialising upnp");
    }
}

void UpnpDiscoveryManager::on_device_proxy_available(GUPnPControlPoint *controlPoint, GUPnPDeviceProxy *proxy)
{ 
    m_apMake = gupnp_device_info_get_manufacturer(GUPNP_DEVICE_INFO(proxy)); 
    m_apModelName = gupnp_device_info_get_model_name(GUPNP_DEVICE_INFO(proxy));
    m_apModelNumber = gupnp_device_info_get_model_number(GUPNP_DEVICE_INFO(proxy));
    m_apMake = m_apMake.substr(0, m_apMake.find(','));
    // Stop discovery to find InternetGatewayDevice
    stopSearchGatewayDevice(); 
    std::lock_guard<std::mutex> lock(m_apMutex);
    m_gatewayDetails.str("");
    m_gatewayDetails.clear();
    m_gatewayDetails << m_apMake << "," << m_apModelName << "," << m_apModelNumber;
    LOG_INFO("Received gateway details: %s", m_gatewayDetails.str().c_str());
}

void UpnpDiscoveryManager::stopSearchGatewayDevice() 
{
    gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(m_controlPoint), FALSE);
}

int main(int argc, char *argv[])
{  
    if (argc < 2) 
    {
        LOG_INFO("Usage: %s <interface name>", argv[0]);
        return 1; 
    }
    
    UpnpDiscoveryManager ssdpDiscover;
    GMainLoop*           mainLoop;

    LOG_INFO("SSDP discover to fetch InternetGatewayDevice on interface %s", argv[1]);
    ssdpDiscover.findGatewayDevice(argv[1]);
    mainLoop = g_main_loop_new(NULL, FALSE);
    // gmain loop need to be running for upnp discovery and telemetry logging
    g_main_loop_run(mainLoop);
    if (mainLoop) 
    {
        g_main_loop_quit(mainLoop);
        g_main_loop_unref(mainLoop);
    }
    return 1;
}

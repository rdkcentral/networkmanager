/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include <sstream>
#include "UpnpDiscoveryManager.h"

UpnpDiscoveryManager::UpnpDiscoveryManager()
{
    m_context = NULL;
    m_controlPoint = NULL;
    m_mainLoop = NULL;
    m_gatewayDetails.str("");
    m_gatewayDetails.clear();
    m_deviceInternetGateway = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";
#if USE_TELEMETRY
    // Initialize Telemtry  
    t2_init("upnpdiscover");
#endif
}

UpnpDiscoveryManager::~UpnpDiscoveryManager()
{
    if (m_controlPoint)
        g_object_unref(m_controlPoint);
    if (m_context)
        g_object_unref(m_context);
    if (m_mainLoop) 
    {
        g_main_loop_quit(m_mainLoop);
        g_main_loop_unref(m_mainLoop);
    }
}

/* @brief Initialise GUPNP context */
gboolean UpnpDiscoveryManager::initialiseUpnp(const std::string& interface)
{
    GError *error = NULL;
    uint8_t errCount = 0;

    do
    {
        // Create a gupnp context
    #ifdef GUPNP_1_0
        m_context = gupnp_context_new(NULL, interface.c_str(), UPNP_DISCOVERY_PORT, &error);
    #else
        m_context = gupnp_context_new(interface.c_str(), UPNP_DISCOVERY_PORT, &error);
    #endif
        if (!m_context) 
        {
            LOG_ERR("Error creating Upnp context: %s", error->message);
            g_clear_error(&error);
            errCount++;
            sleep(2);
        }
    } while ((m_context == NULL) && (errCount < UPNP_MAX_CONTEXT_FAIL));
    if (errCount == UPNP_MAX_CONTEXT_FAIL)
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

/* @brief Telemetry Logging */
void UpnpDiscoveryManager::logTelemetry(std::string message)
{
#if USE_TELEMETRY 
    //T2 telemtery logging
    T2ERROR t2error = t2_event_s("Router_Discovered", (char*)message.c_str());
    if (t2error != T2ERROR_SUCCESS)
    {
        LOG_ERR("t2_event_s(\"%s\", \"%s\") returned error code %d", "Router_Discovered", message.c_str(), t2error);
    }
#endif
}

/* @brief SSDP Discovery Timeout */
gboolean UpnpDiscoveryManager::discoveryTimeout(void *arg)
{
    auto manager = static_cast<UpnpDiscoveryManager*>(arg); 
    manager->stopSearchGatewayDevice();
    manager->m_gatewayDetails << "Unknown";
    LOG_INFO("Router_Discovered : %s", manager->m_gatewayDetails.str().c_str());
    manager->logTelemetry(manager->m_gatewayDetails.str().c_str()); 
    manager->exitWait();
    return true;
}

/* @brief Find the gateway details by sending SSDP discovery on wifi/ethernet interface */
bool UpnpDiscoveryManager::findGatewayDevice(const std::string& interface)
{
    m_interface = interface;
    //Initialise gupnp context
    if (true == initialiseUpnp(interface))
    {
        //Create timer to handle upnp discovery timeout
        g_timeout_add_seconds (UPNP_DISCOVERY_TIMEOUT_IN_SEC, GSourceFunc(&UpnpDiscoveryManager::discoveryTimeout), this);
        // Start discovery to find InternetGatewayDevice
        gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(m_controlPoint), TRUE);
        return true;
    }
    else
    {
        m_gatewayDetails << "Unknown";
	LOG_INFO("Router_Discovered : %s", m_gatewayDetails.str().c_str());
        logTelemetry(m_gatewayDetails.str().c_str()); 
        return false;
    }
}

/* @brief Callback getting invoked when SSDP reply is received from gateway */
void UpnpDiscoveryManager::on_device_proxy_available(GUPnPControlPoint *controlPoint, GUPnPDeviceProxy *proxy)
{ 
    m_apMake = gupnp_device_info_get_manufacturer(GUPNP_DEVICE_INFO(proxy)); 
    m_apModelName = gupnp_device_info_get_model_name(GUPNP_DEVICE_INFO(proxy));
    m_apModelNumber = gupnp_device_info_get_model_number(GUPNP_DEVICE_INFO(proxy));
    // Remove the string after symbol <,> For example: <manufacturer>NETGEAR,Inc.</manufacturer> 
    m_apMake = m_apMake.substr(0, m_apMake.find(','));
    m_apModelName = m_apModelName.substr(0, m_apModelName.find(','));
    m_apModelNumber = m_apModelNumber.substr(0, m_apModelNumber.find(','));
    // Stop discovery to find InternetGatewayDevice
    stopSearchGatewayDevice(); 
    m_gatewayDetails << m_apMake << "," << m_apModelName << "," << m_apModelNumber;
    LOG_INFO("Router_Discovered : %s", m_gatewayDetails.str().c_str());
    if (m_gatewayDetails.str().length() >= UPNP_T2_EVENT_DATA_LEN)
    {
        m_gatewayDetails.str("");
        m_gatewayDetails.clear();
        m_gatewayDetails << m_apModelName << "," << m_apModelNumber;
    }
    logTelemetry(m_gatewayDetails.str()); 
    exitWait();
}

/* @brief Stop sending SSDP discovery */
void UpnpDiscoveryManager::stopSearchGatewayDevice() 
{
    gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(m_controlPoint), FALSE);
}

/* @brief Wait in gmain() loop */
void UpnpDiscoveryManager::enterWait()
{
    m_mainLoop = g_main_loop_new(NULL, FALSE);
    // gmain loop need to be running for upnp discovery
    g_main_loop_run(m_mainLoop);
}

/* @brief Exit the gmain() loop */
void UpnpDiscoveryManager::exitWait()
{
    if (m_mainLoop) 
    {
        g_main_loop_quit(m_mainLoop);
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = NULL;
    }
}
#if DISABLE_MAIN
int main(int argc, char *argv[])
{  
    if (argc < 2) 
    {
        LOG_INFO("Usage: %s <interface name>", argv[0]);
        return 1; 
    }
    UpnpDiscoveryManager ssdpDiscover;
    LOG_INFO("SSDP discover to fetch InternetGatewayDevice on interface %s", argv[1]);
    if (true == ssdpDiscover.findGatewayDevice(argv[1]))
    {
        ssdpDiscover.enterWait();
	return 0;
    }
    return 1;
}
#endif

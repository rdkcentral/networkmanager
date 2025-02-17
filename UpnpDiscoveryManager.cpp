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

#include <fstream>
#include <sstream>
#include <thread>
#include "UpnpDiscoveryManager.h"

std::string const UpnpDiscoveryManager::m_deviceInternetGateway = "urn:schemas-upnp-org:device:InternetGatewayDevice:1";

UpnpDiscoveryManager::UpnpDiscoveryManager()
{
    m_context = NULL;
    m_controlPoint = NULL;
    m_mainLoop = NULL;

    m_mainLoop = g_main_loop_new(NULL, FALSE);

#if USE_TELEMETRY
    // Initialize Telemtry  
    t2_init("networkmanager-plugin");
#endif

    //Timer thread to handle telemetry logging
    g_timeout_add_seconds (LOGGING_PERIOD_IN_SEC, GSourceFunc(&UpnpDiscoveryManager::logTelemetry), this);

    m_threadUpnp  = g_thread_new("thread_upnp", UpnpDiscoveryManager::runUpnp, this);
    m_threadGmain = g_thread_new("thread_gmain", UpnpDiscoveryManager::runMainLoop, this);
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

void UpnpDiscoveryManager::NotifyInterfaceStateChangeEvent()
{
    // Clear the upnp running status if there is any interface state change
    std::lock_guard<std::mutex> lock(m_upnpStatusMutex);
	m_upnpRunStatus = false;
}

void UpnpDiscoveryManager::NotifyIpAcquiredEvent(const std::string& interface)
{
    std::lock_guard<std::mutex> lock(m_upnpStatusMutex);
    // Once IP is acquired, notify upnp thread to start discovery
    if (m_upnpRunStatus == false)
    {
        m_upnpRunStatus = true;
        m_interface = interface;
        m_upnpCv.notify_one();
    }
}

void* UpnpDiscoveryManager::runUpnp(void *arg)
{
    UpnpDiscoveryManager* manager = static_cast<UpnpDiscoveryManager*>(arg);
    while(true)
    {
        std::unique_lock<std::mutex> lock(manager->m_upnpCvMutex);  
        manager->m_upnpCv.wait(lock);
        if (true == manager->m_upnpRunStatus)
        {
            manager->findGatewayDevice(manager->m_interface);
        }
    }
}

gboolean UpnpDiscoveryManager::initialiseUpnp(const std::string& interface)
{
    GError *error = NULL;
    uint8_t count = 0;
    
    do
    {
        // Create a gupnp context
        m_context = gupnp_context_new(NULL, interface.c_str(), 0, &error);
        if (!m_context) 
        {
            LOG_ERR("Error creating Upnp context: %s", error->message);
            g_clear_error(&error);
            count++;
            sleep(1);
        }
    } while ((m_context == NULL) && (count < MAX_CONTEXT_FAIL));
    if (count == MAX_CONTEXT_FAIL)
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

void* UpnpDiscoveryManager::runMainLoop(void *arg)
{
    // gmain loop need to be running for upnp discovery and telemetry logging
    g_main_loop_run(((UpnpDiscoveryManager *)arg)->m_mainLoop);
}

gboolean UpnpDiscoveryManager::logTelemetry(void *arg)
{
    std::lock_guard<std::mutex> lock(((UpnpDiscoveryManager *)arg)->m_apMutex);
    LOG_INFO("Connected_to_Gateway: %s", ((UpnpDiscoveryManager *)arg)->m_gatewayDetails.str().c_str());
#if USE_TELEMETRY
    LOG_INFO("Telemetry logging");   
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
    //Clear previous gupnp contexts if any
    clearUpnpExistingRequests();

    //Initialise gupnp context
    if (true == initialiseUpnp(interface))
    {
        // Start discovery to find InternetGatewayDevice
        if (TRUE != gssdp_resource_browser_get_active(GSSDP_RESOURCE_BROWSER(m_controlPoint)))
        {
            LOG_INFO("Searching for InternetGatewayDevice");
            gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(m_controlPoint), TRUE);
        }
    }
    else
    {
        LOG_ERR("Failed in initialising upnp");
    }
}

void UpnpDiscoveryManager::clearUpnpExistingRequests()
{
    if (m_controlPoint)
    {
        g_object_ref_sink(m_controlPoint);
        stopSearchGatewayDevice();
        g_object_unref(m_controlPoint);
        m_controlPoint = NULL;
    }
    if (m_context)
    {
        g_object_unref(m_context);
        m_context = NULL;
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
    if (TRUE == gssdp_resource_browser_get_active(GSSDP_RESOURCE_BROWSER(m_controlPoint)))
    {
        gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(m_controlPoint), FALSE);
    }
}

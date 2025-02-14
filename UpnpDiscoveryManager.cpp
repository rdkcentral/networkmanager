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
    m_mainLoop = g_main_loop_new(NULL, FALSE);

    // Initialize Telemtry	    
    t2_init("networkmanager-plugin");
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

void UpnpDiscoveryManager::startUpnpDiscovery(const std::string& interface)
{
    m_interface = interface;
    m_upnpReady = true;
    m_upnpCv.notify_one();
}

void* UpnpDiscoveryManager::runUpnp(void *arg)
{
    UpnpDiscoveryManager* manager = static_cast<UpnpDiscoveryManager*>(arg);
    int timeoutInMin = 30;
    while(true)
    {
        std::unique_lock<std::mutex> lock(manager->m_upnpCvMutex);  
        if (manager->m_upnpCv.wait_for(lock, std::chrono::minutes(timeoutInMin)) == std::cv_status::timeout) {
            LOG_INFO("upnp run thread timeout");
        }
        if (manager->m_upnpReady)
        {
	    sleep(3);
            manager->findGatewayDevice(manager->m_interface);
            manager->m_upnpReady = false;
        }
    }
}

bool UpnpDiscoveryManager::initialiseUpnp(const std::string& interface)
{
    GError *error = NULL;
    // Create a gupnp context
    m_context = gupnp_context_new(NULL, interface.c_str(), 0, &error);
    if (!m_context) {
        LOG_ERR("Error creating Upnp context: %s", error->message);
        g_clear_error(&error);
	return false;
    }

    // Create a control point for InternetGatewayDevice
    m_controlPoint = gupnp_control_point_new(m_context, m_deviceInternetGateway.c_str());
    if (!m_controlPoint) {
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
   g_main_loop_run(((UpnpDiscoveryManager *)arg)->m_mainLoop);
}

gboolean UpnpDiscoveryManager::logTelemetry(void *arg)
{
    std::lock_guard<std::mutex> lock(((UpnpDiscoveryManager *)arg)->m_apMutex);
    LOG_INFO("TELEMETRY-UPNP: %s", ((UpnpDiscoveryManager *)arg)->m_gatewayDetails.str().c_str());
    //T2 telemtery logging
    T2ERROR t2error = t2_event_s("gateway_details_split", ((UpnpDiscoveryManager *)arg)->m_gatewayDetails.str().c_str());
    if (t2error != T2ERROR_SUCCESS)
        LOG_ERR("t2_event_s(\"%s\", \"%s\") returned error code %d", "gateway_details_split", ((UpnpDiscoveryManager *)arg)->m_gatewayDetails.str().c_str(), t2error);
    return TRUE;
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
        LOG_INFO("Failed in initialising upnp");
    }
}

void UpnpDiscoveryManager::clearUpnpExistingRequests()
{
    if (m_controlPoint)
    {
        stopSearchGatewayDevice();
        g_object_unref(m_controlPoint);
    }
    if (m_context)
    {
        g_object_unref(m_context);
    }
}

void UpnpDiscoveryManager::on_device_proxy_available(GUPnPControlPoint *controlPoint, GUPnPDeviceProxy *proxy)
{ 
    m_apMake = gupnp_device_info_get_manufacturer(GUPNP_DEVICE_INFO(proxy)); 
    m_apModelName = gupnp_device_info_get_model_name(GUPNP_DEVICE_INFO(proxy));
    m_apModelNumber = gupnp_device_info_get_model_number(GUPNP_DEVICE_INFO(proxy));
    m_apMake = m_apMake.substr(0, m_apMake.find(','));
    LOG_INFO("Connected to Gateway: %s, %s, %s", m_apMake.c_str(), m_apModelName.c_str(), m_apModelNumber.c_str());
    // Stop discovery to find InternetGatewayDevice
    stopSearchGatewayDevice(); 
    std::lock_guard<std::mutex> lock(m_apMutex);
    m_gatewayDetails.str("");
    m_gatewayDetails.clear();
    m_gatewayDetails << "make=" << m_apMake << ",model_name=" << m_apModelName << ",model_number=" << m_apModelNumber;
}

void UpnpDiscoveryManager::stopSearchGatewayDevice() 
{
    LOG_INFO("Stop searching for InternetGatewayDevice");
    if (TRUE == gssdp_resource_browser_get_active(GSSDP_RESOURCE_BROWSER(m_controlPoint)))
    {
        gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(m_controlPoint), FALSE);
    }
    LOG_INFO("Stopped searching for InternetGatewayDevice");
}

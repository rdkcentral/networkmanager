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

    //Create timeout threads to handle telemetry logging
    g_timeout_add_seconds (LOGGING_PERIOD_IN_SEC, GSourceFunc(&UpnpDiscoveryManager::logTelemetry), this);

   // pthread_mutex_init(&mutex, NULL);
    m_thread_upnp = g_thread_new("thread_gmain", UpnpDiscoveryManager::runMainLoop, this);
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

void UpnpDiscoveryManager::initialiseUpnp(const std::string& interface)
{
    GError *error = NULL;
    // Create a gupnp context
    m_context = gupnp_context_new(interface.c_str(), 0, &error);
    if (!m_context) {
        LOG_ERR("Error creating Upnp context: %s", error->message);
        g_clear_error(&error);
    }

    // Create a control point for InternetGatewayDevice
    m_controlPoint = gupnp_control_point_new(m_context, m_deviceInternetGateway.c_str());
    if (!m_controlPoint) {
        LOG_ERR("Error creating control point");
    }

    // Connects a callback function to a signal for InternetGatewayDevice
    g_signal_connect(m_controlPoint, "device-proxy-available",
                         G_CALLBACK(&UpnpDiscoveryManager::deviceProxyAvailableCallback), this);
}

void* UpnpDiscoveryManager::runMainLoop(void *arg)
{
   g_main_loop_run(((UpnpDiscoveryManager *)arg)->m_mainLoop);
}

gboolean UpnpDiscoveryManager::logTelemetry(void *arg)
{
    std::lock_guard<std::mutex> lock(((UpnpDiscoveryManager *)arg)->m_apMutex);
    //T2 telemtery logging
    LOG_INFO("TELEMETRY_UPNP_GATEWAY_DETAILS: %s", ((UpnpDiscoveryManager *)arg)->m_gatewayDetails.str().c_str());
    return TRUE;
}

void UpnpDiscoveryManager::findGatewayDevice(const std::string& interface)
{
    clearUpnpExistingRequests();

    //Initialise gupnp context
    initialiseUpnp(interface);

    // Start discovery to find InternetGatewayDevice
    if (TRUE != gssdp_resource_browser_get_active(GSSDP_RESOURCE_BROWSER(m_controlPoint)))
    {
        LOG_INFO("Searching for InternetGatewayDevice");
        gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(m_controlPoint), TRUE);
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
        g_object_unref(m_context);
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
}

int main()
{
    UpnpDiscoveryManager* obj = UpnpDiscoveryManager::getInstance();
    //Fetch the gateway details
    obj->findGatewayDevice("en0");
    LOG_INFO("AM HERE");
    while(true)
    {
       sleep(10);
    }
}


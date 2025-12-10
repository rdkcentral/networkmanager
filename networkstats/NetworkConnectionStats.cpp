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

#include "NetworkConnectionStats.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

#define LOG_ERR(msg, ...)    fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__)
#define LOG_INFO(msg, ...)   printf("INFO: " msg "\n", ##__VA_ARGS__)

using namespace WPEFramework::Plugin;

/* @brief Singleton instance pointer */
NetworkConnectionStats* NetworkConnectionStats::m_instance = nullptr;

/* @brief Mutex for thread-safe singleton initialization */
std::mutex NetworkConnectionStats::m_instance_mutex;

/* @brief Constructor */
NetworkConnectionStats::NetworkConnectionStats() : iprovider(nullptr), /* m_service(nullptr), */ m_interface(""), m_ipv4Address(""), m_ipv6Address(""),
                                                     m_ipv4Route(""), m_ipv6Route(""), m_ipv4Dns(""), m_ipv6Dns("")
{
    LOG_INFO("NetworkConnectionStats constructor");
#if USE_TELEMETRY
    // Initialize Telemetry
    t2_init("networkstats");
#endif
}

/* @brief Destructor */
NetworkConnectionStats::~NetworkConnectionStats()
{
    LOG_INFO("NetworkConnectionStats destructor");
}

/* @brief Initialize network connection stats */
const std::string NetworkConnectionStats::Initialize(/* PluginHost::IShell* service */)
{
    std::string message{};
    
    // m_service = service;  // Commented for standalone compilation
    // m_service->AddRef();  // Commented for standalone compilation
    
    LOG_INFO("NetworkConnectionStats initializing...");
    
    // Initialize network provider
    iprovider = new NetworkJsonRPCProvider();
    if (iprovider == nullptr) {
        LOG_ERR("Failed to create NetworkJsonRPCProvider");
        message = "Failed to initialize network provider";
    } else {
        LOG_INFO("NetworkJsonRPCProvider initialized successfully");
    }
    
    return message;
}

/* @brief Deinitialize and cleanup resources */
void NetworkConnectionStats::Deinitialize(/* PluginHost::IShell* service */)
{
    LOG_INFO("NetworkConnectionStats deinitialize");
    
    if (iprovider) {
        delete iprovider;
        iprovider = nullptr;
    }
    
    // if (m_service) {  // Commented for standalone compilation
    //     m_service->Release();
    //     m_service = nullptr;
    // }
}

/* @brief Get singleton instance of NetworkConnectionStats */
NetworkConnectionStats* NetworkConnectionStats::getInstance()
{
    if (m_instance == nullptr)
    {
        std::lock_guard<std::mutex> guard(m_instance_mutex);
        if (m_instance == nullptr)
        {
            m_instance = new NetworkConnectionStats();
            LOG_INFO("Created new NetworkConnectionStats instance");
        }
    }
    return m_instance;
}

/* @brief Telemetry Logging */
void NetworkConnectionStats::logTelemetry(const std::string& eventName, const std::string& message)
{
#if USE_TELEMETRY
    // T2 telemetry logging
    T2ERROR t2error = t2_event_s(eventName.c_str(), (char*)message.c_str());
    if (t2error != T2ERROR_SUCCESS)
    {
        LOG_ERR("t2_event_s(\"%s\", \"%s\") returned error code %d", 
                eventName.c_str(), message.c_str(), t2error);
    }
#endif
}

/* @brief Check and validate current connection type */
void NetworkConnectionStats::connectionTypeCheck()
{
    LOG_INFO("Checking connection type");
    if (iprovider) {
        std::string connType = iprovider->getConnectionType();
        LOG_INFO("Connection type: %s", connType.c_str());
        logTelemetry("Connection_Type", connType);
    }
}

/* @brief Verify IP address configuration */
void NetworkConnectionStats::connectionIpCheck()
{
    LOG_INFO("Checking connection IP");
    if (iprovider) {
        m_interface = iprovider->getInterface();
        
        // Get IPv4 settings - provider handles JSON parsing internally
        m_ipv4Address = iprovider->getIpv4Address(m_interface);
        m_ipv4Route = iprovider->getIpv4Gateway();
        m_ipv4Dns = iprovider->getIpv4PrimaryDns();
        
        // Get IPv6 settings - provider handles JSON parsing internally
        m_ipv6Address = iprovider->getIpv6Address(m_interface);
        m_ipv6Route = iprovider->getIpv6Gateway();
        m_ipv6Dns = iprovider->getIpv6PrimaryDns();
        
        LOG_INFO("Interface: %s, IPv4: %s, IPv6: %s", m_interface.c_str(), m_ipv4Address.c_str(), m_ipv6Address.c_str());
        LOG_INFO("IPv4 Gateway: %s, IPv4 DNS: %s", m_ipv4Route.c_str(), m_ipv4Dns.c_str());
        LOG_INFO("IPv6 Gateway: %s, IPv6 DNS: %s", m_ipv6Route.c_str(), m_ipv6Dns.c_str());
        
        // Log telemetry events
        std::string interfaceInfo = m_interface + "," + m_ipv4Address + "," + m_ipv6Address;
        logTelemetry("Network_Interface_Info", interfaceInfo);
        
        std::string ipv4Info = m_ipv4Route + "," + m_ipv4Dns;
        logTelemetry("IPv4_Gateway_DNS", ipv4Info);
        
        std::string ipv6Info = m_ipv6Route + "," + m_ipv6Dns;
        logTelemetry("IPv6_Gateway_DNS", ipv6Info);
    }
}

/* @brief Check default IPv4 route availability */
void NetworkConnectionStats::defaultIpv4RouteCheck()
{
    LOG_INFO("Checking default IPv4 route");
    
    if (m_ipv4Address.empty()) {
        LOG_ERR("No IPv4 address available for route check");
        return;
    }
    
    // Check if IPv4 address is valid (not empty and not 0.0.0.0)
    if (m_ipv4Address == "0.0.0.0" || m_ipv4Address == "") {
        LOG_ERR("Invalid IPv4 address: %s", m_ipv4Address.c_str());
        return;
    }
    
    // Check if we have gateway (route) information
    if (!m_ipv4Route.empty() && m_ipv4Route != "0.0.0.0") {
        LOG_INFO("IPv4 route check: Interface %s has valid IPv4 gateway %s", 
                 m_interface.c_str(), m_ipv4Route.c_str());
    } else {
        LOG_INFO("IPv4 route check: Interface %s has valid IPv4 address %s (gateway: %s)", 
                 m_interface.c_str(), m_ipv4Address.c_str(), m_ipv4Route.c_str());
    }
}

/* @brief Check default IPv6 route availability */
void NetworkConnectionStats::defaultIpv6RouteCheck()
{
    LOG_INFO("Checking default IPv6 route");
    
    if (m_ipv6Address.empty()) {
        LOG_ERR("No IPv6 address available for route check");
        return;
    }
    
    // Check if IPv6 address is valid (not empty and not ::)
    if (m_ipv6Address == "::" || m_ipv6Address == "") {
        LOG_ERR("Invalid IPv6 address: %s", m_ipv6Address.c_str());
        return;
    }
    
    // Check if we have gateway (route) information
    if (!m_ipv6Route.empty() && m_ipv6Route != "::") {
        LOG_INFO("IPv6 route check: Interface %s has valid IPv6 gateway %s", 
                 m_interface.c_str(), m_ipv6Route.c_str());
    } else {
        LOG_INFO("IPv6 route check: Interface %s has valid IPv6 address %s (gateway: %s)", 
                 m_interface.c_str(), m_ipv6Address.c_str(), m_ipv6Route.c_str());
    }
}

/* @brief Monitor packet loss to gateway */
void NetworkConnectionStats::gatewayPacketLossCheck()
{
    LOG_INFO("Checking gateway packet loss");
    
    if (!iprovider) {
        LOG_ERR("Provider not initialized");
        return;
    }
    
    // Check IPv4 gateway packet loss
    if (!m_ipv4Route.empty() && m_ipv4Route != "0.0.0.0") {
        LOG_INFO("Pinging IPv4 gateway: %s", m_ipv4Route.c_str());
        bool ipv4Success = iprovider->pingToGatewayCheck(m_ipv4Route, "IPv4", 5, 30);
        
        if (ipv4Success) {
            LOG_INFO("IPv4 gateway ping successful");
            std::string ipv4PacketLoss = iprovider->getPacketLoss();
            std::string ipv4AvgRtt = iprovider->getAvgRtt();
            if (!ipv4PacketLoss.empty()) {
                LOG_INFO("IPv4 Packet Loss: %s", ipv4PacketLoss.c_str());
                logTelemetry("IPv4_Packet_Loss", ipv4PacketLoss);
            }
            if (!ipv4AvgRtt.empty()) {
                LOG_INFO("IPv4 Average RTT: %s ms", ipv4AvgRtt.c_str());
                logTelemetry("IPv4_Avg_RTT", ipv4AvgRtt);
            }
            logTelemetry("IPv4_Gateway_Ping_Status", "Success");
        } else {
            LOG_ERR("IPv4 gateway ping failed");
            logTelemetry("IPv4_Gateway_Ping_Status", "Failed");
        }
    } else {
        LOG_INFO("No IPv4 gateway available for packet loss check");
    }
    
    // Check IPv6 gateway packet loss
    if (!m_ipv6Route.empty() && m_ipv6Route != "::") {
        LOG_INFO("Pinging IPv6 gateway: %s", m_ipv6Route.c_str());
        bool ipv6Success = iprovider->pingToGatewayCheck(m_ipv6Route, "IPv6", 5, 30);
        
        if (ipv6Success) {
            LOG_INFO("IPv6 gateway ping successful");
            std::string ipv6PacketLoss = iprovider->getPacketLoss();
            std::string ipv6AvgRtt = iprovider->getAvgRtt();
            if (!ipv6PacketLoss.empty()) {
                LOG_INFO("IPv6 Packet Loss: %s", ipv6PacketLoss.c_str());
                logTelemetry("IPv6_Packet_Loss", ipv6PacketLoss);
            }
            if (!ipv6AvgRtt.empty()) {
                LOG_INFO("IPv6 Average RTT: %s ms", ipv6AvgRtt.c_str());
                logTelemetry("IPv6_Avg_RTT", ipv6AvgRtt);
            }
            logTelemetry("IPv6_Gateway_Ping_Status", "Success");
        } else {
            LOG_ERR("IPv6 gateway ping failed");
            logTelemetry("IPv6_Gateway_Ping_Status", "Failed");
        }
    } else {
        LOG_INFO("No IPv6 gateway available for packet loss check");
    }
}

/* @brief Validate DNS configuration and resolution */
void NetworkConnectionStats::networkDnsCheck()
{
    LOG_INFO("Checking network DNS");
    
    // Check if we have DNS entries from either IPv4 or IPv6
    bool hasDnsEntries = false;
    
    if (!m_ipv4Dns.empty()) {
        LOG_INFO("IPv4 Primary DNS: %s", m_ipv4Dns.c_str());
        logTelemetry("IPv4_Primary_DNS", m_ipv4Dns);
        hasDnsEntries = true;
    }
    
    if (!m_ipv6Dns.empty()) {
        LOG_INFO("IPv6 Primary DNS: %s", m_ipv6Dns.c_str());
        logTelemetry("IPv6_Primary_DNS", m_ipv6Dns);
        hasDnsEntries = true;
    }
    
    // Print status based on whether we have DNS entries
    if (hasDnsEntries) {
        LOG_INFO("DNS entries retrieved successfully");
        logTelemetry("DNS_Status", "Success");
    } else {
        LOG_ERR("Empty DNS file");
        logTelemetry("DNS_Status", "Failed");
    }
}

/* @brief Generate network diagnostics report */
void NetworkConnectionStats::generateReport()
{
    LOG_INFO("Generating network diagnostics report");
    
    // Run all diagnostic checks
    this->connectionTypeCheck();
    this->connectionIpCheck();
    this->defaultIpv4RouteCheck();
    this->defaultIpv6RouteCheck();
    this->gatewayPacketLossCheck();
    this->networkDnsCheck();
    
    LOG_INFO("Network diagnostics report completed");
}

/* @brief Generate Periodic reporting diagnostics report */
void NetworkConnectionStats::periodic_reporting()
{
    LOG_INFO("Starting periodic reporting thread...");
    
    // Create thread that runs every 10 minutes
    std::thread reportingThread([this]() {
        while (true)
        {
            LOG_INFO("Running periodic network diagnostics report...");
            
            // Generate report
            this->generateReport();
            
            LOG_INFO("Periodic report completed. Sleeping for 10 minutes...");
            
            // Sleep for 10 minutes (600 seconds)
            std::this_thread::sleep_for(std::chrono::minutes(10));
        }
    });
    
    // Detach thread to run independently
    reportingThread.detach();
}


/* @brief Main function to initiate class objects */
int main(int argc, char *argv[])
{
    LOG_INFO("Starting NetworkConnectionStats application...");

    // Get singleton instance
    NetworkConnectionStats* statsManager = NetworkConnectionStats::getInstance();
    
    if (statsManager == nullptr)
    {
        LOG_ERR("Failed to create NetworkConnectionStats instance");
        return -1;
    }

    LOG_INFO("NetworkConnectionStats instance created successfully");
    
    // Initialize (standalone mode)
    std::string initResult = statsManager->Initialize();
    if (!initResult.empty()) {
        LOG_ERR("Initialization failed: %s", initResult.c_str());
        return -1;
    }
    
    LOG_INFO("NetworkConnectionStats initialized successfully");
    
    // Start periodic reporting (runs every 10 minutes)
    statsManager->periodic_reporting();
    
    LOG_INFO("Periodic reporting thread started. Main thread will keep running...");
    
    // Keep main thread alive
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }
    
    // Cleanup (unreachable in this example, but shown for completeness)
    statsManager->Deinitialize();
    
    return 0;
}

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

#include "NetworkConnectionStatsImplementation.h"
#include <chrono>

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(NetworkConnectionStatsImplementation, 1, 0);

    NetworkConnectionStatsImplementation::NetworkConnectionStatsImplementation()
        : _adminLock()
        , _service(nullptr)
        , m_provider(nullptr)
        , m_interface("")
        , m_ipv4Address("")
        , m_ipv6Address("")
        , m_ipv4Route("")
        , m_ipv6Route("")
        , m_ipv4Dns("")
        , m_ipv6Dns("")
        , _periodicReportingEnabled(true)  // Auto-enabled
        , _reportingIntervalMinutes(10)    // Default 10 minutes
        , _stopReporting(false)
    {
        TRACE(Trace::Information, (_T("NetworkConnectionStatsImplementation Constructor")));
#if USE_TELEMETRY
        t2_init("networkstats");
#endif
    }

    NetworkConnectionStatsImplementation::~NetworkConnectionStatsImplementation()
    {
        TRACE(Trace::Information, (_T("NetworkConnectionStatsImplementation Destructor")));
        
        // Stop periodic reporting thread
        _stopReporting = true;
        if (_reportingThread.joinable()) {
            _reportingThread.join();
        }
        
        if (m_provider) {
            delete m_provider;
            m_provider = nullptr;
        }
        
        _service = nullptr;
    }

    uint32_t NetworkConnectionStatsImplementation::Configure(PluginHost::IShell* service)
    {
        TRACE(Trace::Information, (_T("NetworkConnectionStatsImplementation::Configure")));
        uint32_t result = Core::ERROR_NONE;
        
        ASSERT(service != nullptr);
        _service = service;
        _service->AddRef();
        
        // Parse configuration
        JsonObject config;
        config.FromString(_service->ConfigLine());
        
        if (config.HasLabel("reportingInterval")) {
            _reportingIntervalMinutes = config["reportingInterval"].Number();
            TRACE(Trace::Information, (_T("Reporting interval set to %u minutes"), _reportingIntervalMinutes.load()));
        }
        
        if (config.HasLabel("autoStart")) {
            _periodicReportingEnabled = config["autoStart"].Boolean();
        }
        
        // Initialize network provider
        m_provider = new NetworkJsonRPCProvider();
        if (m_provider == nullptr) {
            TRACE(Trace::Error, (_T("Failed to create NetworkJsonRPCProvider")));
            result = Core::ERROR_GENERAL;
        } else {
            TRACE(Trace::Information, (_T("NetworkJsonRPCProvider initialized successfully")));
            
            // Generate initial report
            generateReport();
            
            // Start periodic reporting if enabled
            if (_periodicReportingEnabled) {
                _stopReporting = false;
                _reportingThread = std::thread(&NetworkConnectionStatsImplementation::periodicReportingThread, this);
                TRACE(Trace::Information, (_T("Periodic reporting started with %u minute interval"), _reportingIntervalMinutes.load()));
            }
        }
        
        return result;
    }

    void NetworkConnectionStatsImplementation::periodicReportingThread()
    {
        TRACE(Trace::Information, (_T("Periodic reporting thread started")));
        
        while (!_stopReporting && _periodicReportingEnabled) {
            // Sleep for configured interval
            std::this_thread::sleep_for(std::chrono::minutes(_reportingIntervalMinutes.load()));
            
            if (_stopReporting || !_periodicReportingEnabled) {
                break;
            }
            
            // Generate report
            generateReport();
            TRACE(Trace::Information, (_T("Periodic report generated")));
        }
        
        TRACE(Trace::Information, (_T("Periodic reporting thread stopped")));
    }

    void NetworkConnectionStatsImplementation::generateReport()
    {
        TRACE(Trace::Information, (_T("Generating network diagnostics report")));
        
        _adminLock.Lock();
        
        // Run all diagnostic checks
        connectionTypeCheck();
        connectionIpCheck();
        defaultIpv4RouteCheck();
        defaultIpv6RouteCheck();
        gatewayPacketLossCheck();
        networkDnsCheck();
        
        _adminLock.Unlock();
        
        TRACE(Trace::Information, (_T("Network diagnostics report completed")));
    }

    void NetworkConnectionStatsImplementation::logTelemetry(const std::string& eventName, const std::string& message)
    {
#if USE_TELEMETRY
        T2ERROR t2error = t2_event_s(eventName.c_str(), (char*)message.c_str());
        if (t2error != T2ERROR_SUCCESS) {
            TRACE(Trace::Error, (_T("t2_event_s(\"%s\", \"%s\") failed with error %d"), 
                   eventName.c_str(), message.c_str(), t2error));
        }
#endif
    }

    void NetworkConnectionStatsImplementation::connectionTypeCheck()
    {
        if (m_provider) {
            std::string connType = m_provider->getConnectionType();
            TRACE(Trace::Information, (_T("Connection type: %s"), connType.c_str()));
            logTelemetry("Connection_Type", connType);
        }
    }

    void NetworkConnectionStatsImplementation::connectionIpCheck()
    {
        if (m_provider) {
            m_interface = m_provider->getInterface();
            
            // Get IPv4 settings
            m_ipv4Address = m_provider->getIpv4Address(m_interface);
            m_ipv4Route = m_provider->getIpv4Gateway();
            m_ipv4Dns = m_provider->getIpv4PrimaryDns();
            
            // Get IPv6 settings
            m_ipv6Address = m_provider->getIpv6Address(m_interface);
            m_ipv6Route = m_provider->getIpv6Gateway();
            m_ipv6Dns = m_provider->getIpv6PrimaryDns();
            
            TRACE(Trace::Information, (_T("Interface: %s, IPv4: %s, IPv6: %s"), 
                   m_interface.c_str(), m_ipv4Address.c_str(), m_ipv6Address.c_str()));
            TRACE(Trace::Information, (_T("IPv4 Gateway: %s, DNS: %s"), 
                   m_ipv4Route.c_str(), m_ipv4Dns.c_str()));
            TRACE(Trace::Information, (_T("IPv6 Gateway: %s, DNS: %s"), 
                   m_ipv6Route.c_str(), m_ipv6Dns.c_str()));
            
            // Log telemetry events
            std::string interfaceInfo = m_interface + "," + m_ipv4Address + "," + m_ipv6Address;
            logTelemetry("Network_Interface_Info", interfaceInfo);
            
            std::string ipv4Info = m_ipv4Route + "," + m_ipv4Dns;
            logTelemetry("IPv4_Gateway_DNS", ipv4Info);
            
            std::string ipv6Info = m_ipv6Route + "," + m_ipv6Dns;
            logTelemetry("IPv6_Gateway_DNS", ipv6Info);
        }
    }

    void NetworkConnectionStatsImplementation::defaultIpv4RouteCheck()
    {
        if (!m_ipv4Address.empty() && m_ipv4Address != "0.0.0.0") {
            if (!m_ipv4Route.empty() && m_ipv4Route != "0.0.0.0") {
                TRACE(Trace::Information, (_T("IPv4: Interface %s has gateway %s"),
                       m_interface.c_str(), m_ipv4Route.c_str()));
            } else {
                TRACE(Trace::Warning, (_T("IPv4: No valid gateway for interface %s"), m_interface.c_str()));
            }
        }
    }

    void NetworkConnectionStatsImplementation::defaultIpv6RouteCheck()
    {
        if (!m_ipv6Address.empty() && m_ipv6Address != "::") {
            if (!m_ipv6Route.empty() && m_ipv6Route != "::") {
                TRACE(Trace::Information, (_T("IPv6: Interface %s has gateway %s"),
                       m_interface.c_str(), m_ipv6Route.c_str()));
            } else {
                TRACE(Trace::Warning, (_T("IPv6: No valid gateway for interface %s"), m_interface.c_str()));
            }
        }
    }

    void NetworkConnectionStatsImplementation::gatewayPacketLossCheck()
    {
        if (!m_provider) {
            return;
        }
        
        // Check IPv4 gateway packet loss
        if (!m_ipv4Route.empty() && m_ipv4Route != "0.0.0.0") {
            TRACE(Trace::Information, (_T("Pinging IPv4 gateway: %s"), m_ipv4Route.c_str()));
            bool success = m_provider->pingToGatewayCheck(m_ipv4Route, "IPv4", 5, 30);
            if (success) {
                std::string packetLoss = m_provider->getPacketLoss();
                std::string avgRtt = m_provider->getAvgRtt();
                TRACE(Trace::Information, (_T("IPv4 gateway ping - Loss: %s%%, RTT: %sms"),
                       packetLoss.c_str(), avgRtt.c_str()));
                
                std::string ipv4PingInfo = "IPv4," + m_ipv4Route + "," + packetLoss + "," + avgRtt;
                logTelemetry("Gateway_Ping_Stats", ipv4PingInfo);
            } else {
                TRACE(Trace::Error, (_T("IPv4 gateway ping failed")));
                logTelemetry("Gateway_Ping_Stats", "IPv4," + m_ipv4Route + ",failed,0");
            }
        }
        
        // Check IPv6 gateway packet loss
        if (!m_ipv6Route.empty() && m_ipv6Route != "::") {
            TRACE(Trace::Information, (_T("Pinging IPv6 gateway: %s"), m_ipv6Route.c_str()));
            bool success = m_provider->pingToGatewayCheck(m_ipv6Route, "IPv6", 5, 30);
            if (success) {
                std::string packetLoss = m_provider->getPacketLoss();
                std::string avgRtt = m_provider->getAvgRtt();
                TRACE(Trace::Information, (_T("IPv6 gateway ping - Loss: %s%%, RTT: %sms"),
                       packetLoss.c_str(), avgRtt.c_str()));
                
                std::string ipv6PingInfo = "IPv6," + m_ipv6Route + "," + packetLoss + "," + avgRtt;
                logTelemetry("Gateway_Ping_Stats", ipv6PingInfo);
            } else {
                TRACE(Trace::Error, (_T("IPv6 gateway ping failed")));
                logTelemetry("Gateway_Ping_Stats", "IPv6," + m_ipv6Route + ",failed,0");
            }
        }
    }

    void NetworkConnectionStatsImplementation::networkDnsCheck()
    {
        bool hasDns = false;
        
        if (!m_ipv4Dns.empty()) {
            TRACE(Trace::Information, (_T("IPv4 DNS: %s"), m_ipv4Dns.c_str()));
            hasDns = true;
        }
        
        if (!m_ipv6Dns.empty()) {
            TRACE(Trace::Information, (_T("IPv6 DNS: %s"), m_ipv6Dns.c_str()));
            hasDns = true;
        }
        
        if (hasDns) {
            TRACE(Trace::Information, (_T("DNS configuration present")));
        } else {
            TRACE(Trace::Warning, (_T("No DNS configuration found")));
            logTelemetry("DNS_Status", "No DNS configured");
        }
    }

} // namespace Plugin
} // namespace WPEFramework

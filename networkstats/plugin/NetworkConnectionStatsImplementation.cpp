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
#include "NetworkConnectionStatsLogger.h"
#include <chrono>

#ifdef USE_RFCAPI
#include "rfcapi.h"
#endif

using namespace NetworkConnectionStatsLogger;

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0
#define SUBSCRIPTION_RETRY_INTERVAL_MS 500

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(NetworkConnectionStatsImplementation, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    NetworkConnectionStatsImplementation::NetworkConnectionStatsImplementation()
        : _adminLock()
        , m_provider(nullptr)
        , m_interface("")
        , m_ipv4Address("")
        , m_ipv6Address("")
        , m_ipv4Route("")
        , m_ipv6Route("")
        , m_ipv4Dns("")
        , m_ipv6Dns("")
        , _periodicReportingEnabled(true)  // Auto-enabled
        , _reportingIntervalSeconds(600)   // Default 10 minutes (600 seconds)
        , _stopReporting(false)
        , _currentAssocState(AssocState::WIFI_ASSOC_IDLE)
        , _stopSubscriptionRetry(false)
        , m_subsIfaceStateChange(false)
        , m_subsActIfaceChange(false)
        , m_subsIPAddrChange(false)
        , m_subsWiFiStateChange(false)
    {
        
        NSLOG_INFO("NetworkConnectionStatsImplementation Constructor");
        /* Set NetworkManager Out-Process name to be "NetworkConnectionStats" */
        Core::ProcessInfo().Name("NetworkConnectionStats");
        NSLOG_INFO((_T("NetworkConnectionStats Out-Of-Process Instantiation; SHA: " _T(EXPAND_AND_QUOTE(PLUGIN_BUILD_REFERENCE)))));
        initStateMachine();
#if USE_TELEMETRY
        t2_init("networkstats");
#endif
    }

    NetworkConnectionStatsImplementation::~NetworkConnectionStatsImplementation()
    {
        NSLOG_INFO("NetworkConnectionStatsImplementation Destructor");
        
        // Stop subscription retry thread
        _stopSubscriptionRetry = true;
        if (_subscriptionRetryThread.joinable()) {
            _subscriptionRetryThread.join();
        }
        
        // Stop reporting threads
        _stopReporting = true;
        
        // Wake timer thread immediately so it doesn't block shutdown
        _timerCv.notify_one();
        
        // Push STOP message to unblock consumer thread
        {
            std::lock_guard<std::mutex> lock(_queueMutex);
            Message stopMsg;
            stopMsg.type = MessageType::STOP;
            _messageQueue.push(std::move(stopMsg));
        }
        _queueCondition.notify_one();
        
        // Wait for threads to finish
        if (_timerThread.joinable()) {
            _timerThread.join();
        }
        if (_reportingThread.joinable()) {
            _reportingThread.join();
        }
        
        if (m_provider) {
            delete m_provider;
            m_provider = nullptr;
        }
    }

    /**
     * Register a notification callback
     */
    uint32_t NetworkConnectionStatsImplementation::Register(INetworkConnectionStats::INotification* notification)
    {
        if (nullptr == notification) {
            NSLOG_ERROR("Register: notification parameter is nullptr");
            return Core::ERROR_BAD_REQUEST;
        }
        
        NSLOG_INFO("Register::Enter");
        _notificationLock.Lock();

        // Make sure we can't register the same notification callback multiple times
        if (std::find(_notificationCallbacks.begin(), _notificationCallbacks.end(), notification) == _notificationCallbacks.end()) {
            _notificationCallbacks.push_back(notification);
            notification->AddRef();
        }

        _notificationLock.Unlock();
        
        return Core::ERROR_NONE;
    }

    /**
     * Unregister a notification callback
     */
    uint32_t NetworkConnectionStatsImplementation::Unregister(INetworkConnectionStats::INotification* notification)
    {
        if (nullptr == notification) {
            NSLOG_ERROR("Unregister: notification parameter is nullptr");
            return Core::ERROR_BAD_REQUEST;
        }
        
        NSLOG_INFO("Unregister::Enter");
        _notificationLock.Lock();

        // Remove the notification callback if it exists
        auto itr = std::find(_notificationCallbacks.begin(), _notificationCallbacks.end(), notification);
        if (itr != _notificationCallbacks.end()) {
            (*itr)->Release();
            _notificationCallbacks.erase(itr);
        }

        _notificationLock.Unlock();

        return Core::ERROR_NONE;
    }


    uint32_t NetworkConnectionStatsImplementation::Configure(const string configLine)
    {
        NSLOG_INFO("NetworkConnectionStatsImplementation::Configure");
        uint32_t result = Core::ERROR_NONE;
        
        // Parse configuration
        JsonObject config;
        config.FromString(configLine);
        
        if (config.HasLabel("reportingInterval")) {
            _reportingIntervalSeconds = config["reportingInterval"].Number();
            NSLOG_INFO("Reporting interval set to %u seconds (%u minutes)", 
                   _reportingIntervalSeconds.load(), _reportingIntervalSeconds.load() / 60);
        }
        
        if (config.HasLabel("autoStart")) {
            _periodicReportingEnabled = config["autoStart"].Boolean();
            NSLOG_INFO("Auto-start set to %s", _periodicReportingEnabled ? "true" : "false");
        }
        
        // Get provider type from configuration (default: comrpc)
        std::string providerType = "comrpc";
        if (config.HasLabel("providerType")) {
            providerType = config["providerType"].Value();
        }
        
        // Create network provider using factory
        m_provider = NetworkDataProviderFactory::CreateProvider(providerType);
        if (m_provider == nullptr) {
            NSLOG_ERROR("Failed to create network provider for type: %s", providerType.c_str());
            result = Core::ERROR_GENERAL;
        } else {
            auto factoryType = NetworkDataProviderFactory::ParseProviderType(providerType);
            std::string typeName = NetworkDataProviderFactory::GetProviderTypeName(factoryType);
            NSLOG_INFO("%s provider created", typeName.c_str());
            
            // Initialize the provider
            if (m_provider->Initialize()) {
                NSLOG_INFO("%s provider initialized successfully", typeName.c_str());
                
                // Subscribe to NetworkManager events (with automatic retry)
                _stopSubscriptionRetry = false;
                subscribeToEvents();
                _subscriptionRetryThread = std::thread(&NetworkConnectionStatsImplementation::subscriptionRetryThread, this);
                
                // Generate initial report
                generateReport();
                
                // Start timer and consumer threads if enabled
                if (_periodicReportingEnabled) {
                    _stopReporting = false;
                    _timerThread = std::thread(&NetworkConnectionStatsImplementation::timerThread, this);
                    _reportingThread = std::thread(&NetworkConnectionStatsImplementation::periodicReportingThread, this);
                    NSLOG_INFO("Timer and reporting threads started with %u second interval (%u minutes)", 
                           _reportingIntervalSeconds.load(), _reportingIntervalSeconds.load() / 60);
                }
            } else {
                auto factoryType = NetworkDataProviderFactory::ParseProviderType(providerType);
                std::string typeName = NetworkDataProviderFactory::GetProviderTypeName(factoryType);
                NSLOG_ERROR("Failed to initialize %s provider", typeName.c_str());
                // Clean up the partially initialized provider to prevent leaks and later accidental use
                delete m_provider;
                m_provider = nullptr;
                result = Core::ERROR_GENERAL;
            }
        }
        
        return result;
    }

    void NetworkConnectionStatsImplementation::timerThread()
    {
        NSLOG_INFO("Timer thread started");
        
        while (!_stopReporting && _periodicReportingEnabled) {
            // Wait for the configured interval or until woken by shutdown.
            // Uses steady_clock internally, so NTP time jumps have no effect.
            {
                std::unique_lock<std::mutex> lock(_timerMutex);
                _timerCv.wait_for(lock,
                    std::chrono::seconds(_reportingIntervalSeconds.load()),
                    [this] { return _stopReporting.load(); });
            }
            
            if (_stopReporting || !_periodicReportingEnabled) {
                break;
            }
            
            // Queue report generation (timer bypasses rate-limit, it has its own 600s interval)
            sendMessage(NetworkEvent::PERIODIC_TIMER, nullptr);
        }
        
        NSLOG_INFO("Timer thread stopped");
    }
    
    void NetworkConnectionStatsImplementation::periodicReportingThread()
    {
        NSLOG_INFO("Periodic reporting thread (consumer) started");
        
        while (!_stopReporting && _periodicReportingEnabled) {
            std::unique_lock<std::mutex> lock(_queueMutex);
            _queueCondition.wait(lock, [this] {
                return !_messageQueue.empty() || _stopReporting;
            });

            if (_stopReporting) {
                break;
            }
            if (_messageQueue.empty()) {
                continue;
            }

            Message msg = std::move(_messageQueue.front());
            _messageQueue.pop();
            lock.unlock();

            if (msg.type == MessageType::STOP) {
                NSLOG_INFO("Consumer: Received STOP message");
                break;
            }

            runStateMachine(msg.event, msg.params.get());
        }
        
        NSLOG_INFO("Periodic reporting thread (consumer) stopped");
    }

    void NetworkConnectionStatsImplementation::generateReport()
    {
        NSLOG_INFO("Generating network diagnostics report");
        
        _adminLock.Lock();
        
        // Run all diagnostic checks
        connectionTypeCheck();
        connectionIpCheck();
        defaultIpv4RouteCheck();
        defaultIpv6RouteCheck();
        networkDnsCheck();
        gatewayPacketLossCheck();
       
        _adminLock.Unlock();
        
        NSLOG_INFO("Network diagnostics report completed");
    }

    void NetworkConnectionStatsImplementation::logTelemetry(const std::string& eventName, const std::string& message)
    {
        //NSLOG_INFO("NS_T2: %s:%s", eventName.c_str(), message.c_str());
#if USE_TELEMETRY
        T2ERROR t2error = t2_event_s(eventName.c_str(), (char*)message.c_str());
        if (t2error != T2ERROR_SUCCESS) {
            NSLOG_ERROR("t2_event_s(\"%s\", \"%s\") failed with error %d", 
                   eventName.c_str(), message.c_str(), t2error);
        }
#endif
    }

    void NetworkConnectionStatsImplementation::connectionTypeCheck()
    {
        if (m_provider) {
            std::string connType = m_provider->getConnectionType();
            NSLOG_INFO("Connection type: %s", connType.c_str());
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
            
            NSLOG_INFO("Interface: %s, IPv4: %s, IPv6: %s", 
                   m_interface.c_str(), m_ipv4Address.c_str(), m_ipv6Address.c_str());
            NSLOG_INFO("IPv4 Gateway: %s, DNS: %s", 
                   m_ipv4Route.c_str(), m_ipv4Dns.c_str());
            NSLOG_INFO("IPv6 Gateway: %s, DNS: %s", 
                   m_ipv6Route.c_str(), m_ipv6Dns.c_str());
            
            // Log telemetry events
            logTelemetry("Network_Interface", m_interface);
            logTelemetry("Network_IPv4_Address", m_ipv4Address);
            logTelemetry("Network_IPv6_Address", m_ipv6Address);
            logTelemetry("IPv4_DNS", m_ipv4Dns);
            logTelemetry("IPv6_DNS", m_ipv6Dns);
        }
    }

    void NetworkConnectionStatsImplementation::defaultIpv4RouteCheck()
    {
        if (!m_ipv4Address.empty() && m_ipv4Address != "0.0.0.0") {
            if (!m_ipv4Route.empty() && m_ipv4Route != "0.0.0.0") {
                NSLOG_INFO("IPv4: Interface %s has gateway %s",
                       m_interface.c_str(), m_ipv4Route.c_str());
                logTelemetry("IPv4_Route_Check", "Success," + m_interface + "," + m_ipv4Route);
            } else {
                NSLOG_INFO("IPv4: No valid gateway for interface %s", m_interface.c_str());
                logTelemetry("IPv4_Route_Check", "Failed,No valid gateway," + m_interface);
            }
        }
    }

    void NetworkConnectionStatsImplementation::defaultIpv6RouteCheck()
    {
        if (!m_ipv6Address.empty() && m_ipv6Address != "::") {
            if (!m_ipv6Route.empty() && m_ipv6Route != "::") {
                NSLOG_INFO("IPv6: Interface %s has gateway %s",
                       m_interface.c_str(), m_ipv6Route.c_str());
                logTelemetry("IPv6_Route_Check", "Success," + m_interface + "," + m_ipv6Route);
            } else {
                NSLOG_INFO("IPv6: No valid gateway for interface %s", m_interface.c_str());
                logTelemetry("IPv6_Route_Check", "Failed,No valid gateway," + m_interface);
            }
        }
    }

    void NetworkConnectionStatsImplementation::gatewayPacketLossCheck()
    {
        if (!m_provider) {
            return;
        }

        // Get RFC threshold for WiFi reassociation tolerance (default: 100%)
        int reassocTolerance = 100;
#ifdef USE_RFCAPI
        {
            RFC_ParamData_t rfcParam = {0};
            WDMP_STATUS wdmpStatus = getRFCParameter("NetworkStats",
                "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.WiFiReset.ReassociateTolerance",
                &rfcParam);
            if (wdmpStatus == WDMP_SUCCESS || wdmpStatus == WDMP_ERR_DEFAULT_VALUE) {
                int rfcValue = atoi(rfcParam.value);
                if (rfcValue > 0 && rfcValue <= 100) {
                    reassocTolerance = rfcValue;
                    NSLOG_INFO("RFC WiFiReset.ReassociateTolerance = %d%%", reassocTolerance);
                } else {
                    NSLOG_WARNING("RFC ReassociateTolerance value '%s' is out of range, using default %d%%",
                           rfcParam.value, reassocTolerance);
                }
            } else {
                NSLOG_WARNING("getRFCParameter for ReassociateTolerance failed (status=%d), using default %d%%",
                       wdmpStatus, reassocTolerance);
            }
        }
#endif

        bool ipv4PacketLoss100 = false;
        bool ipv6PacketLoss100 = false;
        
        // Check IPv4 gateway packet loss
        if (!m_ipv4Route.empty() && m_ipv4Route != "0.0.0.0") {
            NSLOG_INFO("Pinging IPv4 gateway: %s", m_ipv4Route.c_str());
            bool success = m_provider->pingToGatewayCheck(m_ipv4Route, "IPv4", 10);
            std::string packetLoss = m_provider->getPacketLoss();
            std::string avgRtt = m_provider->getAvgRtt();
            
            // Check if packet loss meets or exceeds RFC reassociation threshold
            try {
                if (static_cast<int>(std::stof(packetLoss)) >= reassocTolerance) {
                    ipv4PacketLoss100 = true;
                }
            } catch (...) {}
            
            if (success) {
                NSLOG_INFO("IPv4 gateway ping Loss: %s%%, RTT: %s",
                       packetLoss.c_str(), avgRtt.c_str());
                
                logTelemetry("IPv4_Gateway_Packet_Loss", packetLoss);
                logTelemetry("IPv4_Gateway_RTT", avgRtt);
            } else {
                NSLOG_ERROR("IPv4 gateway ping failed");
                logTelemetry("IPv4_Gateway_Packet_Loss", packetLoss);
                logTelemetry("IPv4_Gateway_RTT", avgRtt);
            }
        }
        
        // Check IPv6 gateway packet loss
        if (!m_ipv6Route.empty() && m_ipv6Route != "::") {
            // Append zone ID for link-local IPv6 addresses
            std::string ipv6Gateway = m_ipv6Route;
            if (ipv6Gateway.find("fe80::") == 0 && !m_interface.empty()) {
                ipv6Gateway += "%" + m_interface;
            }
            NSLOG_INFO("Pinging IPv6 gateway: %s", ipv6Gateway.c_str());
            bool success = m_provider->pingToGatewayCheck(ipv6Gateway, "IPv6", 10);
            std::string packetLoss = m_provider->getPacketLoss();
            std::string avgRtt = m_provider->getAvgRtt();
            
            // Check if packet loss meets or exceeds RFC reassociation threshold
            try {
                if (static_cast<int>(std::stof(packetLoss)) >= reassocTolerance) {
                    ipv6PacketLoss100 = true;
                }
            } catch (...) {}
            
            if (success) {
                NSLOG_INFO("IPv6 gateway ping Loss: %s%%, RTT: %s",
                       packetLoss.c_str(), avgRtt.c_str());
                
                logTelemetry("IPv6_Gateway_Packet_Loss", packetLoss);
                logTelemetry("IPv6_Gateway_RTT", avgRtt);
            } else {
                NSLOG_ERROR("IPv6 gateway ping failed");
                logTelemetry("IPv6_Gateway_Packet_Loss", packetLoss);
                logTelemetry("IPv6_Gateway_RTT", avgRtt);
            }
        }
        
        // WiFi reassociation logic
        // Trigger reassociation if:
        // 1. Connection type is WiFi
        // 2. Both IPv4 and IPv6 packet loss meets or exceeds RFC threshold
        std::string connType = m_provider->getConnectionType();
        
        if ((connType == "WIFI" || connType == "WiFi" || connType == "wifi") && 
            ipv4PacketLoss100 && ipv6PacketLoss100) {
            NSLOG_WARNING("WiFi connection: Both IPv4 and IPv6 gateways have packet loss >= %d%%", reassocTolerance);
            sendMessage(NetworkEvent::GATEWAY_PACKET_LOSS, nullptr);
        }
    }

    void NetworkConnectionStatsImplementation::networkDnsCheck()
    {
        bool hasDns = false;
        
        if (!m_ipv4Dns.empty()) {
            NSLOG_INFO("IPv4 DNS: %s", m_ipv4Dns.c_str());
            hasDns = true;
        }
        
        if (!m_ipv6Dns.empty()) {
            NSLOG_INFO("IPv6 DNS: %s", m_ipv6Dns.c_str());
            hasDns = true;
        }
        
        if (hasDns) {
            NSLOG_INFO("DNS configuration present");
            logTelemetry("DNS_Status", "DNS configured");
        } else {
            NSLOG_WARNING("No DNS configuration found");
            logTelemetry("DNS_Status", "No DNS configured");
        }
    }

    /** NetworkManager Event Subscription */
    void NetworkConnectionStatsImplementation::subscribeToEvents()
    {
        uint32_t errCode = Core::ERROR_GENERAL;
        if (m_provider)
        {
            if (!m_subsIfaceStateChange)
            {
                errCode = m_provider->SubscribeToEvent("onInterfaceStateChange",
                    [this](const WPEFramework::Core::JSON::VariantContainer& parameters) {
                        this->ReportonInterfaceStateChange(parameters);
                    });
                if (Core::ERROR_NONE == errCode)
                    m_subsIfaceStateChange = true;
                else
                    NSLOG_ERROR("Subscribe to onInterfaceStateChange failed, errCode: %u", errCode);
            }

            if (!m_subsActIfaceChange)
            {
                errCode = m_provider->SubscribeToEvent("onActiveInterfaceChange",
                    [this](const WPEFramework::Core::JSON::VariantContainer& parameters) {
                        this->ReportonActiveInterfaceChange(parameters);
                    });
                if (Core::ERROR_NONE == errCode)
                    m_subsActIfaceChange = true;
                else
                    NSLOG_ERROR("Subscribe to onActiveInterfaceChange failed, errCode: %u", errCode);
            }

            if (!m_subsIPAddrChange)
            {
                errCode = m_provider->SubscribeToEvent("onIPAddressChange",
                    [this](const WPEFramework::Core::JSON::VariantContainer& parameters) {
                        this->ReportonIPAddressChange(parameters);
                    });
                if (Core::ERROR_NONE == errCode)
                    m_subsIPAddrChange = true;
                else
                    NSLOG_ERROR("Subscribe to onIPAddressChange failed, errCode: %u", errCode);
            }
            if (!m_subsWiFiStateChange)
            {
                errCode = m_provider->SubscribeToEvent("onWiFiStateChange",
                    [this](const WPEFramework::Core::JSON::VariantContainer& parameters) {
                        this->ReportonWiFiStateChange(parameters);
                    });
                if (Core::ERROR_NONE == errCode)
                    m_subsWiFiStateChange = true;
                else
                    NSLOG_ERROR("Subscribe to onWiFiStateChange failed, errCode: %u", errCode);
            }
        }
        else
            NSLOG_ERROR("m_provider is null");
    }

    void NetworkConnectionStatsImplementation::subscriptionRetryThread()
    {
        NSLOG_INFO("Subscription retry thread started");
        
        while (!_stopSubscriptionRetry)
        {
            // Check if all subscriptions are successful
            if (m_subsIfaceStateChange && m_subsActIfaceChange && m_subsIPAddrChange && m_subsWiFiStateChange)
            {
                NSLOG_INFO("All required events subscribed; Stopping retry thread");
                break;
            }
            
            // Wait before retry
            std::this_thread::sleep_for(std::chrono::milliseconds(SUBSCRIPTION_RETRY_INTERVAL_MS));
            
            if (_stopSubscriptionRetry)
                break;
            
            // Retry subscription
            NSLOG_INFO("Retrying event subscriptions...");
            subscribeToEvents();
        }
        
        NSLOG_INFO("Subscription retry thread stopped");
    }

    /** Event Handling - dispatched through the state machine */
    void NetworkConnectionStatsImplementation::ReportonInterfaceStateChange(const WPEFramework::Core::JSON::VariantContainer& parameters)
    {
        sendMessage(NetworkEvent::IFACE_STATE_CHANGE, &parameters);
    }

    void NetworkConnectionStatsImplementation::ReportonActiveInterfaceChange(const WPEFramework::Core::JSON::VariantContainer& parameters)
    {
        sendMessage(NetworkEvent::ACTIVE_IFACE_CHANGE, &parameters);
    }

    void NetworkConnectionStatsImplementation::ReportonIPAddressChange(const WPEFramework::Core::JSON::VariantContainer& parameters)
    {
        sendMessage(NetworkEvent::IP_ADDR_CHANGE, &parameters);
    }

    void NetworkConnectionStatsImplementation::ReportonWiFiStateChange(const WPEFramework::Core::JSON::VariantContainer& parameters)
    {
        sendMessage(NetworkEvent::WIFI_STATE_CHANGE, &parameters);
    }

    // ---------------------------------------------------------------------------
    // State machine
    // ---------------------------------------------------------------------------

    void NetworkConnectionStatsImplementation::initStateMachine()
    {
        // WIFI_ASSOC_IDLE: all events generate a report; GATEWAY_PACKET_LOSS triggers reassociation
        _stateMachine[AssocState::WIFI_ASSOC_IDLE][NetworkEvent::IFACE_STATE_CHANGE]  =
            &NetworkConnectionStatsImplementation::onAnyEvent_Idle;
        _stateMachine[AssocState::WIFI_ASSOC_IDLE][NetworkEvent::ACTIVE_IFACE_CHANGE] =
            &NetworkConnectionStatsImplementation::onAnyEvent_Idle;
        _stateMachine[AssocState::WIFI_ASSOC_IDLE][NetworkEvent::IP_ADDR_CHANGE]      =
            &NetworkConnectionStatsImplementation::onAnyEvent_Idle;
        _stateMachine[AssocState::WIFI_ASSOC_IDLE][NetworkEvent::WIFI_STATE_CHANGE]   =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_IDLE][NetworkEvent::PERIODIC_TIMER]      =
            &NetworkConnectionStatsImplementation::onAnyEvent_Idle;
        _stateMachine[AssocState::WIFI_ASSOC_IDLE][NetworkEvent::GATEWAY_PACKET_LOSS] =
            &NetworkConnectionStatsImplementation::onGatewayPacketLoss;

        // WIFI_ASSOC_INPROGRESS: all reports suppressed; WIFI_STATE_CONNECTED transitions to COMPLETED
        _stateMachine[AssocState::WIFI_ASSOC_INPROGRESS][NetworkEvent::IFACE_STATE_CHANGE]  =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_INPROGRESS][NetworkEvent::ACTIVE_IFACE_CHANGE] =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_INPROGRESS][NetworkEvent::IP_ADDR_CHANGE]      =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_INPROGRESS][NetworkEvent::WIFI_STATE_CHANGE]   =
            &NetworkConnectionStatsImplementation::onWiFiStateChange_InProgress;
        _stateMachine[AssocState::WIFI_ASSOC_INPROGRESS][NetworkEvent::PERIODIC_TIMER]      =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_INPROGRESS][NetworkEvent::GATEWAY_PACKET_LOSS] =
            &NetworkConnectionStatsImplementation::skipEvent;

        // WIFI_ASSOC_COMPLETED: all reports suppressed; INTERFACE_ACQUIRING_IP transitions back to IDLE
        _stateMachine[AssocState::WIFI_ASSOC_COMPLETED][NetworkEvent::IFACE_STATE_CHANGE]   =
            &NetworkConnectionStatsImplementation::onIfaceStateChange_Completed;
        _stateMachine[AssocState::WIFI_ASSOC_COMPLETED][NetworkEvent::ACTIVE_IFACE_CHANGE]  =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_COMPLETED][NetworkEvent::IP_ADDR_CHANGE]       =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_COMPLETED][NetworkEvent::WIFI_STATE_CHANGE]    =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_COMPLETED][NetworkEvent::PERIODIC_TIMER]       =
            &NetworkConnectionStatsImplementation::skipEvent;
        _stateMachine[AssocState::WIFI_ASSOC_COMPLETED][NetworkEvent::GATEWAY_PACKET_LOSS]  =
            &NetworkConnectionStatsImplementation::skipEvent;

        NSLOG_INFO("State machine initialised; starting in WIFI_ASSOC_IDLE");
    }

    void NetworkConnectionStatsImplementation::sendMessage(
        NetworkEvent event, const WPEFramework::Core::JSON::VariantContainer* params)
    {
        if (params != nullptr) {
            string json;
            params->ToString(json);
            NSLOG_INFO("sendMessage: event=%d params=%s",
                       static_cast<int>(event), json.c_str());
        }

        Message msg;
        msg.event = event;
        if (params) {
            msg.params = std::make_shared<WPEFramework::Core::JSON::VariantContainer>(*params);
        }

        {
            std::lock_guard<std::mutex> lock(_queueMutex);
            _messageQueue.push(std::move(msg));
        }
        _queueCondition.notify_one();
    }

    void NetworkConnectionStatsImplementation::runStateMachine(
        NetworkEvent event, const WPEFramework::Core::JSON::VariantContainer* params)
    {
        NSLOG_INFO("runStateMachine: event=%d state=%d",
                   static_cast<int>(event), static_cast<int>(_currentAssocState));

        auto stateIt = _stateMachine.find(_currentAssocState);
        if (stateIt == _stateMachine.end()) {
            NSLOG_WARNING("runStateMachine: no handlers for state %d", static_cast<int>(_currentAssocState));
            return;
        }
        auto evtIt = stateIt->second.find(event);
        if (evtIt == stateIt->second.end()) {
            NSLOG_WARNING("runStateMachine: no handler for event %d in state %d",
                          static_cast<int>(event), static_cast<int>(_currentAssocState));
            return;
        }
        (this->*(evtIt->second))(params);
    }

    // --- WIFI_ASSOC_INPROGRESS handlers ---

    // onWiFiStateChange_InProgress: WIFI_STATE_CONNECTED → COMPLETED
    void NetworkConnectionStatsImplementation::onWiFiStateChange_InProgress(
        const WPEFramework::Core::JSON::VariantContainer* params)
    {
        if (params && params->HasLabel("status")) {
            std::string status = (*params)["status"].String();
            if (status == "WIFI_STATE_CONNECTED") {
                NSLOG_INFO("INPROGRESS: WIFI_STATE_CONNECTED — transitioning to WIFI_ASSOC_COMPLETED");
                _currentAssocState = AssocState::WIFI_ASSOC_COMPLETED;
            } else {
                NSLOG_INFO("INPROGRESS: WiFi status=%s, suppressing report", status.c_str());
            }
        }
    }

    void NetworkConnectionStatsImplementation::skipEvent(
        const WPEFramework::Core::JSON::VariantContainer* /*params*/)
    {
        NSLOG_INFO("event suppressed — association in progress, report blocked");
    }

    // --- WIFI_ASSOC_IDLE handler ---

    void NetworkConnectionStatsImplementation::onAnyEvent_Idle(
        const WPEFramework::Core::JSON::VariantContainer* /*params*/)
    {
        NSLOG_INFO("WIFI_ASSOC_IDLE: generating report");
        generateReport();
    }

    // --- WIFI_ASSOC_COMPLETED handlers ---

    // onIfaceStateChange_Completed: INTERFACE_ACQUIRING_IP → IDLE
    void NetworkConnectionStatsImplementation::onIfaceStateChange_Completed(
        const WPEFramework::Core::JSON::VariantContainer* params)
    {
        if (params && params->HasLabel("status")) {
            std::string status = (*params)["status"].String();
            if (status == "INTERFACE_ACQUIRING_IP") {
                NSLOG_INFO("COMPLETED: INTERFACE_ACQUIRING_IP — transitioning back to WIFI_ASSOC_IDLE");
                _currentAssocState = AssocState::WIFI_ASSOC_IDLE;
            } else {
                NSLOG_INFO("COMPLETED: interface status=%s, suppressing report", status.c_str());
            }
        }
    }

    void NetworkConnectionStatsImplementation::onGatewayPacketLoss(
        const WPEFramework::Core::JSON::VariantContainer* /*params*/)
    {
        NSLOG_INFO("WiFi reassociation: %s -> WIFI_ASSOC_INPROGRESS",
                   _currentAssocState == AssocState::WIFI_ASSOC_IDLE ? "WIFI_ASSOC_IDLE" : "WIFI_ASSOC_COMPLETED");
        _currentAssocState = AssocState::WIFI_ASSOC_INPROGRESS;

        logTelemetry("Wifi_ReAssoc", "WIFI_Error_Reassociation");
        uint32_t rc = m_provider->invokeWiFiConnect();
        if (rc != Core::ERROR_NONE) {
            NSLOG_ERROR("WiFiConnect call to NetworkManager failed, errCode: %u", rc);
        }
    }

} // namespace Plugin
} // namespace WPEFramework

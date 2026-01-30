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

#pragma once

#include "Module.h"
#include "INetworkConnectionStats.h"
#include "INetworkData.h"
#include "NetworkDataProviderFactory.h"

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

#if USE_TELEMETRY
#include <telemetry_busmessage_sender.h>
#endif

namespace WPEFramework {
namespace Plugin {

    // Internal implementation - runs network diagnostics automatically
    // No external APIs exposed
    class NetworkConnectionStatsImplementation 
        : public Exchange::INetworkConnectionStats {
    
    public:

        NetworkConnectionStatsImplementation(const NetworkConnectionStatsImplementation&) = delete;
        NetworkConnectionStatsImplementation& operator=(const NetworkConnectionStatsImplementation&) = delete;
        NetworkConnectionStatsImplementation();
        virtual ~NetworkConnectionStatsImplementation();

        BEGIN_INTERFACE_MAP(NetworkConnectionStatsImplementation)
        INTERFACE_ENTRY(Exchange::INetworkConnectionStats)
        END_INTERFACE_MAP

        // Handle Notification registration/removal
        uint32_t Register(INetworkConnectionStats::INotification* notification) override;
        uint32_t Unregister(INetworkConnectionStats::INotification* notification) override;

        // Configuration method
        uint32_t Configure(const string configLine) override;

    private:
        // Message queue types
        enum class MessageType {
            GENERATE_REPORT,
            STOP
        };
        
        struct Message {
            MessageType type;
        };
        
        // Internal diagnostic methods
        void connectionTypeCheck();
        void connectionIpCheck();
        void defaultIpv4RouteCheck();
        void defaultIpv6RouteCheck();
        void gatewayPacketLossCheck();
        void networkDnsCheck();
        void generateReport();
        void logTelemetry(const std::string& eventName, const std::string& message);
        void timerThread();
        void periodicReportingThread();
        
        // NetworkManager event subscription
        void subscribeToEvents();
        void subscriptionRetryThread();
        void ReportonInterfaceStateChange(const WPEFramework::Core::JSON::VariantContainer& parameters);
        void ReportonActiveInterfaceChange(const WPEFramework::Core::JSON::VariantContainer& parameters);
        void ReportonIPAddressChange(const WPEFramework::Core::JSON::VariantContainer& parameters);
        void queueReportGeneration(const std::string& source, const WPEFramework::Core::JSON::VariantContainer* parameters = nullptr);

    private:
        mutable Core::CriticalSection _adminLock;
        
        // Notification callbacks
        std::list<Exchange::INetworkConnectionStats::INotification*> _notificationCallbacks;
        Core::CriticalSection _notificationLock;
        
        // Network data provider
        INetworkData* m_provider;
        
        // Cached network data
        std::string m_interface;
        std::string m_ipv4Address;
        std::string m_ipv6Address;
        std::string m_ipv4Route;
        std::string m_ipv6Route;
        std::string m_ipv4Dns;
        std::string m_ipv6Dns;
        
        // Periodic reporting
        std::atomic<bool> _periodicReportingEnabled;
        std::atomic<uint32_t> _reportingIntervalSeconds;
        std::thread _timerThread;
        std::thread _reportingThread;
        std::atomic<bool> _stopReporting;
        
        // Message queue system
        std::queue<Message> _messageQueue;
        std::mutex _queueMutex;
        std::condition_variable _queueCondition;
        
        // NetworkManager event subscription
        std::thread _subscriptionRetryThread;
        std::atomic<bool> _stopSubscriptionRetry;
        bool m_subsIfaceStateChange;
        bool m_subsActIfaceChange;
        bool m_subsIPAddrChange;
    };

} // namespace Plugin
} // namespace WPEFramework

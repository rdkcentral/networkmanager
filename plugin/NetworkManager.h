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

#pragma once

#include "Module.h"

#include "INetworkManager.h"
#include <interfaces/IConfiguration.h>
#include "NetworkManagerLogger.h"

#include <string>
#include <atomic>
#include <mutex>

namespace WPEFramework
{
    namespace Plugin
    {
        /**
         * NetworkManager plugin that exposes an API over both COM-RPC and JSON-RPC
         *
         */
        class NetworkManager : public PluginHost::IPlugin, public PluginHost::JSONRPC, public PluginHost::ISubSystem::IInternet
        {
            /**
             * Our notification handling code
             *
             * Handle both the Activate/Deactivate notifications and provide a handler
             * for notifications raised by the COM-RPC API
             */
            class Notification : public RPC::IRemoteConnection::INotification,
                                 public Exchange::INetworkManager::INotification
            {
            private:
                Notification() = delete;
                Notification(const Notification &) = delete;
                Notification &operator=(const Notification &) = delete;

            public:
                explicit Notification(NetworkManager *parent)
                    : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }
                virtual ~Notification() override
                {
                }

            public:

                void onInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface) override
                {
                    _parent.onInterfaceStateChange(state, interface);
                }

                void onActiveInterfaceChange(const string prevActiveInterface, const string currentActiveinterface) override
                {
                    _parent.onActiveInterfaceChange(prevActiveInterface, currentActiveinterface);
                }

                void onIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status) override
                {
                    _parent.onIPAddressChange(interface, ipversion, ipaddress, status);
                }

                void onInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState) override
                {
                    _parent.onInternetStatusChange(prevState, currState);
                }

                void onAvailableSSIDs(const string jsonOfScanResults) override
                {
                    _parent.onAvailableSSIDs(jsonOfScanResults);
                }

                void onWiFiStateChange(const Exchange::INetworkManager::WiFiState state) override
                {
                    _parent.onWiFiStateChange(state);
                }

                void onWiFiSignalQualityChange(const string ssid, const string strength, const string noise, const string snr, const Exchange::INetworkManager::WiFiSignalQuality quality) override
                {
                    _parent.onWiFiSignalQualityChange(ssid, strength, noise, snr, quality);
                }

                // The activated/deactived methods are part of the RPC::IRemoteConnection::INotification
                // interface. These are triggered when Thunder detects a connection/disconnection over the
                // COM-RPC link.
                void Activated(RPC::IRemoteConnection * /* connection */) override
                {
                }

                void Deactivated(RPC::IRemoteConnection *connection) override
                {
                    // Something's caused the remote connection to be lost - this could be a crash
                    // on the remote side so deactivate ourselves
                    _parent.Deactivated(connection);
                }

                // Build QueryInterface implementation, specifying all possible interfaces we implement
                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::INetworkManager::INotification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

            private:
                NetworkManager &_parent;
            };

        public:
            NetworkManager();
            ~NetworkManager() override;

            // Implement the basic IPlugin interface that all plugins must implement
            const string Initialize(PluginHost::IShell *service) override;
            void Deinitialize(PluginHost::IShell *service) override;
            string Information() const override;

            //override Subscribe from IDispatcher
            uint32_t Subscribe(const uint32_t channel, const string& event, const string& designator) override
            {
                NMLOG_DEBUG("Subscription received for %s event from channelID (%u) with designator as %s", event.c_str(), channel, designator.c_str());
                JSONRPC::Subscribe(channel, event, designator);
                return Core::ERROR_NONE;
            }

            // Do not allow copy/move constructors
            NetworkManager(const NetworkManager &) = delete;
            NetworkManager &operator=(const NetworkManager &) = delete;

            // Build QueryInterface implementation, specifying all possible interfaces we implement
            // This is necessary so that consumers can discover which plugin implements what interface
            BEGIN_INTERFACE_MAP(NetworkManager)

            // Which interfaces do we implement?
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_ENTRY(PluginHost::ISubSystem::IInternet)

            // We need to tell Thunder that this plugin provides the INetworkManager interface, but
            // since it's not actually implemented here we tell Thunder where it can
            // find the real implementation
            // This allows other components to call QueryInterface<INetworkManager>() and
            // receive the actual implementation (which could be in-process or out-of-process)
            INTERFACE_AGGREGATE(Exchange::INetworkManager, _networkManager)
            END_INTERFACE_MAP

            /*
            * ------------------------------------------------------------------------------------------------------------
            * ISubSystem::IInternet methods
            * ------------------------------------------------------------------------------------------------------------
            */
            string PublicIPAddress() const override
            {
                return m_publicIPAddress;
            }
            network_type NetworkType() const override
            {
                return (m_publicIPAddress.empty() == true ? PluginHost::ISubSystem::IInternet::UNKNOWN : (m_publicIPAddressType == "IPV6" ? PluginHost::ISubSystem::IInternet::IPV6 : PluginHost::ISubSystem::IInternet::IPV4));
            }
            void PublishToThunderAboutInternet();
            /* Class to store and manage cached data */
            template<typename CacheValue>
            class Cache {
            public:
                Cache() : is_set(false) {}

                Cache& operator=(const CacheValue& value) {
                    std::lock_guard<std::mutex> lock(mutex);
                    this->value = value;
                    is_set.store(true);
                    return *this;
                }

                Cache& operator=(CacheValue&& value) {
                    std::lock_guard<std::mutex> lock(mutex);
                    this->value = std::move(value);
                    is_set.store(true);
                    return *this;
                }

                bool isSet() const {
                    return is_set.load();
                }

                void reset() {
                    is_set.store(false);
                }

                const CacheValue& getValue() const {
                    std::lock_guard<std::mutex> lock(mutex);
                    return value;
                }

                CacheValue& getValue() {
                    std::lock_guard<std::mutex> lock(mutex);
                    return value;
                }

            private:
                CacheValue value;
                std::atomic<bool> is_set;
                mutable std::mutex mutex;
            };

            // cached varibales
            Cache<Exchange::INetworkManager::IPAddress> m_ipv4AddressCache;
            Cache<Exchange::INetworkManager::IPAddress> m_ipv6AddressCache;
        private:
            // Notification/event handlers
            // Clean up when we're told to deactivate
            void Deactivated(RPC::IRemoteConnection *connection);

            // JSON-RPC setup
            void RegisterAllMethods();
            void UnregisterAllMethods();

            // JSON-RPC methods (take JSON in, spit JSON back out)
            uint32_t SetLogLevel (const JsonObject& parameters, JsonObject& response);
            uint32_t GetLogLevel (const JsonObject& parameters, JsonObject& response);
            uint32_t GetAvailableInterfaces (const JsonObject& parameters, JsonObject& response);
            uint32_t GetPrimaryInterface (const JsonObject& parameters, JsonObject& response);
            uint32_t GetInterfaceState(const JsonObject& parameters, JsonObject& response);
            uint32_t SetInterfaceState(const JsonObject& parameters, JsonObject& response);
            uint32_t GetIPSettings(const JsonObject& parameters, JsonObject& response);
            uint32_t SetIPSettings(const JsonObject& parameters, JsonObject& response);
            uint32_t GetStunEndpoint(const JsonObject& parameters, JsonObject& response);
            uint32_t SetStunEndpoint(const JsonObject& parameters, JsonObject& response);
            uint32_t GetConnectivityTestEndpoints(const JsonObject& parameters, JsonObject& response);
            uint32_t SetConnectivityTestEndpoints(const JsonObject& parameters, JsonObject& response);
            uint32_t IsConnectedToInternet(const JsonObject& parameters, JsonObject& response);
            uint32_t GetCaptivePortalURI(const JsonObject& parameters, JsonObject& response);
            uint32_t GetPublicIP(const JsonObject& parameters, JsonObject& response);
            uint32_t Ping(const JsonObject& parameters, JsonObject& response);
            uint32_t Trace(const JsonObject& parameters, JsonObject& response);
            uint32_t StartWiFiScan(const JsonObject& parameters, JsonObject& response);
            uint32_t StopWiFiScan(const JsonObject& parameters, JsonObject& response);
            uint32_t GetKnownSSIDs(const JsonObject& parameters, JsonObject& response);
            uint32_t AddToKnownSSIDs(const JsonObject& parameters, JsonObject& response);
            uint32_t RemoveKnownSSID(const JsonObject& parameters, JsonObject& response);
            uint32_t WiFiConnect(const JsonObject& parameters, JsonObject& response);
            uint32_t WiFiDisconnect(const JsonObject& parameters, JsonObject& response);
            uint32_t GetConnectedSSID(const JsonObject& parameters, JsonObject& response);
            uint32_t StartWPS(const JsonObject& parameters, JsonObject& response);
            uint32_t StopWPS(const JsonObject& parameters, JsonObject& response);
            uint32_t GetWifiState(const JsonObject& parameters, JsonObject& response);
            uint32_t GetWiFiSignalQuality(const JsonObject& parameters, JsonObject& response);
            uint32_t GetSupportedSecurityModes(const JsonObject& parameters, JsonObject& response);

            void onInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface);
            void onActiveInterfaceChange(const string prevActiveInterface, const string currentActiveinterface);
            void onIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status);
            void onInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState);
            void onAvailableSSIDs(const string jsonOfScanResults);
            void onWiFiStateChange(const Exchange::INetworkManager::WiFiState state);
            void onWiFiSignalQualityChange(const string ssid, const string strength, const string noise, const string snr, const Exchange::INetworkManager::WiFiSignalQuality quality);

        private:
            uint32_t _connectionId;
            PluginHost::IShell *_service;
            PluginHost::IPlugin* _networkManagerImpl;
            Exchange::INetworkManager *_networkManager;
            Core::Sink<Notification> _notification;
            string m_publicIPAddress;
            string m_publicIPAddressType;
        };
    }
}

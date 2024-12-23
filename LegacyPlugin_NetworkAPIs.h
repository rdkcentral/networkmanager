#pragma once

#include <string>
#include <atomic>

#include "Module.h"
#include "core/Link.h"
#include "NetworkManagerTimer.h"
#include "INetworkManager.h"

namespace WPEFramework {
    namespace Plugin {
        // This is a server for a JSONRPC communication channel.
        // For a plugin to be capable to handle JSONRPC, inherit from PluginHost::JSONRPC.
        // By inheriting from this class, the plugin realizes the interface PluginHost::IDispatcher.
        // This realization of this interface implements, by default, the following methods on this plugin
        // - exists
        // - register
        // - unregister
        // Any other methood to be handled by this plugin  can be added can be added by using the
        // templated methods Register on the PluginHost::JSONRPC class.
        // As the registration/unregistration of notifications is realized by the class PluginHost::JSONRPC,
        // this class exposes a public method called, Notify(), using this methods, all subscribed clients
        // will receive a JSONRPC message as a notification, in case this method is called.
        class Network : public PluginHost::IPlugin, public PluginHost::JSONRPC
        {
        private:
            // We do not allow this plugin to be copied !!
            Network(const Network&) = delete;
            Network& operator=(const Network&) = delete;

            void registerLegacyMethods(void);
            void unregisterLegacyMethods(void);
            void activatePrimaryPlugin();
            uint32_t internalGetIPSettings(const JsonObject& parameters, JsonObject& response);
            string getInterfaceNameToType(const string & interface);
            string getInterfaceTypeToName(const string & interface);

            /* Methods */
            uint32_t getInterfaces(const JsonObject& parameters, JsonObject& response);
            uint32_t isInterfaceEnabled(const JsonObject& parameters, JsonObject& response);
            uint32_t setInterfaceEnabled(const JsonObject& parameters, JsonObject& response);
            uint32_t getDefaultInterface(const JsonObject& parameters, JsonObject& response);
            uint32_t setDefaultInterface(const JsonObject& parameters, JsonObject& response);
            uint32_t doTrace(const JsonObject& parameters, JsonObject& response);
            uint32_t doPing(const JsonObject& parameters, JsonObject& response);
            uint32_t setIPSettings(const JsonObject& parameters, JsonObject& response);
            uint32_t getIPSettings(const JsonObject& parameters, JsonObject& response);
            uint32_t getIPSettings2(const JsonObject& parameters, JsonObject& response);
            uint32_t isConnectedToInternet(const JsonObject& parameters, JsonObject& response);
            uint32_t setConnectivityTestEndpoints(const JsonObject& parameters, JsonObject& response);
            uint32_t getInternetConnectionState(const JsonObject& parameters, JsonObject& response);
            uint32_t startConnectivityMonitoring(const JsonObject& parameters, JsonObject& response);
            uint32_t getCaptivePortalURI(const JsonObject& parameters, JsonObject& response);
            uint32_t stopConnectivityMonitoring(const JsonObject& parameters, JsonObject& response);
            uint32_t getPublicIP(const JsonObject& parameters, JsonObject& response);
            uint32_t setStunEndPoint(const JsonObject& parameters, JsonObject& response);
            uint32_t getStbIp(const JsonObject& parameters, JsonObject& response);
            uint32_t getSTBIPFamily(const JsonObject& parameters, JsonObject& response);

            /* Events */
            void onInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface);
            void onActiveInterfaceChange(const string prevActiveInterface, const string currentActiveinterface);
            void onIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status);
            void onInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState);

        private:
            class Notification : public Exchange::INetworkManager::INotification
            {
            private:
                Notification() = delete;
                Notification(const Notification &) = delete;
                Notification &operator=(const Notification &) = delete;

            public:
                explicit Notification(Network *parent)
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
                    return;
                }

                void onWiFiStateChange(const Exchange::INetworkManager::WiFiState state) override
                {
                    return;
                }

                void onWiFiSignalStrengthChange(const string ssid, const string strength, const Exchange::INetworkManager::WiFiSignalQuality quality) override
                {
                    return;
                }

                // Build QueryInterface implementation, specifying all possible interfaces we implement
                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::INetworkManager::INotification)
                END_INTERFACE_MAP

            private:
                Network &_parent;
            };

        public:
            Network();
            ~Network();
            //Build QueryInterface implementation, specifying all possible interfaces to be returned.
            BEGIN_INTERFACE_MAP(Network)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            END_INTERFACE_MAP
            
            void ReportonInterfaceStateChange(const JsonObject& parameters);
            void ReportonActiveInterfaceChange(const JsonObject& parameters);
            void ReportonIPAddressChange(const JsonObject& parameters);
            void ReportonInternetStatusChange(const JsonObject& parameters);

            //IPlugin methods
            virtual const std::string Initialize(PluginHost::IShell* service) override;
            virtual void Deinitialize(PluginHost::IShell* service) override;
            virtual std::string Information() const override;

        private:
            PluginHost::IShell* m_service;
            Core::Sink<Notification> _notification;
        };
    } // namespace Plugin
} // namespace WPEFramework

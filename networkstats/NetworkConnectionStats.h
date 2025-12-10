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
#include <interfaces/IConfiguration.h>

namespace WPEFramework {
    namespace Plugin {
        // Internal-only plugin - no external APIs exposed
        class NetworkConnectionStats : public PluginHost::IPlugin {
        private:
            class Notification : public RPC::IRemoteConnection::INotification {
            private:
                Notification() = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(const Notification&) = delete;

            public:
                explicit Notification(NetworkConnectionStats* parent) 
                    : _parent(*parent) {
                    ASSERT(parent != nullptr);
                }

                virtual ~Notification() {}

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

                void Activated(RPC::IRemoteConnection*) override {
                    TRACE(Trace::Information, (_T("NetworkConnectionStats Activated")));
                }

                void Deactivated(RPC::IRemoteConnection* connection) override {
                    TRACE(Trace::Information, (_T("NetworkConnectionStats Deactivated")));
                    _parent.Deactivated(connection);
                }

            private:
                NetworkConnectionStats& _parent;
            };

        public:
            NetworkConnectionStats(const NetworkConnectionStats&) = delete;
            NetworkConnectionStats& operator=(const NetworkConnectionStats&) = delete;  
            NetworkConnectionStats();
            virtual ~NetworkConnectionStats();

            BEGIN_INTERFACE_MAP(NetworkConnectionStats)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_AGGREGATE(Exchange::INetworkConnectionStats, _networkStats)
            END_INTERFACE_MAP

            // IPlugin methods
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private:
            PluginHost::IShell* _service{};
            uint32_t _connectionId{};
            Exchange::INetworkConnectionStats* _networkStats{};
            Core::Sink<Notification> _notification;
            Exchange::IConfiguration* _configure{};
        };
    } // namespace Plugin
} // namespace WPEFramework

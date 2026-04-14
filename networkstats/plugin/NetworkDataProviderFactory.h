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

#include "INetworkData.h"
#include "ThunderComRPCProvider.h"
#include "ThunderJsonRPCProvider.h"
#include <string>
#include <algorithm>
#include <cctype>

namespace WPEFramework {
namespace Plugin {

    /**
     * @brief Factory class for creating network data providers
     * 
     * This factory creates the appropriate provider instance based on configuration.
     * Supports both COM-RPC and JSON-RPC providers.
     */
    class NetworkDataProviderFactory {
    public:
        enum class ProviderType {
            COMRPC,
            JSONRPC,
            UNKNOWN
        };

        /**
         * @brief Create a network data provider based on type string
         * @param providerType String identifying the provider type ("comrpc" or "jsonrpc")
         * @param service PluginHost::IShell pointer forwarded to the provider's Initialize()
         * @return Pointer to INetworkData implementation, or nullptr on failure
         */
        static INetworkData* CreateProvider(const std::string& providerType, WPEFramework::PluginHost::IShell* service) {
            ProviderType type = ParseProviderType(providerType);
            return CreateProvider(type, service);
        }

        /**
         * @brief Create a network data provider based on enum type
         * @param type ProviderType enum value
         * @param service PluginHost::IShell pointer forwarded to the provider's Init
         * @return Pointer to INetworkData implementation, or nullptr on failure
         */
        static INetworkData* CreateProvider(ProviderType type, WPEFramework::PluginHost::IShell* service) {
            INetworkData* provider = nullptr;

            switch (type) {
                case ProviderType::COMRPC:
                    provider = new NetworkComRPCProvider();
                    break;
                    
                case ProviderType::JSONRPC:
                    provider = new NetworkJsonRPCProvider();
                    break;
                    
                case ProviderType::UNKNOWN:
                default:
                    // Return nullptr for unknown types
                    break;
            }

            (void)service; // forwarded to Initialize() by the caller
            return provider;
        }

        /**
         * @brief Parse provider type string to enum
         * @param providerTypeStr String to parse (case-insensitive)
         * @return ProviderType enum value
         */
        static ProviderType ParseProviderType(const std::string& providerTypeStr) {
            std::string lowerType = providerTypeStr;
            std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
                         [](unsigned char c) { return std::tolower(c); });

            if (lowerType == "comrpc" || lowerType == "com-rpc") {
                return ProviderType::COMRPC;
            } else if (lowerType == "jsonrpc" || lowerType == "json-rpc") {
                return ProviderType::JSONRPC;
            } else {
                return ProviderType::UNKNOWN;
            }
        }

        /**
         * @brief Get string representation of provider type
         * @param type ProviderType enum value
         * @return String name of the provider type
         */
        static std::string GetProviderTypeName(ProviderType type) {
            switch (type) {
                case ProviderType::COMRPC:
                    return "COM-RPC";
                case ProviderType::JSONRPC:
                    return "JSON-RPC";
                case ProviderType::UNKNOWN:
                default:
                    return "Unknown";
            }
        }
    };

} // namespace Plugin
} // namespace WPEFramework

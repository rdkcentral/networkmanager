/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
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

// Ping/Trace callback adapters shared by the NetworkManager and legacy Network plugins.

#include "DeferredReply.h"
#include "NetworkManagerDiagnosticsJson.h"

namespace WPEFramework
{
    namespace Plugin
    {
        namespace Diagnostics
        {
            // Adds the plugin-specific fields to a successful result and logs it; may be nullptr.
            using Decorator = void (*)(JsonObject& response);

            class PingReply : public DeferredReply::CallbackReply<Exchange::INetworkManager::IPingCallback>
            {
            public:
                PingReply(DeferredReply::PendingReply&& reply, const string& endpoint, const Decorator decorate)
                    : CallbackReply(std::move(reply)), _endpoint(endpoint), _decorate(decorate) {}

                void Completed(const uint32_t result, const Exchange::INetworkManager::PingResult& ping) override
                {
                    if (result != Core::ERROR_NONE)
                    {
                        _reply.Fail(result);
                        return;
                    }
                    JsonObject response = PingJson(ping, _endpoint);
                    if (_decorate != nullptr)
                        _decorate(response);
                    string json;
                    response.ToString(json);
                    _reply.Respond(json);
                }

            private:
                const string _endpoint;
                const Decorator _decorate;
            };

            class TraceReply : public DeferredReply::CallbackReply<Exchange::INetworkManager::ITraceCallback>
            {
            public:
                TraceReply(DeferredReply::PendingReply&& reply, const string& endpoint, const Decorator decorate)
                    : CallbackReply(std::move(reply)), _endpoint(endpoint), _decorate(decorate) {}

                void Completed(const uint32_t result, const string& output) override
                {
                    if (result != Core::ERROR_NONE)
                    {
                        _reply.Fail(result);
                        return;
                    }
                    JsonObject response = TraceJson(output, _endpoint);
                    if (_decorate != nullptr)
                        _decorate(response);
                    string json;
                    response.ToString(json);
                    _reply.Respond(json);
                }

            private:
                const string _endpoint;
                const Decorator _decorate;
            };
        }
    }
}

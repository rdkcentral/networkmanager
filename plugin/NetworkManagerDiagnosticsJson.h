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

// The Ping/Trace result JSON seen by JSON-RPC callers and returned by the deprecated sync
// INetworkManager::Ping/Trace. Key order and value text must never change.

#include "Module.h"
#include "INetworkManager.h"

namespace WPEFramework
{
    namespace Plugin
    {
        namespace Diagnostics
        {
            inline JsonObject PingJson(const Exchange::INetworkManager::PingResult& ping, const string& endpoint)
            {
                JsonObject response;
                if (ping.countsReported)
                {
                    response["packetsTransmitted"] = ping.packetsTransmitted;
                    response["packetsReceived"] = ping.packetsReceived;
                    response["packetLoss"] = ping.packetLoss;
                }
                if (!ping.tripMin.empty())
                    response["tripMin"] = ping.tripMin;
                if (!ping.tripAvg.empty())
                    response["tripAvg"] = ping.tripAvg;
                if (!ping.tripMax.empty())
                    response["tripMax"] = ping.tripMax;
                if (!ping.tripStdDev.empty())
                    response["tripStdDev"] = ping.tripStdDev;
                response["success"] = ping.success;
                if (!ping.error.empty())
                    response["error"] = ping.error;
                response["endpoint"] = endpoint;
                return response;
            }

            // "results" is a string holding a JSON array with one element per line; each newline-terminated line keeps a trailing space.
            inline JsonObject TraceJson(const string& output, const string& endpoint)
            {
                JsonArray list;
                size_t start = 0;
                while (start < output.size())
                {
                    const size_t newline = output.find('\n', start);
                    if (newline == string::npos)
                    {
                        list.Add(output.substr(start));
                        break;
                    }
                    list.Add(output.substr(start, newline - start) + " ");
                    start = newline + 1;
                }

                string lines;
                list.ToString(lines);

                JsonObject response;
                response["endpoint"] = endpoint;
                response["results"] = lines;
                return response;
            }
        }
    }
}

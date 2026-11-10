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

#include "Module.h"
#include "INetworkManager.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace WPEFramework
{
    namespace Plugin
    {
        namespace Diagnostics
        {
            struct ProcessResult
            {
                bool started = false;
                bool timedOut = false;
                int status = -1;        // waitpid() status
                std::string output;     // stdout and stderr, merged

                bool Succeeded() const;
            };

            class IProcessRunner
            {
            public:
                virtual ~IProcessRunner() = default;
                // Runs argv (no shell) until it exits, the deadline passes or cancel is set; then kills its process group.
                virtual ProcessResult Run(const std::vector<std::string>& argv, const std::chrono::milliseconds deadline, const std::atomic<bool>& cancel) = 0;
            };

            class ProcessRunner : public IProcessRunner
            {
            public:
                ProcessResult Run(const std::vector<std::string>& argv, const std::chrono::milliseconds deadline, const std::atomic<bool>& cancel) override;
            };

            bool IsValidEndpoint(const std::string& endpoint);

            std::vector<std::string> PingArgs(const bool ipv6, const uint32_t count, const uint16_t timeout, const std::string& endpoint);
            std::vector<std::string> TraceArgs(const bool ipv6, const uint32_t nqueries, const std::string& endpoint);
            std::chrono::milliseconds PingDeadline(const uint32_t count, const uint16_t timeout);
            std::chrono::milliseconds TraceDeadline(const uint32_t nqueries);

            // Parses ping's summary; the INetworkManager::Ping/Trace "response" JSON is built in NetworkManagerDiagnosticsJson.h.
            Exchange::INetworkManager::PingResult ParsePing(const ProcessResult& result);

            // One worker and one bounded FIFO per kind: at most one Ping and one Trace child run at a time.
            class Engine
            {
            public:
                enum class Kind : uint8_t { PING = 0, TRACE = 1 };

                struct Job
                {
                    std::vector<std::string> argv;
                    std::chrono::milliseconds deadline{0};
                    std::function<void(const ProcessResult&)> onDone;
                    std::function<void()> onDrop;   // engine stopped before the job finished
                };

                static constexpr size_t MaxQueued = 4;

                explicit Engine(std::shared_ptr<IProcessRunner> runner);
                ~Engine();

                Engine(const Engine&) = delete;
                Engine& operator=(const Engine&) = delete;

                // ERROR_NONE, ERROR_INPROGRESS (queue full) or ERROR_UNAVAILABLE (stopped).
                uint32_t Submit(const Kind kind, Job&& job);
                void Stop();

            private:
                struct Lane
                {
                    std::mutex lock;
                    std::condition_variable signal;
                    std::deque<Job> queue;
                    std::thread worker;
                };

                void Work(Lane& lane);

                std::shared_ptr<IProcessRunner> _runner;
                std::atomic<bool> _stop;
                Lane _lanes[2];
            };
        }
    }
}

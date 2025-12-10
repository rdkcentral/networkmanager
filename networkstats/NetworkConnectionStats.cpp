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

#include "NetworkConnectionStats.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework {

    namespace {
        static Plugin::Metadata<Plugin::NetworkConnectionStats> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            { PluginHost::ISubSystem::PLATFORM },
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin {

    /*
     * Register NetworkConnectionStats module as WPEFramework plugin
     * This plugin runs internally without exposing external APIs
     */
    SERVICE_REGISTRATION(NetworkConnectionStats, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    NetworkConnectionStats::NetworkConnectionStats() 
        : _service(nullptr)
        , _connectionId(0)
        , _networkStats(nullptr)
        , _notification(this)
        , _configure(nullptr)
    {
        TRACE(Trace::Information, (_T("NetworkConnectionStats Constructor")));
    }

    NetworkConnectionStats::~NetworkConnectionStats()
    {
        TRACE(Trace::Information, (_T("NetworkConnectionStats Destructor")));
    }

    const string NetworkConnectionStats::Initialize(PluginHost::IShell* service)
    {
        string message;

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _networkStats);
        ASSERT(0 == _connectionId);

        TRACE(Trace::Information, (_T("NetworkConnectionStats::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();
        _service->Register(&_notification);
        
        // Spawn out-of-process implementation
        _networkStats = _service->Root<Exchange::INetworkConnectionStats>(_connectionId, 5000, _T("NetworkConnectionStatsImplementation"));

        if (nullptr != _networkStats) {
            _configure = _networkStats->QueryInterface<Exchange::IConfiguration>();
            if (_configure != nullptr) {
                uint32_t result = _configure->Configure(_service);
                if (result != Core::ERROR_NONE) {
                    message = _T("NetworkConnectionStats could not be configured");
                    TRACE(Trace::Error, (_T("Configuration failed")));
                } else {
                    TRACE(Trace::Information, (_T("NetworkConnectionStats configured successfully - running in background")));
                }
            } else {
                message = _T("NetworkConnectionStats implementation did not provide a configuration interface");
                TRACE(Trace::Error, (_T("No configuration interface")));
            }
        } else {
            TRACE(Trace::Error, (_T("NetworkConnectionStats::Initialize: Failed to initialise plugin")));
            message = _T("NetworkConnectionStats plugin could not be initialised");
        }
        
        return message;
    }

    void NetworkConnectionStats::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        TRACE(Trace::Information, (_T("NetworkConnectionStats::Deinitialize")));

        // Unregister from notifications
        _service->Unregister(&_notification);

        if (nullptr != _networkStats) {
            if (_configure != nullptr) {
                _configure->Release();
                _configure = nullptr;
            }

            // Stop out-of-process implementation
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _networkStats->Release();

            _networkStats = nullptr;

            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            if (nullptr != connection) {
                connection->Terminate();
                connection->Release();
            }
        }

        _connectionId = 0;
        _service->Release();
        _service = nullptr;
        TRACE(Trace::Information, (_T("NetworkConnectionStats de-initialised")));
    }

    string NetworkConnectionStats::Information() const
    {
        return ("Internal network diagnostics and monitoring plugin - no external APIs");
    }

    void NetworkConnectionStats::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {
            ASSERT(nullptr != _service);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, 
                PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework

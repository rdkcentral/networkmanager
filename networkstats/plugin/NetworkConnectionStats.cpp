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
#include "NetworkConnectionStatsLogger.h"

using namespace NetworkConnectionStatsLogger;

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework {

    namespace {
        static Plugin::Metadata<Plugin::NetworkConnectionStats> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            { PluginHost::ISubSystem::subsystem::PLATFORM },
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
    {
        NSLOG_INFO("NetworkConnectionStats Constructor");
    }

    NetworkConnectionStats::~NetworkConnectionStats()
    {
        NSLOG_INFO("NetworkConnectionStats Destructor");
    }

    const string NetworkConnectionStats::Initialize(PluginHost::IShell* service)
    {
        string message;

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _networkStats);
        ASSERT(0 == _connectionId);

        SYSLOG(Logging::Startup, (_T("Initializing NetworkConnectionStats")));
        NetworkConnectionStatsLogger::Init();
        
        // Register the Connection::Notification first
        _service = service;
        _service->Register(&_notification);

        // Spawn out-of-process implementation
        _networkStats = _service->Root<Exchange::INetworkConnectionStats>(_connectionId, 25000, _T("NetworkConnectionStatsImplementation"));
        
        NSLOG_INFO("NetworkConnectionStats::Initialize: PID=%u, ConnectionID=%u", getpid(), _connectionId);

        if (nullptr != _networkStats) {
            SYSLOG(Logging::Startup, (_T("Configuring NetworkConnectionStats")));
            if (_networkStats->Configure(_service->ConfigLine()) != Core::ERROR_NONE)
            {
                SYSLOG(Logging::Shutdown, (_T("NetworkConnectionStats failed to configure")));
                message = _T("NetworkConnectionStats could not be configured");
            }
            else
            {
                SYSLOG(Logging::Startup, (_T("NetworkConnectionStats configured successfully")));
            }
        } else {
            // Something went wrong, clean up
            SYSLOG(Logging::Shutdown, (_T("NetworkConnectionStats out-of-process creation failed")));
            NSLOG_ERROR("Failed to initialize NetworkConnectionStats");
            _service->Unregister(&_notification);
            _service = nullptr;
            message = _T("NetworkConnectionStats plugin could not be initialised");
        }
        
        return message;
    }

    void NetworkConnectionStats::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);
        ASSERT(_networkStats != nullptr);

        SYSLOG(Logging::Shutdown, (_T("Deinitializing NetworkConnectionStats")));
        
        if (_networkStats != nullptr)
        {
            SYSLOG(Logging::Shutdown, (_T("Unregister Thunder Notifications for NetworkConnectionStats")));
            _service->Unregister(&_notification);

            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);

            SYSLOG(Logging::Shutdown, (_T("Release of COMRPC Interface of NetworkConnectionStats")));
            _networkStats->Release();
            
            if (connection != nullptr)
            {
                connection->Terminate();
                connection->Release();
            }
        }

        // Set everything back to default
        _connectionId = 0;
        _service = nullptr;
        _networkStats = nullptr;
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

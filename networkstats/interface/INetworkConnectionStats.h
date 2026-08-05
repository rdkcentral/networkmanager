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

namespace WPEFramework {
namespace Exchange {
    enum myIDs {
        ID_NETWORKCONNECTIONSTATS = 0x800004F0,
        ID_NETWORKCONNECTIONSTATS_NOTIFICATION = ID_NETWORKCONNECTIONSTATS + 1
    };

    // Interface for network connection statistics
    struct EXTERNAL INetworkConnectionStats : virtual public Core::IUnknown {
        enum { ID = ID_NETWORKCONNECTIONSTATS };

        // Configuration method
        virtual uint32_t Configure(const string configLine) = 0;

        /* @event */
        struct EXTERNAL INotification : virtual public Core::IUnknown {
            enum { ID = ID_NETWORKCONNECTIONSTATS_NOTIFICATION };

            // Placeholder for future notification events if needed
            // Currently no events are exposed as this is an internal plugin
        };

        // Allow other processes to register/unregister for notifications
        virtual uint32_t Register(INetworkConnectionStats::INotification* notification) = 0;
        virtual uint32_t Unregister(INetworkConnectionStats::INotification* notification) = 0;
    };

} // namespace Exchange
} // namespace WPEFramework

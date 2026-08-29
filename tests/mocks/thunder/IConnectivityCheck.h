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

/**
 * Minimal CI stub for IConnectivityCheck — compatible with Thunder R4.4.3.
 *
 * This file is used only during CI builds (rdk proxy L1/L2 tests) where the
 * full entservices-cpc-apis stack is not available. It defines exactly the
 * surface consumed by NetworkManagerConnectivityClient and nothing more.
 *
 * DO NOT use this file outside of test/CI contexts.
 */

#include <core/core.h>

namespace WPEFramework {
namespace Exchange {

    struct EXTERNAL IConnectivityCheck : virtual public Core::IUnknown {

        // Stub ID — not used for COM lookup in L1/L2 unit tests.
        enum { ID = 0x8190 };

        enum InternetStatus : uint8_t {
            NO_INTERNET,
            LIMITED_INTERNET,
            CAPTIVE_PORTAL,
            FULLY_CONNECTED,
            UNKNOWN,
        };

        struct EXTERNAL StatusInfo {
            InternetStatus status;
            string         reason;
            string         interface;
            string         ipversion;
        };

        struct EXTERNAL INotification : virtual public Core::IUnknown {
            enum { ID = 0x8191 };

            virtual void OnInternetStatusChange(const InternetStatus status, const string& reason) {}
            virtual void OnExternalProbeResult(const InternetStatus status, const string& reason,
                                               const string& captivePortalURI) {}
        };

        virtual Core::hresult GetInternetStatus(StatusInfo& info /* @out */) const {};
        virtual Core::hresult GetCaptivePortalURI(string& uri /* @out */) const {};

        virtual Core::hresult Register(INotification* notification) {};
        virtual Core::hresult Unregister(INotification* notification) {};
    };

} // namespace Exchange
} // namespace WPEFramework

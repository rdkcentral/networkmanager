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
 * Minimal CI stub for IPowerManager — compatible with Thunder R4.4.3.
 *
 * This file is used only during CI builds (gdbus/libnm L1 proxy tests) where
 * the full entservices-apis stack is not available.  It defines exactly the
 * surface consumed by NetworkManagerPowerClient and nothing more.
 *
 * DO NOT use this file outside of test/CI contexts.
 */

#include <core/core.h>

namespace WPEFramework {
namespace Exchange {

    struct EXTERNAL IPowerManager : virtual public Core::IUnknown {

        // Stub ID — not used for COM lookup in L1 unit tests.
        enum { ID = 0x8180 };

        enum PowerState : uint8_t {
            POWER_STATE_UNKNOWN              = 0,
            POWER_STATE_OFF                  = 1,
            POWER_STATE_STANDBY              = 2,
            POWER_STATE_ON                   = 3,
            POWER_STATE_STANDBY_LIGHT_SLEEP  = 4,
            POWER_STATE_STANDBY_DEEP_SLEEP   = 5,
        };

        // @event
        struct EXTERNAL IModePreChangeNotification : virtual public Core::IUnknown {
            enum { ID = 0x8182 };
            virtual void OnPowerModePreChange(const PowerState currentState,
                                              const PowerState newState,
                                              const int transactionId,
                                              const int stateChangeAfter) {}
        };

        // @event
        struct EXTERNAL IModeChangedNotification : virtual public Core::IUnknown {
            enum { ID = 0x8183 };
            virtual void OnPowerModeChanged(const PowerState currentState,
                                            const PowerState newState) {}
        };

        virtual Core::hresult Register(IModePreChangeNotification* notification) = 0;
        virtual Core::hresult Unregister(const IModePreChangeNotification* notification) = 0;

        virtual Core::hresult Register(IModeChangedNotification* notification) = 0;
        virtual Core::hresult Unregister(const IModeChangedNotification* notification) = 0;

        virtual Core::hresult AddPowerModePreChangeClient(const string& clientName,
                                                          uint32_t& clientId) = 0;
        virtual Core::hresult RemovePowerModePreChangeClient(const uint32_t clientId) = 0;
        virtual Core::hresult PowerModePreChangeComplete(const uint32_t clientId,
                                                         const int transactionId) = 0;
        virtual Core::hresult DelayPowerModeChangeBy(const uint32_t clientId,
                                                     const int transactionId,
                                                     const int delayPeriod) = 0;
        virtual Core::hresult GetNetworkStandbyMode(bool& standbyMode) = 0;
    };

} // namespace Exchange
} // namespace WPEFramework

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

/**
 * Stub implementation of NetworkManagerPowerClient for unit/L2 test builds.
 *
 * The real implementation requires WPEFramework COM-RPC infrastructure
 * (SmartInterfaceType::Open/Close) which is not exercised in test builds.
 * This stub satisfies the linker while keeping the test binary free of
 * live COM connections.
 */

#include "NetworkManagerPowerClient.h"

namespace WPEFramework {
namespace Plugin {

NetworkManagerPowerClient::NetworkManagerPowerClient(INetworkPowerCallback& callback)
    : mCallback(callback)
    , mPreChangeNotification(*this)
    , mChangedNotification(*this)
{
}

NetworkManagerPowerClient::~NetworkManagerPowerClient()
{
}

bool NetworkManagerPowerClient::IsValid() const { return false; }

bool NetworkManagerPowerClient::getNetworkStandbyMode() const { return false; }

void NetworkManagerPowerClient::sendPowerModePreChangeComplete(int /* transactionId */) {}

void NetworkManagerPowerClient::sendDelayPowerModeChange(int /* transactionId */, int /* seconds */) {}

void NetworkManagerPowerClient::Operational(bool /* upAndRunning */) {}

void NetworkManagerPowerClient::registerEvents() {}

void NetworkManagerPowerClient::unregisterEvents() {}

void NetworkManagerPowerClient::powerThreadLoop() {}

void NetworkManagerPowerClient::PreChangeNotification::OnPowerModePreChange(
    const PowerState /* currentState */, const PowerState /* newState */,
    const int /* transactionId */, const int /* stateChangeAfter */) {}

void NetworkManagerPowerClient::ChangedNotification::OnPowerModeChanged(
    const PowerState /* currentState */, const PowerState /* newState */) {}

} // namespace Plugin
} // namespace WPEFramework

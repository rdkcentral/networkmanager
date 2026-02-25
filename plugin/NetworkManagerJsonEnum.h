/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#include <core/Enumerate.h>
#include "INetworkManager.h"

// Forward declarations - actual definitions are in NetworkManagerJsonEnum.cpp
namespace WPEFramework {
namespace Core {
    template<> const EnumerateConversion<Exchange::INetworkManager::InterfaceType>* EnumerateType<Exchange::INetworkManager::InterfaceType>::Table(const uint16_t);
    template<> const EnumerateConversion<Exchange::INetworkManager::IPVersion>* EnumerateType<Exchange::INetworkManager::IPVersion>::Table(const uint16_t);
    template<> const EnumerateConversion<Exchange::INetworkManager::InternetStatus>* EnumerateType<Exchange::INetworkManager::InternetStatus>::Table(const uint16_t);
    template<> const EnumerateConversion<Exchange::INetworkManager::WiFiWPS>* EnumerateType<Exchange::INetworkManager::WiFiWPS>::Table(const uint16_t);
    template<> const EnumerateConversion<Exchange::INetworkManager::WiFiState>* EnumerateType<Exchange::INetworkManager::WiFiState>::Table(const uint16_t);
    template<> const EnumerateConversion<Exchange::INetworkManager::WiFiSignalQuality>* EnumerateType<Exchange::INetworkManager::WiFiSignalQuality>::Table(const uint16_t);
    template<> const EnumerateConversion<Exchange::INetworkManager::Logging>* EnumerateType<Exchange::INetworkManager::Logging>::Table(const uint16_t);
    template<> const EnumerateConversion<Exchange::INetworkManager::InterfaceState>* EnumerateType<Exchange::INetworkManager::InterfaceState>::Table(const uint16_t);
    template<> const EnumerateConversion<Exchange::INetworkManager::IPStatus>* EnumerateType<Exchange::INetworkManager::IPStatus>::Table(const uint16_t);
}
}

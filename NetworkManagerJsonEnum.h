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

namespace WPEFramework {

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::InterfaceType)
    { Exchange::INetworkManager::InterfaceType::INTERFACE_TYPE_ETHERNET, _TXT("ETHERNET") },
    { Exchange::INetworkManager::InterfaceType::INTERFACE_TYPE_WIFI, _TXT("WIFI") },
    { Exchange::INetworkManager::InterfaceType::INTERFACE_TYPE_P2P, _TXT("P2P") },
ENUM_CONVERSION_END(Exchange::INetworkManager::InterfaceType)

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::IPVersion)
    { Exchange::INetworkManager::IPVersion::IP_ADDRESS_V4, _TXT("IPv4") },
    { Exchange::INetworkManager::IPVersion::IP_ADDRESS_V6, _TXT("IPv6") },
ENUM_CONVERSION_END(Exchange::INetworkManager::IPVersion)

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::InternetStatus)
    { Exchange::INetworkManager::InternetStatus::INTERNET_NOT_AVAILABLE, _TXT("NO_INTERNET") },
    { Exchange::INetworkManager::InternetStatus::INTERNET_LIMITED, _TXT("LIMITED_INTERNET") },
    { Exchange::INetworkManager::InternetStatus::INTERNET_CAPTIVE_PORTAL, _TXT("CAPTIVE_PORTAL") },
    { Exchange::INetworkManager::InternetStatus::INTERNET_FULLY_CONNECTED, _TXT("FULLY_CONNECTED") },
    { Exchange::INetworkManager::InternetStatus::INTERNET_UNKNOWN, _TXT("UNKNOWN") },
ENUM_CONVERSION_END(Exchange::INetworkManager::InternetStatus)

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::WiFiWPS)
    { Exchange::INetworkManager::WiFiWPS::WIFI_WPS_PBC, _TXT("PBC") },
    { Exchange::INetworkManager::WiFiWPS::WIFI_WPS_PIN, _TXT("PIN") },
    { Exchange::INetworkManager::WiFiWPS::WIFI_WPS_SERIALIZED_PIN, _TXT("SERIALIZED_PIN") },
ENUM_CONVERSION_END(Exchange::INetworkManager::WiFiWPS)

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::WiFiState)
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_UNINSTALLED, _TXT("WIFI_STATE_UNINSTALLED") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_DISABLED, _TXT("WIFI_STATE_DISABLED") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_DISCONNECTED, _TXT("WIFI_STATE_DISCONNECTED") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_PAIRING, _TXT("WIFI_STATE_PAIRING") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTING, _TXT("WIFI_STATE_CONNECTING") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTED, _TXT("WIFI_STATE_CONNECTED") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_SSID_NOT_FOUND, _TXT("WIFI_STATE_SSID_NOT_FOUND") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_SSID_CHANGED, _TXT("WIFI_STATE_SSID_CHANGED") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTION_LOST, _TXT("WIFI_STATE_CONNECTION_LOST") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTION_FAILED, _TXT("WIFI_STATE_CONNECTION_FAILED") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTION_INTERRUPTED, _TXT("WIFI_STATE_CONNECTION_INTERRUPTED") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_INVALID_CREDENTIALS, _TXT("WIFI_STATE_INVALID_CREDENTIALS") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_AUTHENTICATION_FAILED, _TXT("WIFI_STATE_AUTHENTICATION_FAILED") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_ERROR, _TXT("WIFI_STATE_ERROR") },
    { Exchange::INetworkManager::WiFiState::WIFI_STATE_INVALID, _TXT("WIFI_STATE_INVALID") },
ENUM_CONVERSION_END(Exchange::INetworkManager::WiFiState)

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::WiFiSignalQuality)
    { Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_DISCONNECTED, _TXT("Disconnected") },
    { Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_WEAK, _TXT("Weak") },
    { Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_FAIR, _TXT("Fair") },
    { Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_GOOD, _TXT("Good") },
    { Exchange::INetworkManager::WiFiSignalQuality::WIFI_SIGNAL_EXCELLENT, _TXT("Excellent") },
ENUM_CONVERSION_END(Exchange::INetworkManager::WiFiSignalQuality)

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::Logging)
    { Exchange::INetworkManager::Logging::LOG_LEVEL_FATAL, _TXT("LOG_LEVEL_FATAL") },
    { Exchange::INetworkManager::Logging::LOG_LEVEL_ERROR, _TXT("LOG_LEVEL_ERROR") },
    { Exchange::INetworkManager::Logging::LOG_LEVEL_WARNING, _TXT("LOG_LEVEL_WARNING") },
    { Exchange::INetworkManager::Logging::LOG_LEVEL_INFO, _TXT("LOG_LEVEL_INFO") },
    { Exchange::INetworkManager::Logging::LOG_LEVEL_DEBUG, _TXT("LOG_LEVEL_DEBUG") },
ENUM_CONVERSION_END(Exchange::INetworkManager::Logging)

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::InterfaceState)
    { Exchange::INetworkManager::InterfaceState::INTERFACE_ADDED, _TXT("INTERFACE_ADDED") },
    { Exchange::INetworkManager::InterfaceState::INTERFACE_LINK_UP, _TXT("INTERFACE_LINK_UP") },
    { Exchange::INetworkManager::InterfaceState::INTERFACE_LINK_DOWN, _TXT("INTERFACE_LINK_DOWN") },
    { Exchange::INetworkManager::InterfaceState::INTERFACE_ACQUIRING_IP, _TXT("INTERFACE_ACQUIRING_IP") },
    { Exchange::INetworkManager::InterfaceState::INTERFACE_REMOVED, _TXT("INTERFACE_REMOVED") },
    { Exchange::INetworkManager::InterfaceState::INTERFACE_DISABLED, _TXT("INTERFACE_DISABLED") },
ENUM_CONVERSION_END(Exchange::INetworkManager::InterfaceState)

ENUM_CONVERSION_BEGIN(Exchange::INetworkManager::IPStatus)
    { Exchange::INetworkManager::IPStatus::IP_LOST, _TXT("LOST") },
    { Exchange::INetworkManager::IPStatus::IP_ACQUIRED, _TXT("ACQUIRED") },
ENUM_CONVERSION_END(Exchange::INetworkManager::IPStatus)

}

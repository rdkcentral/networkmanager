/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#include <gmock/gmock.h>
#include "Module.h"

class MockINetworkManager : public WPEFramework::Exchange::INetworkManager {
public:
    MOCK_METHOD(uint32_t, GetAvailableInterfaces, (WPEFramework::Exchange::INetworkManager::IInterfaceDetailsIterator*& interfaces), (override));
    MOCK_METHOD(uint32_t, GetPrimaryInterface, (string& interface), (override));                                              MOCK_METHOD(uint32_t, SetInterfaceState, (const string& interface, const bool enabled), (override));
    MOCK_METHOD(uint32_t, GetInterfaceState, (const string& interface, bool& enabled), (override));
    MOCK_METHOD(uint32_t, GetIPSettings, (string& interface, const string& ipversion, IPAddress& address), (override));
    MOCK_METHOD(uint32_t, SetIPSettings, (const string& interface, const IPAddress& address), (override));
    MOCK_METHOD(uint32_t, GetStunEndpoint, (string& endpoint, uint32_t& port, uint32_t& timeout, uint32_t& cacheLifetime), (const));
    MOCK_METHOD(uint32_t, SetStunEndpoint, (string const endpoint, const uint32_t port, const uint32_t timeout, const uint32_t cacheLifetime), (override));
    MOCK_METHOD(uint32_t, GetConnectivityTestEndpoints, (IStringIterator*& endpoints), (const));
    MOCK_METHOD(uint32_t, SetConnectivityTestEndpoints, (IStringIterator* const endpoints), (override));
    MOCK_METHOD(uint32_t, IsConnectedToInternet, (string& ipversion, string& interface, WPEFramework::Exchange::INetworkManager::InternetStatus& status), (override));
    MOCK_METHOD(uint32_t, GetCaptivePortalURI, (string& uri), (const));
    MOCK_METHOD(uint32_t, GetPublicIP, (string& interface, string& ipversion, string& ipaddress), (override));
    MOCK_METHOD(uint32_t, Ping, (const string ipversion, const string endpoint, const uint32_t count, const uint16_t timeout, const string guid, string& response), (override));
    MOCK_METHOD(uint32_t, Trace, (const string ipversion, const string endpoint, const uint32_t nqueries, const string guid, string& response), (override));
    MOCK_METHOD(uint32_t, StartWiFiScan, (const string& frequency, IStringIterator* const ssids), (override));
    MOCK_METHOD(uint32_t, StopWiFiScan, (), (override));
    MOCK_METHOD(uint32_t, GetKnownSSIDs, (IStringIterator*& ssids), (override));
    MOCK_METHOD(uint32_t, AddToKnownSSIDs, (const WiFiConnectTo& ssid), (override));
    MOCK_METHOD(uint32_t, RemoveKnownSSID, (const string& ssid), (override));
    MOCK_METHOD(uint32_t, WiFiConnect, (const WiFiConnectTo& ssid), (override));
    MOCK_METHOD(uint32_t, WiFiDisconnect, (), (override));
    MOCK_METHOD(uint32_t, GetConnectedSSID, (WiFiSSIDInfo& ssidInfo), (override));
    MOCK_METHOD(uint32_t, StartWPS, (const WiFiWPS& method, const string& pin), (override));
    MOCK_METHOD(uint32_t, StopWPS, (), (override));
    MOCK_METHOD(uint32_t, GetWifiState, (WPEFramework::Exchange::INetworkManager::WiFiState& state), (override));
    MOCK_METHOD(uint32_t, GetWiFiSignalQuality, (string& ssid, string& strength, string& noise, string& snr, WPEFramework::Exchange::INetworkManager::WiFiSignalQuality& quality), (override));
    MOCK_METHOD(uint32_t, GetSupportedSecurityModes, (ISecurityModeIterator*& modes), (const));
    MOCK_METHOD(uint32_t, SetLogLevel, (const Logging& level), (override));
    MOCK_METHOD(uint32_t, GetLogLevel, (Logging& level), (override));
    MOCK_METHOD(uint32_t, Configure, (WPEFramework::PluginHost::IShell* service), (override));
    MOCK_METHOD(uint32_t, Register, (WPEFramework::Exchange::INetworkManager::INotification* notification), (override));
    MOCK_METHOD(uint32_t, Unregister, (WPEFramework::Exchange::INetworkManager::INotification* notification), (override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (uint32_t), (override));
};

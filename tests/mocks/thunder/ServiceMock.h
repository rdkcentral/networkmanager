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

#ifndef SERVICEMOCK_H
#define SERVICEMOCK_H

#include <gmock/gmock.h>

#include "LegacyNetworkAPIs.h"
#include "Module.h"

class ServiceMock : public WPEFramework::PluginHost::IShell {
public:
    virtual ~ServiceMock() = default;

    enum { ID = WPEFramework::RPC::ID_SHELL };
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(string, Versions, (), (const, override));
    MOCK_METHOD(string, Locator, (), (const, override));
    MOCK_METHOD(string, ClassName, (), (const, override));
    MOCK_METHOD(string, Callsign, (), (const, override));
    MOCK_METHOD(string, WebPrefix, (), (const, override));
    MOCK_METHOD(string, ConfigLine, (), (const, override));
    MOCK_METHOD(string, PersistentPath, (), (const, override));
    MOCK_METHOD(string, VolatilePath, (), (const, override));
    MOCK_METHOD(string, DataPath, (), (const, override));
    MOCK_METHOD(state, State, (), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, Activate, (const reason), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Deactivate, (const reason), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Unavailable, (const reason), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, ConfigLine, (const string& config), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SystemRootPath, (const string& systemRootPath), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Hibernate, (const uint32_t timeout), (override));
    MOCK_METHOD(string, SystemPath, (), (const, override));
    MOCK_METHOD(string, PluginPath, (), (const, override));
    MOCK_METHOD(WPEFramework::PluginHost::IShell::startup, Startup, (), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, Startup, (const startup value), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Resumed, (const bool value), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Metadata, (string& info), (const, override));

    MOCK_METHOD(bool, Resumed, (), (const, override));
    MOCK_METHOD(bool, IsSupported, (const uint8_t), (const, override));
    MOCK_METHOD(void, EnableWebServer, (const string&, const string&), (override));
    MOCK_METHOD(void, DisableWebServer, (), (override));
    MOCK_METHOD(WPEFramework::PluginHost::ISubSystem*, SubSystems, (), (override));
    MOCK_METHOD(uint32_t, Submit, (const uint32_t, const WPEFramework::Core::ProxyType<WPEFramework::Core::JSON::IElement>&), (override));
    MOCK_METHOD(void, Notify, (const string&), (override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t), (override));
    MOCK_METHOD(void*, QueryInterfaceByCallsign, (const uint32_t, const string&), (override));
    MOCK_METHOD(void, Register, (WPEFramework::PluginHost::IPlugin::INotification*), (override));
    MOCK_METHOD(void, Unregister, (WPEFramework::PluginHost::IPlugin::INotification*), (override));
    MOCK_METHOD(string, Model, (), (const, override));
    MOCK_METHOD(bool, Background, (), (const, override));
    MOCK_METHOD(string, Accessor, (), (const, override));
    MOCK_METHOD(string, ProxyStubPath, (), (const, override));
    MOCK_METHOD(string, HashKey, (), (const, override));
    MOCK_METHOD(string, Substitute, (const string&), (const, override));
    MOCK_METHOD(WPEFramework::PluginHost::IShell::ICOMLink*, COMLink, (), (override));
    MOCK_METHOD(reason, Reason, (), (const, override));
    MOCK_METHOD(string, SystemRootPath, (), (const, override));
};

class MockIAuthenticate : public WPEFramework::PluginHost::IAuthenticate {
public:
    MOCK_METHOD(WPEFramework::PluginHost::ISecurity*, Officer, (const string&), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, CreateToken, (const uint16_t, const uint8_t*, std::string&), (override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t), (override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
};

/* straight mock methods for the SystemInfo class is because it has a private constructor and assignment operator */
class MockSystemInfo {
public:
    virtual bool SetEnvironment(const string& name, const TCHAR* value, const bool forced = true) = 0;
};

class MockSystemInfoImpl : public MockSystemInfo {
public:
    MOCK_METHOD(bool, SetEnvironment, (const string&, const TCHAR*, const bool), (override));
};

template <typename INBOUND, typename METHOD>
class MockSmartLinkType : public WPEFramework::JSONRPC::SmartLinkType<WPEFramework::Core::JSON::IElement> {
public:
    MockSmartLinkType(const string& remoteCallsign, const TCHAR* localCallsign, const string& query): WPEFramework::JSONRPC::SmartLinkType<WPEFramework::Core::JSON::IElement>(remoteCallsign, localCallsign, query) {}
    MOCK_METHOD(uint32_t, Subscribe, (const uint32_t, const string&, const METHOD&), ());
};

class MockINotification : public WPEFramework::Exchange::INetworkManager::INotification {
public:
    MOCK_METHOD(void, onInterfaceStateChange, (const WPEFramework::Exchange::INetworkManager::InterfaceState, const string), (override));
    MOCK_METHOD(void, onActiveInterfaceChange, (const string, const string), (override));
    MOCK_METHOD(void, onIPAddressChange, (const string, const string, const string, const WPEFramework::Exchange::INetworkManager::IPStatus), (override));
    MOCK_METHOD(void, onInternetStatusChange, (const WPEFramework::Exchange::INetworkManager::InternetStatus, const WPEFramework::Exchange::INetworkManager::InternetStatus), (override));
    MOCK_METHOD(void, onAvailableSSIDs, (const string), (override));
    MOCK_METHOD(void, onWiFiStateChange, (const WPEFramework::Exchange::INetworkManager::WiFiState), (override));
    MOCK_METHOD(void, onWiFiSignalQualityChange, (const string, const string, const string, const string, const WPEFramework::Exchange::INetworkManager::WiFiSignalQuality), (override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t), (override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
};

class MockINetworkManager : public WPEFramework::Exchange::INetworkManager {
public:
    MOCK_METHOD(uint32_t, GetAvailableInterfaces, (WPEFramework::Exchange::INetworkManager::IInterfaceDetailsIterator*& interfaces), (override));
    MOCK_METHOD(uint32_t, GetPrimaryInterface, (string& interface), (override));
    MOCK_METHOD(uint32_t, SetInterfaceState, (const string& interface, const bool enabled), (override));
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

class MockIInterfaceDetailsIterator : public WPEFramework::Exchange::INetworkManager::IInterfaceDetailsIterator {
public:
    MOCK_METHOD(bool, Next, (WPEFramework::Exchange::INetworkManager::InterfaceDetails&), (override));
    MOCK_METHOD(bool, Previous, (WPEFramework::Exchange::INetworkManager::InterfaceDetails&), (override));
    MOCK_METHOD(void, Reset, (const uint32_t position), (override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(uint32_t, Count, (), (const, override));
    MOCK_METHOD(WPEFramework::Exchange::INetworkManager::InterfaceDetails, Current, (), (const, override));
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (uint32_t), (override));
};

class MockTimer {
public:
    MOCK_METHOD(void, stop, (), ());
    MOCK_METHOD(void, start, (uint32_t), ());
};

class MockNetwork: public WPEFramework::PluginHost::IPlugin, WPEFramework::PluginHost::JSONRPC{
public:
    MOCK_METHOD(void, subscribeToEvents, (), ());
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (uint32_t), (override));
    MOCK_METHOD(const std::string, Initialize, (WPEFramework::PluginHost::IShell*), (override));
    MOCK_METHOD(void, Deinitialize, (WPEFramework::PluginHost::IShell*), (override));
    MOCK_METHOD(void, ReportonInterfaceStateChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonActiveInterfaceChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonIPAddressChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonInternetStatusChange, (const JsonObject&), ());
};

class StubNetwork : public WPEFramework::Plugin::Network {
public:
    MockNetwork* network;
    void onInterfaceStateChange(const JsonObject& parameters) {
        WPEFramework::Plugin::Network::ReportonInterfaceStateChange(parameters); // Call the method on this object
    }

    void onActiveInterfaceChange(const JsonObject& parameters) {
        EXPECT_EQ(parameters["prevActiveInterface"].String(), "eth0");
        EXPECT_EQ(parameters["currentActiveInterface"].String(), "wlan0");
        WPEFramework::Plugin::Network::ReportonActiveInterfaceChange(parameters);
    }

    void onIPAddressChange(const JsonObject& parameters) {
        WPEFramework::Plugin::Network::ReportonIPAddressChange(parameters);
    }

    void onInternetStatusChange(const JsonObject& parameters) {
        EXPECT_EQ(parameters["state"].String(), "CONNECTED");
        EXPECT_EQ(parameters["status"].String(), "OK");
        WPEFramework::Plugin::Network::ReportonInternetStatusChange(parameters);
    }

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (uint32_t), (override));
    MOCK_METHOD(const std::string, Initialize, (WPEFramework::PluginHost::IShell*), (override));
    MOCK_METHOD(void, Deinitialize, (WPEFramework::PluginHost::IShell*), (override));
    MOCK_METHOD(std::string, Information, (), (override, const));
    MOCK_METHOD(void, ReportonInterfaceStateChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonActiveInterfaceChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonIPAddressChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonInternetStatusChange, (const JsonObject&), ());
    MOCK_METHOD(void, subscribeToEvents, (), ());
};

#endif //SERVICEMOCK_H


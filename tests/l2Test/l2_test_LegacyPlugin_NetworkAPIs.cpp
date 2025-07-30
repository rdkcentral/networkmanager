/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <iostream>
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include "NetworkManagerLogger.h"
#include "LegacyNetworkAPIs.h"
#include "ThunderPortability.h"
#include "INetworkManager.h"
#include "INetworkManagerMock.h"
#include "LegacyNetworkMock.h"
#include "IStringIteratorMock.h"
#include "mockauthservices.h"
#include "InterfaceIteratorMock.h"

using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using IStringIterator = RPC::IIteratorType<string,RPC::ID_STRINGITERATOR>;
using ::testing::NiceMock;

class NetworkTest : public ::testing::Test{
protected:
    Core::ProxyType<Plugin::Network> plugin;
    Core::JSONRPC::Handler& handler;
    Core::JSONRPC::Handler& handlerV2;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    ServiceMock *m_service;
    bool m_subsIfaceStateChange;
    bool m_subsActIfaceChange;
    bool m_subsIPAddrChange;
    bool m_subsInternetChange;

    NetworkTest()
        : plugin(Core::ProxyType<Plugin::Network>::Create())
          , handler(*(plugin))
          , handlerV2(*(plugin->GetHandler(2)))
          , INIT_CONX(1, 0)
          , m_subsIfaceStateChange(true)
          , m_subsActIfaceChange(true)
          , m_subsIPAddrChange(true)
          , m_subsInternetChange(true)
    {
        ServiceMock* service = new ServiceMock();
        WPEFramework::PluginHost::IAuthenticate* mock_security_agent = new MockIAuthenticate();
        ServiceMock* mockShell = new ServiceMock();
        
        EXPECT_CALL(*service, AddRef()).Times(1);
        EXPECT_CALL(*service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .Times(2)
            .WillOnce(::testing::Invoke(
                        [&](const uint32_t, const string& name) -> void* {
                        EXPECT_EQ(name, string(_T("SecurityAgent")));
                        return static_cast<void*>(mock_security_agent);
                        }))
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    m_subsIfaceStateChange = true;
                    m_subsActIfaceChange = true;
                    m_subsIPAddrChange = true;
                    m_subsInternetChange = true;
                    return static_cast<void*>(mockShell);
                    }));
        m_service = service;
        EXPECT_CALL(*mockShell, State())
            .WillOnce(::testing::Return(PluginHost::IShell::state::UNAVAILABLE))
            .WillOnce(::testing::Return(PluginHost::IShell::state::UNAVAILABLE))
            .WillRepeatedly(::testing::Return(PluginHost::IShell::state::ACTIVATED));
        EXPECT_EQ(string{}, plugin->Initialize(service));
        delete mockShell;
        delete mock_security_agent;
    }
    virtual ~NetworkTest() override
    {
        plugin->Deinitialize(m_service);
        m_service->Release();
        delete m_service;
    }
};

TEST_F(NetworkTest, getInterfaces)
{
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    NiceMock<MockIInterfaceDetailsIterator> mockIterator;
    Exchange::INetworkManager::InterfaceDetails entry1;
    entry1.type = Exchange::INetworkManager::InterfaceType::INTERFACE_TYPE_ETHERNET;
    entry1.mac = "00:11:22:33:44:55";
    entry1.enabled = true;
    entry1.connected = false;

    Exchange::INetworkManager::InterfaceDetails entry2;
    entry2.type = Exchange::INetworkManager::InterfaceType::INTERFACE_TYPE_WIFI;
    entry2.mac = "66:77:88:99:AA:BB";
    entry2.enabled = false;
    entry2.connected = true;

    // Set up Next() to return two entries, then false
    EXPECT_CALL(mockIterator, Next(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(entry1), testing::Return(true)))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(entry2), testing::Return(true)))
        .WillOnce(testing::Return(false));
    EXPECT_CALL(mockIterator, Release()).Times(1);
    NiceMock<ServiceMock> service;
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
                }));
    EXPECT_CALL(*mockNetworkManager, GetAvailableInterfaces(::testing::_))
        .WillOnce(testing::DoAll(
                    ::testing::Invoke([&mockIterator](WPEFramework::Exchange::INetworkManager::IInterfaceDetailsIterator*& iterator) -> uint32_t {
                        iterator = &mockIterator;
                        return 0;
                        })));
    EXPECT_CALL(*mockNetworkManager, Release()).Times(1);
    JsonObject parameters, JsonResponse;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getInterfaces"), _T("{}"), response));
    std::string expectedResponse = "{\"interfaces\":[{\"interface\":\"ETHERNET\",\"macAddress\":\"00:11:22:33:44:55\",\"enabled\":true,\"connected\":false},{\"interface\":\"WIFI\",\"macAddress\":\"66:77:88:99:AA:BB\",\"enabled\":false,\"connected\":true}],\"success\":true}";
    EXPECT_EQ(response, expectedResponse);
    delete mockNetworkManager;
}

TEST_F(NetworkTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getStbIp")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getInterfaces")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setStunEndPoint")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isInterfaceEnabled")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setInterfaceEnabled")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDefaultInterface")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getIPSettings")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isConnectedToInternet")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPublicIP")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSTBIPFamily")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setConnectivityTestEndpoints")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getCaptivePortalURI")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("startConnectivityMonitoring")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("stopConnectivityMonitoring")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setDefaultInterface")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setIPSettings")));
    EXPECT_EQ(Core::ERROR_NONE, handlerV2.Exists(_T("getIPSettings")));
}

TEST_F(NetworkTest, setStunEndpoint) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    string endpoint = "stun.example.com";
    uint32_t port = 3478;
    uint32_t bindTimeout = 10;
    uint32_t cacheTimeout = 30;

    JsonObject parameters;
    parameters["server"] = endpoint;
    parameters["port"] = port;
    parameters["timeout"] = bindTimeout;
    parameters["cache_timeout"] = cacheTimeout;

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));
    EXPECT_CALL(*mockNetworkManager, SetStunEndpoint(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([&](string const endpoint, const uint32_t port, const uint32_t timeout, const uint32_t cacheLifetime ) -> uint32_t {
                    return 0;
                    }));
    EXPECT_CALL(*mockNetworkManager, Release()).Times(1);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setStunEndPoint"), _T("{}"), response));
    EXPECT_EQ(response, "{\"success\":true}");
    delete mockNetworkManager;
}

TEST_F(NetworkTest, setInterfaceEnabled){
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    string interface = "ETHERNET";
    bool enabled = true;

    JsonObject jsonParameters;
    jsonParameters["interface"] = interface;
    jsonParameters["enabled"] = enabled;
    string parameters;
    jsonParameters.ToString(parameters);
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));

    EXPECT_CALL(*mockNetworkManager, SetInterfaceState(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setInterfaceEnabled"), _T(parameters), response));
    EXPECT_EQ(response, "{\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, getDefaultInterface) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    string interface = "eth0";

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));

    EXPECT_CALL(*mockNetworkManager, GetPrimaryInterface(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](string& interface) -> uint32_t                     {
                    interface = "eth0";
                    return Core::ERROR_NONE;
                    }));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDefaultInterface"), _T("{}"), response));
    EXPECT_EQ(response, "{\"interface\":\"ETHERNET\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, setDefaultInterface) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setDefaultInterface"), _T("{}"), response));
    EXPECT_EQ(response, "{\"success\":true}");
}

TEST_F(NetworkTest, setIPSettings) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["interface"] = "WIFI";
    jsonParameters["ipversion"] = "IPv4";
    jsonParameters["autoconfig"] = false;
    jsonParameters["ipaddr"] = "192.168.1.100";
    jsonParameters["gateway"] = "192.168.1.1";
    jsonParameters["primarydns"] = "8.8.8.8";
    jsonParameters["secondarydns"] = "8.8.4.4";
    jsonParameters["netmask"] = "255.255.255.0";

    string parameters;
    jsonParameters.ToString(parameters);

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));

    EXPECT_CALL(*mockNetworkManager, SetIPSettings(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setIPSettings"), _T(parameters), response));
    EXPECT_EQ(response, "{\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, getIPSettings) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["interface"] = "WIFI";
    jsonParameters["ipversion"] = "IPv4";
    string parameters;
    string response;
    jsonParameters.ToString(parameters);

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));

    WPEFramework::Exchange::INetworkManager::IPAddress address{};
    EXPECT_CALL(*mockNetworkManager, GetIPSettings(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](string& , const string&, WPEFramework::Exchange::INetworkManager::IPAddress& address) -> uint32_t                     {
                    address.primarydns = "75.75.75.76";
                    address.secondarydns = "75.75.76.76";
                    address.dhcpserver = "192.168.0.1";
                    address.gateway = "192.168.0.1";
                    address.ipaddress = "192.168.0.11";
                    address.ipversion = "IPv4";
                    address.prefix = 24;
                    return Core::ERROR_NONE;
                    }));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getIPSettings"), _T(parameters), response));
    EXPECT_EQ(response, "{\"primarydns\":\"75.75.75.76\",\"gateway\":\"192.168.0.1\",\"secondarydns\":\"75.75.76.76\",\"dhcpserver\":\"192.168.0.1\",\"netmask\":\"255.255.255.0\",\"ipaddr\":\"192.168.0.11\",\"autoconfig\":false,\"ipversion\":\"IPv4\",\"interface\":\"WIFI\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, getIPSettingsIPv6) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["interface"] = "WIFI";
    jsonParameters["ipversion"] = "IPv6";
    string parameters;
    string response;
    jsonParameters.ToString(parameters);

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));

    WPEFramework::Exchange::INetworkManager::IPAddress address{};
    EXPECT_CALL(*mockNetworkManager, GetIPSettings(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](string& , const string&, WPEFramework::Exchange::INetworkManager::IPAddress& address) -> uint32_t                     {
                    address.ipaddress = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
                    address.ipversion = "IPv6";
                    address.prefix = 64;
                    address.primarydns = "2001:4860:4860::8888";
                    address.gateway = "fe80::1";
                    address.secondarydns = "2001:4860:4860::8844";
                    address.dhcpserver = "fe80::2";
                    return Core::ERROR_NONE;
                    }));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getIPSettings"), _T(parameters), response));
    EXPECT_EQ(response, "{\"primarydns\":\"2001:4860:4860::8888\",\"gateway\":\"fe80::1\",\"secondarydns\":\"2001:4860:4860::8844\",\"dhcpserver\":\"fe80::2\",\"netmask\":64,\"ipaddr\":\"2001:0db8:85a3:0000:0000:8a2e:0370:7334\",\"autoconfig\":false,\"ipversion\":\"IPv6\",\"interface\":\"WIFI\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, getIPSettingsErrorEmptyString) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["interface"] = "WIFI";
    jsonParameters["ipversion"] = "IPv4";
    string parameters;
    string response;
    jsonParameters.ToString(parameters);

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));

    WPEFramework::Exchange::INetworkManager::IPAddress address{};
    EXPECT_CALL(*mockNetworkManager, GetIPSettings(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](string& , const string&, WPEFramework::Exchange::INetworkManager::IPAddress& address) -> uint32_t {
                    address.ipaddress = "";
                    address.ipversion = "IPv4";
                    address.prefix = 34;
                    return Core::ERROR_NONE;
                    }));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getIPSettings"), _T(parameters), response));
    EXPECT_EQ(response, "{\"success\":false}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, getIPSettings2) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["interface"] = "WIFI";
    jsonParameters["ipversion"] = "IPv4";
    string parameters;
    string response;
    jsonParameters.ToString(parameters);

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));

    WPEFramework::Exchange::INetworkManager::IPAddress address{};
    EXPECT_CALL(*mockNetworkManager, GetIPSettings(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](string& , const string&, WPEFramework::Exchange::INetworkManager::IPAddress& address) -> uint32_t                     {
                    address.primarydns = "75.75.75.76";
                    address.secondarydns = "75.75.76.76";
                    address.dhcpserver = "192.168.0.1";
                    address.gateway = "192.168.0.1";
                    address.ipaddress = "192.168.0.11";
                    address.ipversion = "IPv4";
                    address.prefix = 24;
                    return Core::ERROR_NONE;
                    }));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handlerV2.Invoke(connection, _T("getIPSettings"), _T(parameters), response));
    string expectedResponse = "{\"interface\":\"WIFI\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddr\":\"192.168.0.11\",\"netmask\":\"255.255.255.0\",\"dhcpserver\":\"192.168.0.1\",\"gateway\":\"192.168.0.1\",\"primarydns\":\"75.75.75.76\",\"secondarydns\":\"75.75.76.76\",\"success\":true}";
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(NetworkTest, isConnectedToInternet) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["ipversion"] = "IPv4";
    string parameters;
    string response;
    jsonParameters.ToString(parameters);
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));

    EXPECT_CALL(*mockNetworkManager, IsConnectedToInternet(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](string& , string&, WPEFramework::Exchange::INetworkManager::InternetStatus& result) -> uint32_t
                    {
                    result = WPEFramework::Exchange::INetworkManager::InternetStatus::INTERNET_FULLY_CONNECTED;
                    return Core::ERROR_NONE;
                    }));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isConnectedToInternet"), _T(parameters), response));
    EXPECT_EQ(response, "{\"ipversion\":\"IPv4\",\"connectedToInternet\":true,\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, getInternetConnectionState) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["ipversion"] = "IPv4";
    string parameters;
    string response;
    jsonParameters.ToString(parameters);
 
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    EXPECT_CALL(*mockNetworkManager, IsConnectedToInternet(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, Exchange::INetworkManager::InternetStatus& status) {
                status = Exchange::INetworkManager::InternetStatus::INTERNET_CAPTIVE_PORTAL;
                return Core::ERROR_NONE;
            }));

    EXPECT_CALL(*mockNetworkManager, GetCaptivePortalURI(::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SaveArg<0>(&response),
            ::testing::Return(Core::ERROR_NONE)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getInternetConnectionState"), _T(parameters), response));
    EXPECT_EQ(response, "{\"ipversion\":\"IPv4\",\"state\":2,\"URI\":\"\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, doPing) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    std::string traceResult = "{"
        "\"success\": true,"
        "\"tripStdDev\": \"0.741 ms\","
        "\"tripAvg\": \"16.702\","
        "\"tripMin\": \"15.747\","
        "\"tripMax\": \"17.564\","
        "\"packetLoss\": \"0\","
        "\"endpoint\": \"8.8.8.8\","
        "\"packetsReceived\": 5,"
        "\"packetsTransmitted\": 5,"
        "\"target\": \"8.8.8.8\""
        "}";
    EXPECT_CALL(*mockNetworkManager, Ping(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const string, const string, const uint32_t, const uint16_t, const string, string& result) -> uint32_t
                    {
                    result = traceResult;
                    return Core::ERROR_NONE;
                    }));

    JsonObject parametersJson;
    parametersJson["endpoint"] = "8.8.8.8";
    parametersJson["packets"] = 5;
    parametersJson["ipversion"] = "IPv4";
    parametersJson["guid"] = "...";
    string parameters;
    parametersJson.ToString(parameters);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("ping"), _T(parameters), response));
    string expectedResponse = "{\"packetsTransmitted\":5,\"packetsReceived\":5,\"packetLoss\":\"0\",\"target\":\"8.8.8.8\",\"tripMax\":\"17.564\",\"tripMin\":\"15.747\",\"tripAvg\":\"16.702\",\"tripStdDev\":\"0.741 ms\",\"endpoint\":\"8.8.8.8\",\"success\":true}";
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(NetworkTest, doTrace) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    std::string traceResult = "{"
    "\"results\": \"[\\\"traceroute to 8.8.8.8 (8.8.8.8), 6 hops max, 52 byte packets \\\",\\\""
    "1  gateway (10.46.5.1)  0.448 ms\\\",\\\""
    "2  10.46.0.240 (10.46.0.240)  3.117 ms\\\",\\\""
    "3  *\\\",\\\""
    "4  *\\\",\\\""
    "5  *\\\",\\\""
    "6  *\\\"]\","
    "\"endpoint\": \"8.8.8.8\","
    "\"success\": true"
    "}";
    EXPECT_CALL(*mockNetworkManager, Trace(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const string, const string, const uint32_t, const string, string& response1) -> uint32_t
                    {
                    response1 = traceResult;
                    return Core::ERROR_NONE;
                    }));

    JsonObject parametersJson;
    parametersJson["endpoint"] = "8.8.8.8";
    parametersJson["ipversion"] = "IPv4";
    parametersJson["packets"] = 1;
    string parametersStr;
    parametersJson.ToString(parametersStr);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("trace"), _T(parametersStr), response));
    // We expect the response to contain success and results fields
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"results\":") != std::string::npos);
    EXPECT_TRUE(response.find("\"endpoint\":\"8.8.8.8\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"target\":\"8.8.8.8\"") != std::string::npos);
    EXPECT_TRUE(response.find("6 hops max, 52 byte packets") != std::string::npos);

    delete mockNetworkManager;
}

TEST_F(NetworkTest, getPublicIP) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    EXPECT_CALL(*mockNetworkManager, GetPublicIP(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](string&, string&, string& ipaddress) {
                ipaddress = "69.136.49.95";
                return Core::ERROR_NONE;
            }));


    JsonObject parametersJson;
    parametersJson["ipv6"] = true;
    parametersJson["iface"] = "WIFI";
    string parametersStr;
    parametersJson.ToString(parametersStr);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPublicIP"), _T(parametersStr), response));
    EXPECT_EQ(response, "{\"public_ip\":\"69.136.49.95\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, isInterfaceEnabled) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    EXPECT_CALL(*mockNetworkManager, GetInterfaceState(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::Return(Core::ERROR_NONE)));

    JsonObject parametersJson;
    parametersJson["interface"] = "WIFI";
    string parametersStr;
    parametersJson.ToString(parametersStr);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isInterfaceEnabled"), _T(parametersStr), response));
    EXPECT_EQ(response, "{\"enabled\":false,\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, isInterfaceEnabledErrorInvalidInterface) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    JsonObject parametersJson;
    parametersJson["interface"] = "INVALID";
    string parametersStr;
    parametersJson.ToString(parametersStr);

    EXPECT_EQ(Core::ERROR_BAD_REQUEST, handler.Invoke(connection, _T("isInterfaceEnabled"), _T(parametersStr), response));
    EXPECT_EQ(response, string{});

    delete mockNetworkManager;
}

TEST_F(NetworkTest, setConnectivityTestEndpoints) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    EXPECT_CALL(*mockNetworkManager, SetConnectivityTestEndpoints(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    JsonArray array;
    array.Add("http://example.com");
    array.Add("http://example2.com");
    array.Add("http://example3.com");

    JsonObject parametersJson;
    parametersJson["endpoints"] = array;
    string parametersStr;
    parametersJson.ToString(parametersStr);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setConnectivityTestEndpoints"), _T(parametersStr), response));
    EXPECT_EQ(response, "{\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, setConnectivityTestEndpointsErrorInvalidType) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    JsonArray array;
    array.Add("http://example.com");
    array.Add(1234);
    array.Add("http://example3.com");

    JsonObject parametersJson;
    parametersJson["endpoints"] = array;
    string parametersStr;
    parametersJson.ToString(parametersStr);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setConnectivityTestEndpoints"), _T(parametersStr), response));
    EXPECT_EQ(response, "{\"success\":false}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, setConnectivityTestEndpointsErrorEmptyArray) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    JsonArray array;

    JsonObject parametersJson;
    string parametersStr;
    parametersJson.ToString(parametersStr);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setConnectivityTestEndpoints"), _T(parametersStr), response));
    EXPECT_EQ(response, "{\"success\":false}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, startConnectivityMonitoring) {
    string parameters;

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startConnectivityMonitoring"), _T(parameters), response));
    EXPECT_EQ(response, "{\"success\":true}");
}

TEST_F(NetworkTest, getCaptivePortalURI) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    string parameters;

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    EXPECT_CALL(*mockNetworkManager, GetCaptivePortalURI(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](string& uri) {
                uri = "http://10.0.0.1/captiveportal.jst";
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getCaptivePortalURI"), _T(parameters), response));
    EXPECT_EQ(response, "{\"uri\":\"http:\\/\\/10.0.0.1\\/captiveportal.jst\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, stopConnectivityMonitoring) {
    string parameters;

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("stopConnectivityMonitoring"), _T(parameters), response));
    EXPECT_EQ(response, "{\"success\":true}");
}

TEST_F(NetworkTest, getStbIp) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["interface"] = "WIFI";
    jsonParameters["ipversion"] = "IPv4";
    string parameters;
    string response;
    jsonParameters.ToString(parameters);

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));
    EXPECT_CALL(*mockNetworkManager, GetIPSettings(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](string& , const string&, WPEFramework::Exchange::INetworkManager::IPAddress& address) -> uint32_t {
                    address.ipaddress = "192.168.0.1";
                    address.ipversion = "IPv4";
                    address.prefix = 24;
                    return Core::ERROR_NONE;
                    }));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getStbIp"), _T(parameters), response));
    EXPECT_EQ(response, "{\"ip\":\"192.168.0.1\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, getSTBIPFamily) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    JsonObject jsonParameters;
    jsonParameters["interface"] = "WIFI";
    jsonParameters["ipversion"] = "IPv4";
    string parameters;
    string response;
    jsonParameters.ToString(parameters);

    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));
    EXPECT_CALL(*mockNetworkManager, GetIPSettings(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](string& , const string&, WPEFramework::Exchange::INetworkManager::IPAddress& address) -> uint32_t {
                    address.ipaddress = "192.168.0.1";
                    address.ipversion = "IPv4";
                    address.prefix = 24;
                    return Core::ERROR_NONE;
                    }));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSTBIPFamily"), _T({}), response));
    EXPECT_EQ(response, "{\"ip\":\"192.168.0.1\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(NetworkTest, ReportonInterfaceStateChangeAdded) {
    StubNetwork network;
    JsonObject parameters;
    parameters["status"] = "INTERFACE_ADDED";
    parameters["interface"] = "ETHERNET";
    network.onInterfaceStateChange(parameters);
}

TEST_F(NetworkTest, ReportonInterfaceStateChangeRemoved) {
    StubNetwork network;
    JsonObject parameters;
    parameters["status"] = "INTERFACE_REMOVED";
    parameters["interface"] = "eth0";
    network.onInterfaceStateChange(parameters);
}

TEST_F(NetworkTest, ReportonInterfaceStateChangeLinkUp) {
    StubNetwork network;
    JsonObject parameters;
    parameters["status"] = "INTERFACE_LINK_UP";
    parameters["interface"] = "wlan0";
    network.onInterfaceStateChange(parameters);
}

TEST_F(NetworkTest, ReportonInterfaceStateChangeLinkDown) {
    StubNetwork network;
    JsonObject parameters;
    parameters["status"] = "INTERFACE_LINK_DOWN";
    parameters["interface"] = "eth0";
    network.onInterfaceStateChange(parameters);
}

TEST_F(NetworkTest, ReportonActiveInterfaceChange) {
    StubNetwork network;
    JsonObject parameters;
    parameters["prevActiveInterface"] = "eth0";
    parameters["currentActiveInterface"] = "wlan0";
    network.onActiveInterfaceChange(parameters);
}

TEST_F(NetworkTest, ReportonIPAddressChange_IPv4) {
    StubNetwork network;
    JsonObject parameters;
    parameters["interface"] = "eth0";
    parameters["ipversion"] = "IPv4";
    parameters["ipaddress"] = "192.168.1.100";
    network.onIPAddressChange(parameters);
}

TEST_F(NetworkTest, ReportonIPAddressChange_IPv6) {
    StubNetwork network;
    JsonObject parameters;
    parameters["interface"] = "wlan0";
    parameters["ipversion"] = "IPv6";
    parameters["ipaddress"] = "fe80::1";

    network.onIPAddressChange(parameters);
}

TEST_F(NetworkTest, ReportonInternetStatusChange) {
    StubNetwork network;
    JsonObject parameters;
    parameters["state"] = "CONNECTED";
    parameters["status"] = "OK";
    network.onInternetStatusChange(parameters);
}

TEST_F(NetworkTest, Information) {
    StubNetwork network;
    network.Information();
}

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
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include "NetworkManagerLogger.h"
#include "LegacyWiFiManagerAPIs.h"
#include "ThunderPortability.h"
#include "INetworkManagerMock.h"
#include "LegacyWiFiManagerMock.h"
#include "IStringIteratorMock.h"
#include "mockauthservices.h"
 
using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::NiceMock;

#define WPA_SUPPLICANT_CONF "/opt/secure/wifi/wpa_supplicant.conf/wpa_supplicant.conf"
 
Plugin::WiFiManager* _gWiFiInstance = nullptr;
class WiFiManagerTest : public ::testing::Test  {
protected:
    Core::ProxyType<Plugin::WiFiManager> plugin;
    Core::JSONRPC::Handler& handler;
    Core::JSONRPC::Handler& handlerV2;
    DECL_CORE_JSONRPC_CONX connection;
    ServiceMock *m_service;
    Core::JSONRPC::Message message;
    string response;

    WiFiManagerTest()
        : plugin(Core::ProxyType<Plugin::WiFiManager>::Create())
          , handler(*(plugin))
          , handlerV2(*(plugin->GetHandler(2)))
          , INIT_CONX(1, 0)
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

    virtual ~WiFiManagerTest() override
    {
        plugin->Deinitialize(m_service);
        m_service->Release();
        delete m_service;
    }
};

TEST_F(WiFiManagerTest, TestedAPIsShouldExist)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("startScan")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("stopScan")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("connect")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("disconnect")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("initiateWPSPairing")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("cancelWPSPairing")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("saveSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("clearSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPairedSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPairedSSIDInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isPaired")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getCurrentState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getConnectedSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("retrieveSSID")));
}

TEST_F(WiFiManagerTest, cancelWPSPairing)
{
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
                    [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return static_cast<void*>(mockNetworkManager);
                    }));
    EXPECT_CALL(*mockNetworkManager, StopWPS())
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("cancelWPSPairing"), _T("{}"), response));
    EXPECT_EQ(response, "{\"result\":\"\",\"success\":true}");
    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, clearSSID) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    EXPECT_CALL(*mockNetworkManager, RemoveKnownSSID(::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clearSSID"), _T("{}"), response));
    EXPECT_EQ(response, "{\"result\":0,\"success\":true}");
    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, connect) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    EXPECT_CALL(*mockNetworkManager, WiFiConnect(::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Create a sample parameters object
    JsonObject jsonParameters;
    jsonParameters["ssid"] = "my-ssid";
    jsonParameters["passphrase"] = "my-passphrase";
    jsonParameters["securityMode"] = 1; // Assuming 1 is the value for WPA2

    string parameters;
    jsonParameters.ToString(parameters);
    // Call the connect method
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("connect"), parameters, response));
    EXPECT_EQ(response, "{\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getConnectedSSID) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiSSIDInfo ssidInfo{};
    ssidInfo.ssid = "my-ssid";
    ssidInfo.bssid = "00:11:22:33:44:55";
    ssidInfo.rate = "100";
    ssidInfo.noise = "-50";
    ssidInfo.security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK;
    ssidInfo.strength = "50";
    ssidInfo.frequency = "2.4";
    EXPECT_CALL(*mockNetworkManager, GetConnectedSSID(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ssidInfo),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getConnectedSSID method
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getConnectedSSID"), _T("{}"), response));

    std::string expectedResponse = "{\"ssid\":\"my-ssid\",\"bssid\":\"00:11:22:33:44:55\",\"rate\":\"100\",\"noise\":\"-50\",\"security\":6,\"signalStrength\":\"50\",\"frequency\":\"2.4\",\"success\":true}";
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getConnectedSSIDSAE) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiSSIDInfo ssidInfo{};
    ssidInfo.ssid = "my-ssid";
    ssidInfo.bssid = "00:11:22:33:44:55";
    ssidInfo.rate = "100";
    ssidInfo.noise = "-50";
    ssidInfo.security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE;
    ssidInfo.strength = "50";
    ssidInfo.frequency = "2.4";
    EXPECT_CALL(*mockNetworkManager, GetConnectedSSID(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ssidInfo),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getConnectedSSID method
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getConnectedSSID"), _T("{}"), response));

    std::string expectedResponse = "{\"ssid\":\"my-ssid\",\"bssid\":\"00:11:22:33:44:55\",\"rate\":\"100\",\"noise\":\"-50\",\"security\":14,\"signalStrength\":\"50\",\"frequency\":\"2.4\",\"success\":true}";
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getConnectedSSIDEAP) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiSSIDInfo ssidInfo{};
    ssidInfo.ssid = "my-ssid";
    ssidInfo.bssid = "00:11:22:33:44:55";
    ssidInfo.rate = "100";
    ssidInfo.noise = "-50";
    ssidInfo.security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_EAP;
    ssidInfo.strength = "50";
    ssidInfo.frequency = "2.4";
    EXPECT_CALL(*mockNetworkManager, GetConnectedSSID(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ssidInfo),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getConnectedSSID method
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getConnectedSSID"), _T("{}"), response));

    std::string expectedResponse = "{\"ssid\":\"my-ssid\",\"bssid\":\"00:11:22:33:44:55\",\"rate\":\"100\",\"noise\":\"-50\",\"security\":12,\"signalStrength\":\"50\",\"frequency\":\"2.4\",\"success\":true}";
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getCurrentState) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiState state = Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTED;
    EXPECT_CALL(*mockNetworkManager, GetWifiState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(state),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getCurrentState method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getCurrentState"), _T("{}"), response));

    std::string expectedResponse = "{\"state\":5,\"success\":true}";
    // Verify the response
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getCurrentStateFailed1) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiState state = Exchange::INetworkManager::WiFiState::WIFI_STATE_ERROR;
    EXPECT_CALL(*mockNetworkManager, GetWifiState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(state),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getCurrentState method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getCurrentState"), _T("{}"), response));

    std::string expectedResponse = "{\"state\":6,\"success\":true}";
    // Verify the response
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getCurrentStateFailed2) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiState state = Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTION_INTERRUPTED;
    EXPECT_CALL(*mockNetworkManager, GetWifiState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(state),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getCurrentState method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getCurrentState"), _T("{}"), response));

    std::string expectedResponse = "{\"state\":2,\"success\":true}";
    // Verify the response
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getCurrentStateDisconnected) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiState state = Exchange::INetworkManager::WiFiState::WIFI_STATE_DISCONNECTED;
    EXPECT_CALL(*mockNetworkManager, GetWifiState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(state),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getCurrentState method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getCurrentState"), _T("{}"), response));

    std::string expectedResponse = "{\"state\":2,\"success\":true}";
    // Verify the response
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getCurrentStateConnected) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiState state = Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTED;
    EXPECT_CALL(*mockNetworkManager, GetWifiState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(state),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getCurrentState method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getCurrentState"), _T("{}"), response));

    std::string expectedResponse = "{\"state\":5,\"success\":true}";
    // Verify the response
    EXPECT_EQ(response, expectedResponse);

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getPairedSSIDInfo) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiSSIDInfo ssidInfo{};
    ssidInfo.ssid = "my-ssid";
    ssidInfo.bssid = "00:11:22:33:44:55";
    EXPECT_CALL(*mockNetworkManager, GetConnectedSSID(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(ssidInfo),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the getPairedSSIDInfo method
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedSSIDInfo"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"ssid\":\"my-ssid\",\"bssid\":\"00:11:22:33:44:55\",\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, getSupportedSecurityModes) {
    // Call the getSupportedSecurityModes method
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSupportedSecurityModes"), _T("{}"), response));

    // Verify the response
    std::string expectedResponse = "{\"security_modes\":{\"NET_WIFI_SECURITY_NONE\":0,\"NET_WIFI_SECURITY_WEP_64\":1,\"NET_WIFI_SECURITY_WEP_128\":2,\"NET_WIFI_SECURITY_WPA_PSK_TKIP\":3,\"NET_WIFI_SECURITY_WPA_PSK_AES\":4,\"NET_WIFI_SECURITY_WPA2_PSK_TKIP\":5,\"NET_WIFI_SECURITY_WPA2_PSK_AES\":6,\"NET_WIFI_SECURITY_WPA_ENTERPRISE_TKIP\":7,\"NET_WIFI_SECURITY_WPA_ENTERPRISE_AES\":8,\"NET_WIFI_SECURITY_WPA2_ENTERPRISE_TKIP\":9,\"NET_WIFI_SECURITY_WPA2_ENTERPRISE_AES\":10,\"NET_WIFI_SECURITY_WPA_WPA2_PSK\":11,\"NET_WIFI_SECURITY_WPA_WPA2_ENTERPRISE\":12,\"NET_WIFI_SECURITY_WPA3_PSK_AES\":13,\"NET_WIFI_SECURITY_WPA3_SAE\":14,\"NET_WIFI_SECURITY_NOT_SUPPORTED\":99},\"success\":true}";
    EXPECT_EQ(response, expectedResponse);

}

TEST_F(WiFiManagerTest, saveSSID) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    Exchange::INetworkManager::WiFiConnectTo ssid{};
    ssid.ssid = "my-ssid";
    ssid.passphrase = "my-passphrase";
    ssid.security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK;
    EXPECT_CALL(*mockNetworkManager, AddToKnownSSIDs(::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Create a sample parameters object
    JsonObject jsonParameters;
    jsonParameters["ssid"] = "my-ssid";
    jsonParameters["passphrase"] = "my-passphrase";
    jsonParameters["security"] = 1; // Assuming 1 is the value for WPA2

    string parameters;
    jsonParameters.ToString(parameters);
    // Call the saveSSID method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("saveSSID"), parameters, response));

    // Verify the response
    EXPECT_EQ(response, "{\"result\":0,\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, disconnect) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    EXPECT_CALL(*mockNetworkManager, WiFiDisconnect())
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the disconnect method
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("disconnect"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"result\":0,\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, initiateWPSPairing) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    EXPECT_CALL(*mockNetworkManager, StartWPS(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Create a sample parameters object
    JsonObject jsonParameters;
    jsonParameters["method"] = static_cast<int>(Exchange::INetworkManager::WiFiWPS::WIFI_WPS_PIN);
    jsonParameters["pin"] = "my-pin";

    string parameters;
    jsonParameters.ToString(parameters);
    // Call the initiateWPSPairing method
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("initiateWPSPairing"), parameters, response));

    // Verify the response
    EXPECT_EQ(response, "{\"result\":0,\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, startScan) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    EXPECT_CALL(*mockNetworkManager, StartWiFiScan(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Create a sample parameters object
    JsonObject jsonParameters;
    jsonParameters["frequency"] = "2.4GHz";
    jsonParameters["ssid"]  = "test";

    // Call the startScan method
    std::string parameters;
    jsonParameters.ToString(parameters);
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("startScan"), parameters, response));

    // Verify the response
    EXPECT_EQ(response, "{\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, stopScan) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    EXPECT_CALL(*mockNetworkManager, StopWiFiScan())
        .WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the stopScan method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("stopScan"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"success\":true}");

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, stopScan_Error) {
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));
    EXPECT_CALL(*mockNetworkManager, StopWiFiScan())
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));
    EXPECT_CALL(*mockNetworkManager, Release())
        .Times(1);

    // Call the stopScan method
    std::string response;
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("stopScan"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, string{});

    delete mockNetworkManager;
}

TEST_F(WiFiManagerTest, onWiFiStateChange1) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["state"] = static_cast<int>(Exchange::INetworkManager::WIFI_STATE_SSID_CHANGED);
    wifi.onWiFiStateChange(parameters);
}

TEST_F(WiFiManagerTest, onWiFiStateChange2) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["state"] = static_cast<int>(Exchange::INetworkManager::WIFI_STATE_CONNECTION_LOST);
    wifi.onWiFiStateChange(parameters);
}

TEST_F(WiFiManagerTest, onWiFiStateChange3) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["state"] = static_cast<int>(Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED);
    wifi.onWiFiStateChange(parameters);
}

TEST_F(WiFiManagerTest, onWiFiStateChange4) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["state"] = static_cast<int>(Exchange::INetworkManager::WIFI_STATE_CONNECTION_INTERRUPTED);
    wifi.onWiFiStateChange(parameters);
}

TEST_F(WiFiManagerTest, onWiFiStateChange5) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["state"] = static_cast<int>(Exchange::INetworkManager::WIFI_STATE_INVALID_CREDENTIALS);
    wifi.onWiFiStateChange(parameters);
}

TEST_F(WiFiManagerTest, onWiFiStateChange6) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["state"] = static_cast<int>(Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND);
    wifi.onWiFiStateChange(parameters);
}

TEST_F(WiFiManagerTest, onWiFiStateChange7) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["state"] = static_cast<int>(Exchange::INetworkManager::WIFI_STATE_ERROR);
    wifi.onWiFiStateChange(parameters);
}

TEST_F(WiFiManagerTest, onWiFiStateChange8) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["state"] = static_cast<int>(Exchange::INetworkManager::WIFI_STATE_AUTHENTICATION_FAILED);
    wifi.onWiFiStateChange(parameters);
}

TEST_F(WiFiManagerTest, onAvailableSSIDs) {
    StubWiFi wifi;
    JsonObject parameters;
    JsonArray ssids;
    JsonObject ssid1;
    ssid1["ssid"] = "test";
    ssid1["security"] = 2;
    ssid1["signalStrength"] = "-27.000";
    ssid1["frequency"] = "2.4";
    ssids.Add(ssid1);
    parameters["ssids"] = ssids;

    wifi.onAvailableSSIDs(parameters);
}

TEST_F(WiFiManagerTest, onWiFiSignalQualityChange) {
    StubWiFi wifi;
    JsonObject parameters;
    parameters["signalStrength"] = "27.00";
    parameters["strength"] = "Excellent";

    wifi.onWiFiSignalQualityChange(parameters);
}

TEST_F(WiFiManagerTest, Information) {
    StubWiFi wifi;
    wifi.Information();
}

TEST_F(WiFiManagerTest, getPairedSSID) {
    // Create a mock network manager object
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    // Set up the mock network manager to return a mock string iterator
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    // Set up the mock string iterator to return a string
    MockStringIterator<string, RPC::ID_STRINGITERATOR>* mockStringIterator = new MockStringIterator<string, RPC::ID_STRINGITERATOR>();
    EXPECT_CALL(*mockNetworkManager, GetKnownSSIDs(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(mockStringIterator),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockStringIterator, Next(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>("my-ssid"),
            ::testing::Return(true)));

    // Call the getPairedSSID method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getPairedSSID"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"ssid\":\"my-ssid\",\"success\":true}");

    // Clean up
    delete mockNetworkManager;
    delete mockStringIterator;
}

TEST_F(WiFiManagerTest, isPaired) {
    // Create a mock network manager object
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    // Set up the mock network manager to return a mock string iterator
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    // Set up the mock string iterator to return a string
    MockStringIterator<string, RPC::ID_STRINGITERATOR>* mockStringIterator = new MockStringIterator<string, RPC::ID_STRINGITERATOR>();
    EXPECT_CALL(*mockNetworkManager, GetKnownSSIDs(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>(mockStringIterator),
            ::testing::Return(Core::ERROR_NONE)));
    EXPECT_CALL(*mockStringIterator, Next(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<0>("my-ssid"),
            ::testing::Return(true)));

    // Call the getPairedSSID method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isPaired"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"result\":1,\"success\":true}");

    // Clean up
    delete mockNetworkManager;
    delete mockStringIterator;
}

TEST_F(WiFiManagerTest, isPairedNoSSID) {
    // Create a mock network manager object
    MockINetworkManager* mockNetworkManager = new MockINetworkManager();

    // Set up the mock network manager to return a mock string iterator
    EXPECT_CALL(*m_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const string& name) -> void* {
                EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                return static_cast<void*>(mockNetworkManager);
            }));

    // Set up the mock string iterator to return a string
    EXPECT_CALL(*mockNetworkManager, GetKnownSSIDs(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Return(Core::ERROR_NONE)));

    // Call the getPairedSSID method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("isPaired"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"result\":0,\"success\":true}");

    // Clean up
    delete mockNetworkManager;
}

// Create a sample configuration file
std::string configFileContent = "ssid=\"my-ssid\"\n"
"key_mgmt=PSK\n"
"psk=\"my-passphrase\"\n";

void wpaSupplicantConfCreate(std::string configFileContent)
{
    std::string filePath = "/opt/secure/wifi/wpa_supplicant.conf";
    // Create the parent directories if they don't exist
    size_t pos = filePath.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dirPath = filePath.substr(0, pos);
        std::string cmd = "mkdir -p " + dirPath;
        FILE* pipe = popen(cmd.c_str(), "w");
        if (!pipe) {
            // Handle error
        }
        pclose(pipe);
    }
    std::ofstream configFileOut(filePath, std::ios_base::out | std::ios_base::app);
    // Check if the file is open
    if (configFileOut.is_open()) {
        // File is open, write to it
        configFileOut << configFileContent;
        configFileOut.close();

        // Print the content of the file
        std::ifstream configFileRead(filePath);
        if (configFileRead.is_open()) {
            std::cout << configFileRead.rdbuf();
            configFileRead.close();
        } else {
            std::cerr << "Unable to open file for reading." << std::endl;
        }
    } else {
        std::cerr << "Unable to open file for writing." << std::endl;
    }
}

TEST_F(WiFiManagerTest, retrieveSSIDWPAPSK) {
    std::string configFileContent = "ssid=\"my-ssid\"\n"
        "key_mgmt=PSK\n"
        "psk=\"my-passphrase\"\n";

    wpaSupplicantConfCreate(configFileContent);
    // Call the retrieveSSID method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("retrieveSSID"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"ssid\":\"my-ssid\",\"securityMode\":6,\"passphrase\":\"my-passphrase\",\"success\":true}");

    // Remove the configuration file
    std::remove("/opt/secure/wifi/wpa_supplicant.conf");
}

TEST_F(WiFiManagerTest, retrieveSSIDSecurityNone) {
    std::string configFileContent = "ssid=\"my-ssid\"\n"
        "key_mgmt=NONE\n"
        "psk=\"my-passphrase\"\n";

    wpaSupplicantConfCreate(configFileContent);
    // Call the retrieveSSID method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("retrieveSSID"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"ssid\":\"my-ssid\",\"securityMode\":0,\"passphrase\":\"my-passphrase\",\"success\":true}");

    // Remove the configuration file
    std::remove("/opt/secure/wifi/wpa_supplicant.conf");
}

TEST_F(WiFiManagerTest, retrieveSSIDSecuritySAE) {
    std::string configFileContent = "ssid=\"my-ssid\"\n"
        "key_mgmt=SAE\n"
        "psk=\"my-passphrase\"\n";

    wpaSupplicantConfCreate(configFileContent);
    // Call the retrieveSSID method
    std::string response;
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("retrieveSSID"), _T("{}"), response));

    // Verify the response
    EXPECT_EQ(response, "{\"ssid\":\"my-ssid\",\"securityMode\":14,\"passphrase\":\"my-passphrase\",\"success\":true}");

    // Remove the configuration file
    std::remove("/opt/secure/wifi/wpa_supplicant.conf");
}

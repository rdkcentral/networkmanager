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
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>

#include "FactoriesImplementation.h"
#include "IarmBusMock.h"
#include "WrapsMock.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "COMLinkMock.h"
#include "IarmBusMock.h"
#include "WorkerPoolImplementation.h"
#include "NetworkManagerImplementation.h"
#include "NetworkManagerRDKProxy.h"
#include "NetworkManagerLogger.h"
#include "NetworkManager.h"
#include "ConnectivityMockServer.h"

using namespace WPEFramework;
using ::testing::NiceMock;

class NetworkManagerEventTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::NetworkManager> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;

    // Mock classes
    IarmBusImplMock  *p_iarmBusImplMock   = nullptr;
    WrapsImplMock *p_wrapsImplMock = nullptr;
    IARM_EventHandler_t _nmEventHandler{};
    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl;

    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;


    NetworkManagerEventTest()
        : plugin(Core::ProxyType<Plugin::NetworkManager>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16))
    {
        p_iarmBusImplMock  = new NiceMock <IarmBusImplMock>;
        IarmBus::setImpl(p_iarmBusImplMock);
        p_wrapsImplMock = new NiceMock <WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);
        ON_CALL(service, COMLink())
        .WillByDefault(::testing::Invoke(
              [this]() {
                    return &comLinkMock;
                }));

        ON_CALL(service, ConfigLine())
            .WillByDefault(::testing::Return(
                "{"
                " \"locator\":\"libWPEFrameworkNetworkManager.so\"," 
                " \"classname\":\"NetworkManager\"," 
                " \"callsign\":\"org.rdk.NetworkManager\"," 
                " \"startuporder\":55," 
                " \"autostart\":false," 
                " \"configuration\":{"
                "  \"root\":{"
                "   \"outofprocess\":true,"
                "   \"locator\":\"libWPEFrameworkNetworkManagerImpl.so\""
                "  },"
                "  \"connectivity\":{"
                "   \"endpoint_1\":\"http://localhost:8080/generate_204\","
                "   \"interval\":60"
                "  },"
                "  \"stun\":{"
                "   \"endpoint\":\"stun.l.google.com\","
                "   \"port\":19302,"
                "   \"interval\":30"
                "  }"
                " }"
                "}"
            ));

        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                    [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        NetworkManagerImpl = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
                        return &NetworkManagerImpl;
                }));

        ON_CALL(*p_iarmBusImplMock, IARM_Bus_Init(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME)))
            .WillByDefault(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

        ON_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [&](const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
                    if ((string(IARM_BUS_NM_SRV_MGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS)) {
                        _nmEventHandler = handler;
                    }
                    if ((string(IARM_BUS_NM_SRV_MGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS)) {
                         _nmEventHandler = handler;
                    }
                    if ((string(IARM_BUS_NM_SRV_MGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS)) {
                         _nmEventHandler = handler;
                    }
                    if ((string(IARM_BUS_NM_SRV_MGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE)) {
                         _nmEventHandler = handler;
                    }
                    if ((string(IARM_BUS_NM_SRV_MGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_WIFI_MGR_EVENT_onAvailableSSIDs)) {
                         _nmEventHandler = handler;
                    }
                    if ((string(IARM_BUS_NM_SRV_MGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_WIFI_MGR_EVENT_onWIFIStateChanged)) {
                         _nmEventHandler = handler;
                    }
                    if ((string(IARM_BUS_NM_SRV_MGR_NAME) == string(ownerName)) && (eventId == IARM_BUS_WIFI_MGR_EVENT_onError)) {
                         _nmEventHandler = handler;
                    }
                    return IARM_RESULT_SUCCESS;
                }));

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);
        response = plugin->Initialize(&service);
        EXPECT_EQ(string(""), response);
        NetworkManagerLogger::SetLevel(static_cast<NetworkManagerLogger::LogLevel>(NetworkManagerLogger::DEBUG_LEVEL)); // Set log level to DEBUG_LEVEL
    }

    virtual void SetUp() override
    {

    }

    virtual ~NetworkManagerEventTest() override
    {
        plugin->Deinitialize(&service);

        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        IarmBus::setImpl(nullptr);
         if (p_iarmBusImplMock != nullptr)
        {
            delete p_iarmBusImplMock;
            p_iarmBusImplMock = nullptr;
        }

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }
    }
};

TEST_F(NetworkManagerEventTest, iarmEventHandlerErrorChack)
{
    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t eventData = { .interface = "wlan0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS, &eventData, 0);

     eventData = { .interface = "eth0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERNET_CONNECTION_CHANGED, &eventData, sizeof(eventData)); 
}

TEST_F(NetworkManagerEventTest, onInterfaceStateChange)
{
    Core::Event onInterfaceStateChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onInterfaceStateChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
        [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
            string text;
            EXPECT_TRUE(json->ToString(text));
            EXPECT_EQ(text, string(_T("{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.NetworkManager.onInterfaceStateChange\",\"params\":{\"state\":0,\"status\":\"INTERFACE_ADDED\",\"interface\":\"wlan0\"}}")));
            onInterfaceStateChange.SetEvent();
            return Core::ERROR_NONE;
        }))
            .WillOnce(::testing::Invoke(
        [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
            string text;
            EXPECT_TRUE(json->ToString(text));
            EXPECT_EQ(text, string(_T("{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.NetworkManager.onInterfaceStateChange\",\"params\":{\"state\":0,\"status\":\"INTERFACE_ADDED\",\"interface\":\"eth0\"}}")));
            onInterfaceStateChange.SetEvent();
            return Core::ERROR_NONE;
        }));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t eventData = { .interface = "wlan0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS, &eventData, sizeof(eventData));

     eventData = { .interface = "eth0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_ENABLED_STATUS, &eventData, sizeof(eventData));

    EXPECT_EQ(Core::ERROR_NONE, onInterfaceStateChange.Lock());

    EVENT_UNSUBSCRIBE(2, _T("onInterfaceStateChange"), _T("org.rdk.NetworkManager"), message);
}

TEST_F(NetworkManagerEventTest, onActiveInterfaceChange)
{
    Core::Event onActiveInterfaceChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onActiveInterfaceChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onActiveInterfaceChange\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"prevActiveInterface\":\"eth0\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"currentActiveInterface\":\"wlan0\"") != std::string::npos);
                onActiveInterfaceChange.SetEvent();
                return Core::ERROR_NONE;
            }));

    IARM_BUS_NetSrvMgr_Iface_EventDefaultInterface_t eventData;
    strcpy(eventData.oldInterface, "eth0");
    strcpy(eventData.newInterface, "wlan0");
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_DEFAULT_INTERFACE, &eventData, sizeof(eventData));

    EXPECT_EQ(Core::ERROR_NONE, onActiveInterfaceChange.Lock());
    EVENT_UNSUBSCRIBE(2, _T("onActiveInterfaceChange"), _T("org.rdk.NetworkManager"), message);
}

TEST_F(NetworkManagerEventTest, onWiFiStateChange)
{
    Core::Event onWiFiStateChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onWiFiStateChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onWiFiStateChange\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"state\":5") != std::string::npos); // WIFI_STATE_CONNECTED = 4
                EXPECT_TRUE(text.find("\"status\":\"WIFI_STATE_CONNECTED\"") != std::string::npos);
                onWiFiStateChange.SetEvent();
                return Core::ERROR_NONE;
            }));

    EXPECT_CALL(*p_wrapsImplMock, popen(::testing::_, ::testing::_))
    .Times(2)
    .WillOnce(::testing::Invoke(
            [&](const char* command, const char* type) -> FILE* {
            EXPECT_THAT(string(command), ::testing::MatchesRegex("wpa_cli status"));
            // Create a temporary file with the mock output
            FILE* tempFile = tmpfile();
            if (tempFile) {
                fputs("Selected interface 'wlan0'\n"
                      "bssid=aa:bb:cc:dd:ee:ff\n"
                      "freq=5462\n"
                      "ssid=dummySSID\n", tempFile);
                rewind(tempFile);
            }
            return tempFile;
        }))
        .WillOnce(::testing::Invoke(
            [&](const char* command, const char* type) -> FILE* {
            EXPECT_THAT(string(command), ::testing::MatchesRegex("wpa_cli bss aa:bb:cc:dd:ee:ff"));
            FILE* tempFile = tmpfile();
            if (tempFile) {
                fputs("Selected interface 'wlan0'\n"
                    "ssid=dummySSID\n"
                    "noise=-114\n"
                    "level=-90\n"
                    "snr=33\n", tempFile);
                rewind(tempFile);
            }
            return tempFile;
        }));

    // Create and populate event data for WiFi state change
    IARM_BUS_WiFiSrvMgr_EventData_t eventData;
    eventData.data.wifiStateChange.state = WIFI_CONNECTED;
    
    // Trigger the event handler
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onWIFIStateChanged, &eventData, sizeof(eventData));
    EXPECT_EQ(Core::ERROR_NONE, onWiFiStateChange.Lock());
    EVENT_UNSUBSCRIBE(2, _T("onWiFiStateChange"), _T("org.rdk.NetworkManager"), message);
}

TEST_F(NetworkManagerEventTest, onWiFiErrorStateChange)
{
    Core::Event onWiFiStateChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onWiFiStateChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onWiFiStateChange\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"state\":8") != std::string::npos); // WIFI_STATE_CONNECTION_LOST = 8
                EXPECT_TRUE(text.find("\"status\":\"WIFI_STATE_CONNECTION_LOST\"") != std::string::npos);
                onWiFiStateChange.SetEvent();
                return Core::ERROR_NONE;
            }));

    // Create and populate event data for WiFi error
    IARM_BUS_WiFiSrvMgr_EventData_t eventData;
    eventData.data.wifiError.code = WIFI_CONNECTION_LOST;
    
    // Trigger the event handler
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onError, &eventData, sizeof(eventData));

    EXPECT_EQ(Core::ERROR_NONE, onWiFiStateChange.Lock());

    EVENT_UNSUBSCRIBE(2, _T("onWiFiStateChange"), _T("org.rdk.NetworkManager"), message);
}

TEST_F(NetworkManagerEventTest, onAvailableSSIDsEvent)
{
    Core::Event onAvailableSSIDs(false, true);
    EVENT_SUBSCRIBE(2, _T("onAvailableSSIDs"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onAvailableSSIDs\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"ssids\"") != std::string::npos);
                onAvailableSSIDs.SetEvent();
                return Core::ERROR_NONE;
            }));

    // Create a simple JSON string with SSID list
    const char* jsonData = R"({
        "getAvailableSSIDs": [
            {
                "ssid": "HomeWiFi",
                "security": 0,
                "signalStrength": -65,
                "frequency": 2.4
            },
            {
                "ssid": "OfficeNetwork",
                "security": 4,
                "signalStrength": -72,
                "frequency": 5.0
            }
        ]
    })";

    // Create and populate event data for WiFi SSIDs
    IARM_BUS_WiFiSrvMgr_EventData_t eventData;
    strncpy(eventData.data.wifiSSIDList.ssid_list, jsonData, sizeof(eventData.data.wifiSSIDList.ssid_list));
    
    // Trigger the event handler
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_EVENT_onAvailableSSIDs, &eventData, sizeof(eventData));

    EXPECT_EQ(Core::ERROR_NONE, onAvailableSSIDs.Lock());

    EVENT_UNSUBSCRIBE(2, _T("onAvailableSSIDs"), _T("org.rdk.NetworkManager"), message);
}


TEST_F(NetworkManagerEventTest, ConnectivityMonitorEvents)
{
    Core::Event onAddressChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onIPAddressChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onIPAddressChange\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"interface\":\"eth0\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"ipversion\":\"IPv4\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"ipaddress\":\"192.168.1.100\"") != std::string::npos);
                EXPECT_TRUE(text.find("\"status\":\"LOST\"") != std::string::npos);
                onAddressChange.SetEvent();
                return Core::ERROR_NONE;
            }));

    // Create and populate event data for IP address change
    IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t eventData{.interface = "eth0",
                                                                .ip_address = "192.168.1.100",
                                                                .is_ipv6 = false,
                                                                .acquired = false};
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, &eventData, sizeof(eventData));
    EXPECT_EQ(Core::ERROR_NONE, onAddressChange.Lock());
    EVENT_UNSUBSCRIBE(2, _T("onIPAddressChange"), _T("org.rdk.NetworkManager"), message);
}

TEST_F(NetworkManagerEventTest, onInterfaceStateChange2)
{
    Core::Event onInterfaceStateChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onInterfaceStateChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
        [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
            string text;
            EXPECT_TRUE(json->ToString(text));
            EXPECT_EQ(text, string(_T("{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.NetworkManager.onInterfaceStateChange\",\"params\":{\"state\":1,\"status\":\"INTERFACE_LINK_UP\",\"interface\":\"wlan0\"}}")));
            onInterfaceStateChange.SetEvent();
            return Core::ERROR_NONE;
        }))
            .WillOnce(::testing::Invoke(
        [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
            string text;
            EXPECT_TRUE(json->ToString(text));
            EXPECT_EQ(text, string(_T("{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.NetworkManager.onInterfaceStateChange\",\"params\":{\"state\":2,\"status\":\"INTERFACE_LINK_DOWN\",\"interface\":\"eth0\"}}")));
            onInterfaceStateChange.SetEvent();
            return Core::ERROR_NONE;
        }));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t eventData = { .interface = "wlan0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &eventData, sizeof(eventData));

     eventData = { .interface = "eth0", .status = false };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &eventData, sizeof(eventData));

    EXPECT_EQ(Core::ERROR_NONE, onInterfaceStateChange.Lock());

    EVENT_UNSUBSCRIBE(2, _T("onInterfaceStateChange"), _T("org.rdk.NetworkManager"), message);

}

TEST_F(NetworkManagerEventTest, onInternetStatusChange_CaptivePortal)
{
    Core::Event onInternetStatusChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onInternetStatusChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onInternetStatusChange\"") != std::string::npos);
                onInternetStatusChange.SetEvent();
                return Core::ERROR_NONE;
            }));

    SimpleHttpServer server(8080);
    server.setHttpResponseCode(302); // Found (Redirect)
    server.setHttpMessage("http://captive.portal/login");
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow server to start

    std::string request = _T("{\"endpoints\":[\"http://localhost:8080/generate_204\"]}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t ifaceEventData = { .interface = "wlan0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &ifaceEventData, sizeof(ifaceEventData));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t ipEventData{.interface = "wlan0",
                                                                .ip_address = "fe80::1889:1e36:52a:a82f%26",
                                                                .is_ipv6 = true,
                                                                .acquired = true};
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, &ipEventData, sizeof(ipEventData));

    EXPECT_EQ(Core::ERROR_NONE, onInternetStatusChange.Lock());
    EVENT_UNSUBSCRIBE(2, _T("onInternetStatusChange"), _T("org.rdk.NetworkManager"), message);

        // Test with both IPv4 and IPv6
    // EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("IsConnectedToInternet"), _T("{}"), response));
    // EXPECT_EQ(response, _T("{\"ipversion\":\"IPv4\",\"interface\":\"wlan0\",\"connected\":false,\"state\":0,\"status\":\"NO_INTERNET\",\"success\":true}"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetCaptivePortalURI"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"uri\":\"\",\"success\":true}"));
    server.stop();
}

TEST_F(NetworkManagerEventTest, onInternetStatusChange_LimitedInternet)
{
    Core::Event onInternetStatusChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onInternetStatusChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onInternetStatusChange\"") != std::string::npos);
                onInternetStatusChange.SetEvent();
                return Core::ERROR_NONE;
            }));

    SimpleHttpServer server(8080);
    server.setHttpResponseCode(200); // OK  
    server.setHttpMessage("");
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow server to start

    std::string request = _T("{\"endpoints\":[\"http://localhost:8080/generate_204\"]}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t ifaceEventData = { .interface = "eth0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &ifaceEventData, sizeof(ifaceEventData));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t ipEventData{.interface = "eth0",
                                                                .ip_address = "192.168.148.1",
                                                                .is_ipv6 = false,
                                                                .acquired = true};
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, &ipEventData, sizeof(ipEventData));

    EXPECT_EQ(Core::ERROR_NONE, onInternetStatusChange.Lock());
    EVENT_UNSUBSCRIBE(2, _T("onInternetStatusChange"), _T("org.rdk.NetworkManager"), message);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow server to start
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("IsConnectedToInternet"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ipversion\":\"IPv4\",\"interface\":\"eth0\",\"connected\":false,\"state\":1,\"status\":\"LIMITED_INTERNET\",\"success\":true}"));
    server.stop();
}

TEST_F(NetworkManagerEventTest, onInternetStatusChange_FULLY_CONNECTED)
{
    Core::Event onInternetStatusChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onInternetStatusChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onInternetStatusChange\"") != std::string::npos);
                onInternetStatusChange.SetEvent();
                return Core::ERROR_NONE;
            }));

    SimpleHttpServer server(8080);
    server.setHttpResponseCode(204); // No Content
    server.setHttpMessage("");
    server.start();
  
    std::string request = _T("{\"endpoints\":[\"http://localhost:8080/generate_204\"]}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow server to start

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t ifaceEventData = { .interface = "eth0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &ifaceEventData, sizeof(ifaceEventData));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t ipEventData{.interface = "eth0",
                                                                .ip_address = "192.168.148.1",
                                                                .is_ipv6 = false,
                                                                .acquired = true};
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, &ipEventData, sizeof(ipEventData));

    EXPECT_EQ(Core::ERROR_NONE, onInternetStatusChange.Lock());
    EVENT_UNSUBSCRIBE(2, _T("onInternetStatusChange"), _T("org.rdk.NetworkManager"), message);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow server to start
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("IsConnectedToInternet"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ipversion\":\"IPv4\",\"interface\":\"eth0\",\"connected\":true,\"state\":3,\"status\":\"FULLY_CONNECTED\",\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetCaptivePortalURI"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"uri\":\"\",\"success\":true}"));

    server.stop();
}

TEST_F(NetworkManagerEventTest, GetPublicIP_SuccessWithEvent)
{
    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t ifaceEventData = { .interface = "wlan0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &ifaceEventData, sizeof(ifaceEventData));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t ipEventData{.interface = "wlan0",
                                                                .ip_address = "fe80::1889:1e36:52a:a82f%26",
                                                                .is_ipv6 = true,
                                                                .acquired = true};
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, &ipEventData, sizeof(ipEventData));

        // Test with both IPv4 and IPv6
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetPublicIP"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipversion\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
}

TEST_F(NetworkManagerEventTest, GetSupportedSecurityModes)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetSupportedSecurityModes"), _T(""), response));
    EXPECT_EQ(response, _T("{\"security\":{\"NONE\":0,\"WPA_PSK\":1,\"SAE\":2,\"EAP\":3},\"success\":true}"));
}

/*
TEST_F(NetworkManagerEventTest, IsConnectedToInternet_Failed)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow server to start
    std::string request = _T("{\"endpoints\":[\"http://localhost:8077/generate_204\"]}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow server to start

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t ifaceEventData = { .interface = "wlan0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &ifaceEventData, sizeof(ifaceEventData));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t ipEventData{.interface = "wlan0",
                                                                .ip_address = "fe80::1889:1e36:52a:a82f%26",
                                                                .is_ipv6 = true,
                                                                .acquired = true};
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, &ipEventData, sizeof(ipEventData));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("IsConnectedToInternet"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ipversion\":\"IPv4\",\"interface\":\"wlan0\",\"connected\":false,\"state\":0,\"status\":\"NO_INTERNET\",\"success\":true}"));
    
    // Test with IPv4 only
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("IsConnectedToInternet"), _T("{\"ipversion\":\"IPv6\"}"), response));
    EXPECT_EQ(response, _T("{\"ipversion\":\"IPv6\",\"interface\":\"wlan0\",\"connected\":false,\"state\":0,\"status\":\"NO_INTERNET\",\"success\":true}"));

    // Test with specific interface
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("IsConnectedToInternet"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_EQ(response, _T("{\"ipversion\":\"IPv4\",\"interface\":\"eth0\",\"connected\":false,\"state\":0,\"status\":\"NO_INTERNET\",\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("IsConnectedToInternet"), _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_EQ(response, _T("{\"ipversion\":\"IPv4\",\"interface\":\"wlan0\",\"connected\":false,\"state\":0,\"status\":\"NO_INTERNET\",\"success\":true}"));

}

TEST_F(NetworkManagerEventTest, FULLY_CONNECTED_wait)
{
    Core::Event onInternetStatusChange(false, true);
    EVENT_SUBSCRIBE(2, _T("onInternetStatusChange"), _T("org.rdk.NetworkManager"), message);

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_TRUE(text.find("\"method\":\"org.rdk.NetworkManager.onInternetStatusChange\"") != std::string::npos);
                onInternetStatusChange.SetEvent();
                return Core::ERROR_NONE;
            }));

    SimpleHttpServer server(8080);
    server.setHttpResponseCode(204); // No Content
    server.setHttpMessage("");
    server.start();

    std::string request = _T("{\"endpoints\":[\"http://localhost:8080/generate_204\"]}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow server to start

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t ifaceEventData = { .interface = "eth0", .status = true };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &ifaceEventData, sizeof(ifaceEventData));

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceIPAddress_t ipEventData{.interface = "eth0",
                                                                .ip_address = "192.168.148.1",
                                                                .is_ipv6 = false,
                                                                .acquired = true};
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS, &ipEventData, sizeof(ipEventData));
    std::this_thread::sleep_for(std::chrono::milliseconds(15000)); // Allow server to start
    server.setTimeout(15000); // Set timeout to 15 seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    server.stop();

    IARM_BUS_NetSrvMgr_Iface_EventInterfaceConnectionStatus_t ifaceEventDataLost = { .interface = "eth0", .status = false };
    _nmEventHandler(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_CONNECTION_STATUS, &ifaceEventDataLost, sizeof(ifaceEventDataLost));
     std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(Core::ERROR_NONE, onInternetStatusChange.Lock());
    EVENT_UNSUBSCRIBE(2, _T("onInternetStatusChange"), _T("org.rdk.NetworkManager"), message);
}
*/

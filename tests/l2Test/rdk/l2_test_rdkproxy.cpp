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
#include <algorithm>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
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

using namespace WPEFramework;
using ::testing::NiceMock;

class NetworkManagerTest : public ::testing::Test {
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

    NetworkManagerTest()
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
                "   \"endpoint_1\":\"http://clients3.google.com/generate_204\","
                "   \"interval\":60"
                "  },"
                "  \"stun\":{"
                "   \"endpoint\":\"stun.l.google.com\","
                "   \"port\":19302,"
                "   \"interval\":30"
                "  },"
                "  \"loglevel\":3"
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
        ON_CALL(*p_iarmBusImplMock, IARM_Bus_Init(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME)))
            .WillByDefault(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);
        response = plugin->Initialize(&service);
        EXPECT_EQ(string(""), response);
        NetworkManagerLogger::SetLevel(static_cast<NetworkManagerLogger::LogLevel>(NetworkManagerLogger::INFO_LEVEL)); // Set log level to DEBUG_LEVEL
    }

    virtual void SetUp() override
    {
        // remove any previous connectivity endpoints file
    }

    virtual ~NetworkManagerTest() override
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

TEST_F(NetworkManagerTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("SetLogLevel")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetLogLevel")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetAvailableInterfaces")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetPrimaryInterface")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetInterfaceState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("SetInterfaceState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetIPSettings")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("SetIPSettings")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetStunEndpoint")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("SetStunEndpoint")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetConnectivityTestEndpoints")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("SetConnectivityTestEndpoints")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("IsConnectedToInternet")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetCaptivePortalURI")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetPublicIP")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("Ping")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("Trace")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("StartWiFiScan")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("StopWiFiScan")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetKnownSSIDs")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("AddToKnownSSIDs")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("RemoveKnownSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("WiFiConnect")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("WiFiDisconnect")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetConnectedSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("StartWPS")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("StopWPS")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetWifiState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetWiFiSignalQuality")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("GetSupportedSecurityModes")));
}

TEST_F(NetworkManagerTest, LogLevel)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetLogLevel"), _T("{\"level\":1}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetLogLevel"), _T(""), response));
    EXPECT_EQ(response, _T("{\"level\":1,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetLogLevel"), _T("{\"level\":2}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetLogLevel"), _T(""), response));
    EXPECT_EQ(response, _T("{\"level\":2,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetLogLevel"), _T("{\"level\":3}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetLogLevel"), _T(""), response));
    EXPECT_EQ(response, _T("{\"level\":3,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetLogLevel"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetLogLevel"), _T(""), response));
    EXPECT_EQ(response, _T("{\"level\":3,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetLogLevel"), _T("{\"level\":4}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetLogLevel"), _T(""), response));
    EXPECT_EQ(response, _T("{\"level\":4,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetLogLevel"), _T("{\"levelInvalid\":4}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetAvailableInterfaces)
{
    IARM_BUS_NetSrvMgr_InterfaceList_t mockList = {};
    mockList.size = 2;
    // eth0
    strcpy(mockList.interfaces[0].name, "eth0");
    strcpy(mockList.interfaces[0].mac, "AA:AA:AA:AA:AA:AA");
    mockList.interfaces[0].flags = IFF_UP | IFF_RUNNING; // enabled and connected
    // wlan0
    strcpy(mockList.interfaces[1].name, "wlan0");
    strcpy(mockList.interfaces[1].mac, "BB:BB:BB:BB:BB:BB");
    mockList.interfaces[1].flags = IFF_UP; // enabled, not connected

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_getInterfaceList),
                                                 ::testing::NotNull(), sizeof(mockList)))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockList](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockList, sizeof(mockList));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T(""), response));
    std::string expectedResponse =
        _T("{\"interfaces\":[")
        _T("{\"type\":\"ETHERNET\",\"name\":\"eth0\",\"mac\":\"AA:AA:AA:AA:AA:AA\",\"enabled\":true,\"connected\":true},")
        _T("{\"type\":\"WIFI\",\"name\":\"wlan0\",\"mac\":\"BB:BB:BB:BB:BB:BB\",\"enabled\":true,\"connected\":false}")
        _T("],\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, GetAvailableInterfaces_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_getInterfaceList),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_BothInterfacesDown)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetPrimaryInterface"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"interface\":\"\",\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetInterfaceState)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_setInterfaceEnabled),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([](const char*, const char*, void* arg, size_t) {
                IARM_BUS_NetSrvMgr_Iface_EventData_t* ifaceData = 
                    static_cast<IARM_BUS_NetSrvMgr_Iface_EventData_t*>(arg);
                // Verify the parameters
                EXPECT_TRUE(ifaceData->isInterfaceEnabled);
                EXPECT_TRUE(ifaceData->persist);
                EXPECT_STREQ(ifaceData->setInterface, "WIFI"); // RDKProxy converts wlan0 to WIFI
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), 
        _T("{\"interface\":\"wlan0\",\"enabled\":true}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_NETSRVMGR_API_setInterfaceEnabled),
                                                ::testing::NotNull(), ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::Invoke([](const char*, const char*, void* arg, size_t) {
            IARM_BUS_NetSrvMgr_Iface_EventData_t* ifaceData = 
                static_cast<IARM_BUS_NetSrvMgr_Iface_EventData_t*>(arg);
            // Verify the parameters
            EXPECT_TRUE(ifaceData->isInterfaceEnabled);
            EXPECT_TRUE(ifaceData->persist);
            EXPECT_STREQ(ifaceData->setInterface, "ETHERNET"); // RDKProxy converts eth0 to ETHERNET
            return IARM_RESULT_SUCCESS;
        })
    ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), 
        _T("{\"interface\":\"eth0\",\"enabled\":true}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetInterfaceState_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_setInterfaceEnabled),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), 
        _T("{\"interface\":\"wlan0\",\"enabled\":true}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetInterfaceState_wifi)
{
    IARM_BUS_NetSrvMgr_Iface_EventData_t mockIfaceData = {};
    strcpy(mockIfaceData.setInterface, "WIFI"); // Interface is converted to WIFI for wlan0
    mockIfaceData.isInterfaceEnabled = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_isInterfaceEnabled),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockIfaceData](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockIfaceData, sizeof(mockIfaceData));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), 
        _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_EQ(response, _T("{\"enabled\":true,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetInterfaceState_eth0)
{
    IARM_BUS_NetSrvMgr_Iface_EventData_t mockIfaceData = {};
    strcpy(mockIfaceData.setInterface, "ETHERNET"); // Interface is converted to ETHERNET for eth0
    mockIfaceData.isInterfaceEnabled = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_isInterfaceEnabled),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockIfaceData](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockIfaceData, sizeof(mockIfaceData));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), 
        _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_EQ(response, _T("{\"enabled\":true,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetIPSettings)
{
    IARM_BUS_NetSrvMgr_Iface_Settings_t mockSettings = {};
    strcpy(mockSettings.interface, "eth0");
    strcpy(mockSettings.ipversion, "IPv4");
    mockSettings.autoconfig = true;
    strcpy(mockSettings.ipaddress, "192.168.1.100");
    strcpy(mockSettings.netmask, "255.255.255.0");
    strcpy(mockSettings.gateway, "192.168.1.1");
    strcpy(mockSettings.primarydns, "8.8.8.8");
    strcpy(mockSettings.secondarydns, "8.8.4.4");
    mockSettings.isSupported = true;
    mockSettings.errCode = NETWORK_IPADDRESS_ACQUIRED;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_getIPSettings),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockSettings](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockSettings, sizeof(mockSettings));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), 
        _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\"}"), response));

    // The response should include all IP settings in JSON format
    std::string expectedResponse = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"ula\":\"\",\"dhcpserver\":\"\",\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\",\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, GetIPSettings_IPv6)
{
    IARM_BUS_NetSrvMgr_Iface_Settings_t mockSettings = {};
    strcpy(mockSettings.interface, "eth0");
    strcpy(mockSettings.ipversion, "IPv6");
    mockSettings.autoconfig = true;
    strcpy(mockSettings.ipaddress, "2001:db8::1");
    strcpy(mockSettings.netmask, "64");
    strcpy(mockSettings.gateway, "2001:db8::fe");
    strcpy(mockSettings.primarydns, "2001:4860:4860::8888");
    strcpy(mockSettings.secondarydns, "2001:4860:4860::8844");
     mockSettings.isSupported = true;
    mockSettings.errCode = NETWORK_IPADDRESS_ACQUIRED;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_getIPSettings),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockSettings](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockSettings, sizeof(mockSettings));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), 
        _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv6\"}"), response));

    // The response should include all IP settings in JSON format
    std::string expectedResponse = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv6\",\"autoconfig\":true,\"ipaddress\":\"2001:db8::1\",\"prefix\":64,\"ula\":\"\",\"dhcpserver\":\"\",\"gateway\":\"2001:db8::fe\",\"primarydns\":\"2001:4860:4860::8888\",\"secondarydns\":\"2001:4860:4860::8844\",\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, GetIPSettings_NoInterfaceNoIpVersion) {
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_StaticIPv4)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_setIPSettings),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([](const char*, const char*, void* arg, size_t) {
                IARM_BUS_NetSrvMgr_Iface_Settings_t* settings = 
                    static_cast<IARM_BUS_NetSrvMgr_Iface_Settings_t*>(arg);
                
                // Verify the parameters
                EXPECT_STREQ(settings->interface, "ETHERNET");
                EXPECT_STREQ(settings->ipversion, "IPv4");
                EXPECT_EQ(settings->autoconfig, false);
                EXPECT_STREQ(settings->ipaddress, "192.168.1.100");
                EXPECT_STREQ(settings->netmask, "255.255.255.0");
                EXPECT_STREQ(settings->gateway, "192.168.1.1");
                EXPECT_STREQ(settings->primarydns, "8.8.8.8");
                EXPECT_STREQ(settings->secondarydns, "8.8.4.4");
                
                return IARM_RESULT_SUCCESS;
            })
        ));

    // Test setting static IP configuration
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_DHCP)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_setIPSettings),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([](const char*, const char*, void* arg, size_t) {
                IARM_BUS_NetSrvMgr_Iface_Settings_t* settings = 
                    static_cast<IARM_BUS_NetSrvMgr_Iface_Settings_t*>(arg);
                
                // Verify the parameters
                EXPECT_STREQ(settings->interface, "ETHERNET");
                EXPECT_STREQ(settings->ipversion, "IPv4");
                EXPECT_EQ(settings->autoconfig, true);
                
                return IARM_RESULT_SUCCESS;
            })
        ));

    // Test setting DHCP configuration
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_WiFi)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_setIPSettings),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([](const char*, const char*, void* arg, size_t) {
                IARM_BUS_NetSrvMgr_Iface_Settings_t* settings = 
                    static_cast<IARM_BUS_NetSrvMgr_Iface_Settings_t*>(arg);
                
                // Verify the parameters for WiFi interface
                EXPECT_STREQ(settings->interface, "WIFI");
                EXPECT_STREQ(settings->ipversion, "IPv4");
                EXPECT_EQ(settings->autoconfig, false);
                EXPECT_STREQ(settings->ipaddress, "192.168.1.100");
                EXPECT_STREQ(settings->netmask, "255.255.255.0");
                EXPECT_STREQ(settings->gateway, "192.168.1.1");
                
                return IARM_RESULT_SUCCESS;
            })
        ));

    std::string request = _T("{\"interface\":\"wlan0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_InvalidInterface)
{
    // Test with an invalid interface name
    std::string request = _T("{\"interface\":\"invalid0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_InvalidIPVersion)
{
    // Test with an invalid IP version
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv5\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_IPv6NotSupported)
{
    // Test with IPv6, which is marked as not supported in the implementation
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv6\",\"autoconfig\":false,\"ipaddress\":\"2001:db8::1\",\"prefix\":64,\"gateway\":\"2001:db8::1:1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_InvalidPrefix)
{
    // Test with an invalid prefix value (greater than 32 for IPv4)
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":33,\"gateway\":\"192.168.1.1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_InvalidIPFormat)
{
    // Test with an invalid IP address format
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1\",\"prefix\":24,\"gateway\":\"192.168.1.1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_SameIPAndGateway)
{
    // Test when IP and gateway are the same (should fail validation)
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.1\",\"prefix\":24,\"gateway\":\"192.168.1.1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_DifferentBroadcastDomain)
{
    // Test when IP and gateway are not in the same broadcast domain
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.2.1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_IPEqualsBroadcast)
{
    // Test when IP equals broadcast address
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.255\",\"prefix\":24,\"gateway\":\"192.168.1.1\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_GatewayEqualsBroadcast)
{
    // Test when gateway equals broadcast address
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.255\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_APICallFailed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_NETSRVMGR_API_setIPSettings),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, StartWiFiScan_Success)
{
    IARM_Bus_WiFiSrvMgr_SsidList_Param_t mockParam = {};
    mockParam.status = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getAvailableSSIDsAsync),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockParam, sizeof(mockParam));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), 
        _T("{\"frequency\":[\"2.4\"]}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, StartWiFiScan_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getAvailableSSIDsAsync),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), 
        _T("{\"frequency\":[\"2.4\"]}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, StopWiFiScan_Success)
{
    IARM_Bus_WiFiSrvMgr_Param_t mockParam = {};
    mockParam.status = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_stopProgressiveWifiScanning),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockParam, sizeof(mockParam));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StopWiFiScan"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, StopWiFiScan_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_stopProgressiveWifiScanning),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StopWiFiScan"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetKnownSSIDs_Success)
{
    IARM_Bus_WiFiSrvMgr_Param_t mockParam = {};
    mockParam.status = true;
    strncpy(mockParam.data.getPairedSSID.ssid, "TestNetwork", SSID_SIZE - 1);
    
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getPairedSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockParam, sizeof(mockParam));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetKnownSSIDs"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssids\":[\"TestNetwork\"],\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetKnownSSIDs_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getPairedSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetKnownSSIDs"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, AddToKnownSSIDs_Success)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_saveSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([](const char*, const char*, void* arg, size_t) {
                IARM_Bus_WiFiSrvMgr_Param_t* param = static_cast<IARM_Bus_WiFiSrvMgr_Param_t*>(arg);
                
                // Verify the parameters
                EXPECT_STREQ(param->data.connect.ssid, "TestNetwork");
                EXPECT_STREQ(param->data.connect.passphrase, "TestPassword");
                EXPECT_EQ(param->data.connect.security_mode, NET_WIFI_SECURITY_WPA2_PSK_AES);
                param->status = true;

                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("AddToKnownSSIDs"), 
        _T("{\"ssid\":\"TestNetwork\",\"passphrase\":\"TestPassword\",\"security\":1}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, AddToKnownSSIDs_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_saveSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("AddToKnownSSIDs"), 
        _T("{\"ssid\":\"TestNetwork\",\"passphrase\":\"TestPassword\",\"security\":1}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, RemoveKnownSSID_Success)
{
    IARM_Bus_WiFiSrvMgr_Param_t mockParam = {};
    mockParam.status = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_clearSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                IARM_Bus_WiFiSrvMgr_Param_t* param = static_cast<IARM_Bus_WiFiSrvMgr_Param_t*>(arg);
                param->status = true;
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("RemoveKnownSSID"), 
        _T("{\"ssid\":\"TestNetwork\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, RemoveKnownSSID_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_clearSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("RemoveKnownSSID"), 
        _T("{\"ssid\":\"TestNetwork\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, WiFiConnect_Success)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_connect),
                                                ::testing::NotNull(), ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::Invoke([](const char*, const char*, void* arg, size_t) {
            IARM_Bus_WiFiSrvMgr_Param_t* param = static_cast<IARM_Bus_WiFiSrvMgr_Param_t*>(arg);
            
            // Verify the parameters
            EXPECT_STREQ(param->data.connect.ssid, "TestNetwork");
            EXPECT_STREQ(param->data.connect.passphrase, "TestPassword");
            EXPECT_EQ(param->data.connect.security_mode, NET_WIFI_SECURITY_WPA2_PSK_AES);
            param->status = true;
            return IARM_RESULT_SUCCESS;
        })
    ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiConnect"), 
        _T("{\"ssid\":\"TestNetwork\",\"passphrase\":\"TestPassword\",\"security\":1}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, WiFiConnectEAP_Success)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_connect),
                                                ::testing::NotNull(), ::testing::_))
    .WillOnce(::testing::DoAll(
        ::testing::Invoke([](const char*, const char*, void* arg, size_t) {
            IARM_Bus_WiFiSrvMgr_Param_t* param = static_cast<IARM_Bus_WiFiSrvMgr_Param_t*>(arg);
            
            // Verify the parameters
            EXPECT_STREQ(param->data.connect.ssid, "TestNetwork");
            EXPECT_STREQ(param->data.connect.passphrase, "TestPassword");
            EXPECT_STREQ(param->data.connect.clientcert, "test_client_cert");
            EXPECT_STREQ(param->data.connect.privatekey, "test_private_key");
            EXPECT_EQ(param->data.connect.security_mode, NET_WIFI_SECURITY_WPA_WPA2_ENTERPRISE);
            EXPECT_TRUE(param->data.connect.persistSSIDInfo);
            param->status = true;
            return IARM_RESULT_SUCCESS;
        })
    ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiConnect"), 
        _T("{\"ssid\":\"TestNetwork\",\"passphrase\":\"TestPassword\",\"security\":3,\"ca_cert\":\"test_ca_cert\",\"client_cert\":\"test_client_cert\",\"private_key\":\"test_private_key\",\"private_key_passwd\":\"test_private_key_passwd\",\"eap\":\"test_eap\",\"eap_identity\":\"test_eap_identity\",\"eap_password\":\"test_eap_password\",\"eap_phase1\":\"test_eap_phase1\",\"eap_phase2\":\"test_eap_phase2\",\"persist\":true}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, WiFiConnect_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_connect),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiConnect"), 
        _T("{\"ssid\":\"TestNetwork\",\"passphrase\":\"TestPassword\",\"security\":1}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, WiFiDisconnect_Success)
{
    IARM_Bus_WiFiSrvMgr_Param_t mockParam = {};
    mockParam.status = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_disconnectSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockParam, sizeof(mockParam));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiDisconnect"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, WiFiDisconnect_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_disconnectSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiDisconnect"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetConnectedSSID_Success)
{
    IARM_Bus_WiFiSrvMgr_Param_t mockParam = {};
    mockParam.status = true;
    strncpy(mockParam.data.getConnectedSSID.ssid, "TestNetwork", SSID_SIZE - 1);
    mockParam.data.getConnectedSSID.securityMode = NET_WIFI_SECURITY_WPA2_PSK_AES;
    mockParam.data.getConnectedSSID.signalStrength = -30.00; // Example signal strength
    mockParam.data.getConnectedSSID.frequency = 2412; // Example frequency in MHz
    mockParam.data.getConnectedSSID.rate = 0; // Example rate
    mockParam.data.getConnectedSSID.noise = 6; // Example noise level
    strncpy(mockParam.data.getConnectedSSID.bssid, "AB:CD:EF:GH:IJ:KL", BSSID_BUFF - 1); // Example BSSID
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getConnectedSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockParam, sizeof(mockParam));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"TestNetwork\",\"bssid\":\"AB:CD:EF:GH:IJ:KL\",\"security\":1,\"strength\":-30,\"frequency\":2.412,\"rate\":0,\"noise\":0,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetConnectedSSID_Success_noise_9999)
{
    IARM_Bus_WiFiSrvMgr_Param_t mockParam = {};
    mockParam.status = true;
    strncpy(mockParam.data.getConnectedSSID.ssid, "TestNetwork", SSID_SIZE - 1);
    mockParam.data.getConnectedSSID.securityMode = NET_WIFI_SECURITY_WPA2_PSK_AES;
    mockParam.data.getConnectedSSID.signalStrength = -30.00; // Example signal strength
    mockParam.data.getConnectedSSID.frequency = 2412; // Example frequency in MHz
    mockParam.data.getConnectedSSID.rate = 0; // Example rate
    mockParam.data.getConnectedSSID.noise = 9999; // Example noise level
    strncpy(mockParam.data.getConnectedSSID.bssid, "AB:CD:EF:GH:IJ:KL", BSSID_BUFF - 1); // Example BSSID
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getConnectedSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockParam, sizeof(mockParam));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"TestNetwork\",\"bssid\":\"AB:CD:EF:GH:IJ:KL\",\"security\":1,\"strength\":-30,\"frequency\":2.412,\"rate\":0,\"noise\":0,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetConnectedSSID_Success_noise_100)
{
    IARM_Bus_WiFiSrvMgr_Param_t mockParam = {};
    mockParam.status = true;
    strncpy(mockParam.data.getConnectedSSID.ssid, "TestNetwork", SSID_SIZE - 1);
    mockParam.data.getConnectedSSID.securityMode = NET_WIFI_SECURITY_WPA2_PSK_AES;
    mockParam.data.getConnectedSSID.signalStrength = -30.00; // Example signal strength
    mockParam.data.getConnectedSSID.frequency = 2412; // Example frequency in MHz
    mockParam.data.getConnectedSSID.rate = 0; // Example rate
    mockParam.data.getConnectedSSID.noise = -100; // Example noise level
    strncpy(mockParam.data.getConnectedSSID.bssid, "AB:CD:EF:GH:IJ:KL", BSSID_BUFF - 1); // Example BSSID
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getConnectedSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockParam, sizeof(mockParam));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"TestNetwork\",\"bssid\":\"AB:CD:EF:GH:IJ:KL\",\"security\":1,\"strength\":-30,\"frequency\":2.412,\"rate\":0,\"noise\":-96,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetConnectedSSID_Success_noise_50)
{
    IARM_Bus_WiFiSrvMgr_Param_t mockParam = {};
    mockParam.status = true;
    strncpy(mockParam.data.getConnectedSSID.ssid, "TestNetwork", SSID_SIZE - 1);
    mockParam.data.getConnectedSSID.securityMode = NET_WIFI_SECURITY_WPA2_PSK_AES;
    mockParam.data.getConnectedSSID.signalStrength = -30.00; // Example signal strength
    mockParam.data.getConnectedSSID.frequency = 2412; // Example frequency in MHz
    mockParam.data.getConnectedSSID.rate = 0; // Example rate
    mockParam.data.getConnectedSSID.noise = -50; // Example noise level
    strncpy(mockParam.data.getConnectedSSID.bssid, "AB:CD:EF:GH:IJ:KL", BSSID_BUFF - 1); // Example BSSID
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getConnectedSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockParam](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockParam, sizeof(mockParam));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"TestNetwork\",\"bssid\":\"AB:CD:EF:GH:IJ:KL\",\"security\":1,\"strength\":-30,\"frequency\":2.412,\"rate\":0,\"noise\":-50,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetConnectedSSID_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getConnectedSSID),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, StartWPS_Success)
{
    IARM_Bus_WiFiSrvMgr_WPS_Parameters_t mockWpsParams = {};
    mockWpsParams.pbc = true;
    mockWpsParams.status = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_initiateWPSPairing2),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockWpsParams](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockWpsParams, sizeof(mockWpsParams));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWPS"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, StartWPS_WIFI_WPS_PIN_Success)
{
    IARM_Bus_WiFiSrvMgr_WPS_Parameters_t mockWpsParams = {};
    mockWpsParams.pbc = true;
    mockWpsParams.status = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_initiateWPSPairing2),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockWpsParams](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockWpsParams, sizeof(mockWpsParams));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWPS"), _T("{\"method\":\"PIN\", \"pin\":\"12345678\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, StartWPS_WIFI_WPS_SERIALIZED_PIN_Success)
{
    IARM_Bus_WiFiSrvMgr_WPS_Parameters_t mockWpsParams = {};
    mockWpsParams.pbc = true;
    mockWpsParams.status = true;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_initiateWPSPairing2),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockWpsParams](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockWpsParams, sizeof(mockWpsParams));
                return IARM_RESULT_SUCCESS;
            })
        ));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWPS"), _T("{\"method\":\"SERIALIZED_PIN\", \"pin\":\"12345678\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, StartWPS_Failed)
{
    IARM_Bus_WiFiSrvMgr_WPS_Parameters_t mockWpsParams = {};
    mockWpsParams.pbc = true;
    mockWpsParams.status = false;
    
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_initiateWPSPairing2),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&mockWpsParams](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &mockWpsParams, sizeof(mockWpsParams));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWPS"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, StopWPS_Success)
{
    IARM_Bus_WiFiSrvMgr_Param_t param;
    memset(&param, 0, sizeof(param));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_cancelWPSPairing),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&param](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &param, sizeof(param));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StopWPS"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, StopWPS_Failed)
{
    IARM_Bus_WiFiSrvMgr_Param_t param;
    memset(&param, 0, sizeof(param));
    
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                ::testing::StrEq(IARM_BUS_WIFI_MGR_API_cancelWPSPairing),
                                                ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StopWPS"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetWifiState)
{
    IARM_Bus_WiFiSrvMgr_Param_t param;
    param.data.wifiStatus = WIFI_CONNECTED;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getCurrentState),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::Invoke([&param](const char*, const char*, void* arg, size_t) {
                memcpy(arg, &param, sizeof(param));
                return IARM_RESULT_SUCCESS;
            })
        ));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"state\":5,\"status\":\"WIFI_STATE_CONNECTED\",\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetWifiState_Failed)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME),
                                                 ::testing::StrEq(IARM_BUS_WIFI_MGR_API_getCurrentState),
                                                 ::testing::NotNull(), ::testing::_))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetPublicIP_Failed)
{
    // Mock STUN client behavior for a successful public IP lookup
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetPublicIP"), _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetPublicIP"), _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv6\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetStunEndpoint)
{
    // The default values from the configuration in NetworkManagerTest constructor
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetStunEndpoint"), _T("{}"), response));
    std::string expectedResponse = _T("{\"endpoint\":\"stun.l.google.com\",\"port\":19302,\"timeout\":30,\"cacheLifetime\":0,\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, SetStunEndpoint_Success)
{
    std::string request = _T("{\"endpoint\":\"stun.example.com\",\"port\":3478,\"timeout\":60,\"cacheLifetime\":300}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetStunEndpoint"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    
    // Verify the settings were applied by getting them
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetStunEndpoint"), _T("{}"), response));
    std::string expectedResponse = _T("{\"endpoint\":\"stun.example.com\",\"port\":3478,\"timeout\":60,\"cacheLifetime\":300,\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, SetStunEndpoint_InvalidParams)
{
    // Test with empty request
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetStunEndpoint"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, ConfigLineEmpty)
{
    // Mock the service to return an empty configuration line
    EXPECT_CALL(service, ConfigLine())
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Return(_T("")));

    // Initialize the plugin with the mocked service
    plugin->Initialize(&service);
}

TEST_F(NetworkManagerTest, ComLinkEmpty)
{
    // Mock the service to return an empty configuration line
    ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                    [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        return nullptr;
                }));

    // Initialize the plugin with the mocked service
    plugin->Initialize(&service);
}

TEST_F(NetworkManagerTest, platformInit)
{
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(::testing::StrEq("netsrvmgr-thunder")))
        .Times(2)
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
        .Times(2)
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Call_with_IPCTimeout(::testing::StrEq(IARM_BUS_NM_SRV_MGR_NAME), 
                                                ::testing::StrEq(IARM_BUS_NETSRVMGR_API_isAvailable),
                                                ::testing::NotNull(), ::testing::_, ::testing::_))
        .Times(3)
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL))
        .WillOnce(::testing::Return(IARM_RESULT_IPCCORE_FAIL))
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    plugin->Initialize(&service);
}

TEST_F(NetworkManagerTest, GetConnectivityTestEndpoints)
{
    std::string request = _T("{\"endpoints\":[\"https://example.com/connectivity\",\"https://example.org/check\"]}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow server to start
    // The default values from the configuration in NetworkManagerTest constructor
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectivityTestEndpoints"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"endpoints\":[\"https:\\/\\/example.com\\/connectivity\",\"https:\\/\\/example.org\\/check\"],\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetConnectivityTestEndpoints_Success)
{
    std::string request = _T("{\"endpoints\":[\"https://example.com/connectivity\",\"https://example.org/check\"]}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow server to start
    // Verify the settings were applied by getting them
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectivityTestEndpoints"), _T("{}"), response));
    std::string expectedResponse = _T("{\"endpoints\":[\"https:\\/\\/example.com\\/connectivity\",\"https:\\/\\/example.org\\/check\"],\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, SetConnectivityTestEndpoints_Empty)
{
    // Test with empty endpoints array
    std::string request = _T("{\"endpoints\":[]}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, m_ipv4AddressCache)
{
    Exchange::INetworkManager::IPAddress address{};
    address.ipversion = "IPv4";
    address.autoconfig = true;
    address.dhcpserver = "192.168.1.1";
    address.ula = "";
    address.ipaddress = "192.168.1.100";
    address.prefix = 24;
    address.gateway = "192.168.1.1";
    address.primarydns = "8.8.8.8";
    address.secondarydns = "8.8.4.4";
    plugin->m_ipv4AddressCache = address;
}

TEST_F(NetworkManagerTest, m_ipv6AddressCache)
{
    Exchange::INetworkManager::IPAddress address{};
    address.ipversion = "IPv6";
    address.autoconfig = true;
    address.dhcpserver = "";
    address.ula = "fd00:1234:5678::1";
    address.ipaddress = "2001:db8::1";
    address.prefix = 64;
    address.gateway = "2001:db8::fffe";
    address.primarydns = "2001:4860:4860::8888";
    address.secondarydns = "2001:4860:4860::8844";
    plugin->m_ipv6AddressCache = address;
}

TEST_F(NetworkManagerTest, GetWiFiSignalQualityWpa_cliFailed)
{
    EXPECT_CALL(*p_wrapsImplMock, popen(::testing::_, ::testing::_))
    .Times(1)
    .WillOnce(::testing::Invoke(
            [&](const char* command, const char* type) -> FILE* {
            EXPECT_THAT(string(command), ::testing::MatchesRegex("wpa_cli status"));
            return nullptr;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWiFiSignalQuality"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetWiFiSignalQualityDisconnected2)
{
    EXPECT_CALL(*p_wrapsImplMock, popen(::testing::_, ::testing::_))
    .Times(1)
    .WillOnce(::testing::Invoke(
            [&](const char* command, const char* type) -> FILE* {
            EXPECT_THAT(string(command), ::testing::MatchesRegex("wpa_cli status"));
            // Create a temporary file with the mock output
            FILE* tempFile = tmpfile();
            if (tempFile) {
                fputs("Selected interface 'wlan0'\n"
                      "wpa_state=DISCONNECTED\n"
                      "p2p_device_address=d4:52:ee:d9:0a:39\n"
                      "address=d4:52:ee:d9:0a:39\n"
                      "uuid=feb4fcc9-04c3-5be3-a3f9-03fed1d0604c\n", tempFile);
                rewind(tempFile);
            }
            return tempFile;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWiFiSignalQuality"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"\",\"quality\":\"Disconnected\",\"snr\":0,\"strength\":0,\"noise\":0,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetWiFiSignalQualityConnected)
{
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
                      "freq=2462\n"
                      "ssid=dummySSID\n", tempFile);
                rewind(tempFile);
            }
            return tempFile;
        }))
        .WillOnce(::testing::Invoke(
            [&](const char* command, const char* type) -> FILE* {
            EXPECT_THAT(string(command), ::testing::MatchesRegex("wpa_cli signal_poll"));
            FILE* tempFile = tmpfile();
            if (tempFile) {
                fputs("Selected interface 'wlan0'\n"
                    "RSSI=-30\n"
                    "LINKSPEED=300\n"
                    "NOISE=-114\n"
                    "FREQUENCY=2417\n"
                    "AVG_RSSI=-30\n", tempFile);
                rewind(tempFile);
            }
            return tempFile;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWiFiSignalQuality"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"dummySSID\",\"quality\":\"Excellent\",\"snr\":66,\"strength\":-30,\"noise\":-96,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetWiFiSignalQualityConnectedGood)
{
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
            EXPECT_THAT(string(command), ::testing::MatchesRegex("wpa_cli signal_poll"));
            FILE* tempFile = tmpfile();
            if (tempFile) {
                fputs("Selected interface 'wlan0'\n"
                    "RSSI=\n"
                    "LINKSPEED=300\n"
                    "NOISE=-114\n"
                    "FREQUENCY=2417\n"
                    "AVG_RSSI=-90\n", tempFile);
                rewind(tempFile);
            }
            return tempFile;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWiFiSignalQuality"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"dummySSID\",\"quality\":\"Weak\",\"snr\":6,\"strength\":-90,\"noise\":-96,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetWiFiSignalQualityConnectedLowBad)
{
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
            EXPECT_THAT(string(command), ::testing::MatchesRegex("wpa_cli signal_poll"));
            FILE* tempFile = tmpfile();
            if (tempFile) {
                fputs("Selected interface 'wlan0'\n"
                    "RSSI=\n"
                    "LINKSPEED=300\n"
                    "NOISE=9999\n"
                    "FREQUENCY=5462\n"
                    "AVG_RSSI=-120\n", tempFile);
                rewind(tempFile);
            }
            return tempFile;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWiFiSignalQuality"), _T("{}"), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"dummySSID\",\"quality\":\"Excellent\",\"snr\":120,\"strength\":-120,\"noise\":0,\"success\":true}"));
}

namespace WPEFramework { namespace Plugin { extern NetworkManagerImplementation* _instance; } }

namespace {
    using Plugin::Diagnostics::ProcessResult;

    // Replaces ping/traceroute: records each argv and returns canned output.
    class CannedRunner : public Plugin::Diagnostics::IProcessRunner {
    public:
        CannedRunner(const string& output, const int exitCode, const bool timedOut = false)
        {
            _result.started = true;
            _result.status = exitCode << 8;
            _result.timedOut = timedOut;
            _result.output = output;
        }
        ProcessResult Run(const std::vector<string>& argv, const std::chrono::milliseconds, const std::atomic<bool>&) override
        {
            std::lock_guard<std::mutex> guard(_lock);
            _calls.push_back(argv);
            return _result;
        }
        std::vector<std::vector<string>> Calls()
        {
            std::lock_guard<std::mutex> guard(_lock);
            return _calls;
        }
    private:
        std::mutex _lock;
        ProcessResult _result;
        std::vector<std::vector<string>> _calls;
    };

    // Each Run() blocks until Open() (or engine shutdown), so tests can fill the queue deterministically.
    class GatedRunner : public Plugin::Diagnostics::IProcessRunner {
    public:
        ProcessResult Run(const std::vector<string>&, const std::chrono::milliseconds, const std::atomic<bool>& cancel) override
        {
            std::unique_lock<std::mutex> guard(_lock);
            ++_started;
            _signal.notify_all();
            while (!_open && !cancel.load())
                _signal.wait_for(guard, std::chrono::milliseconds(10));
            ProcessResult result;
            result.started = true;
            result.status = 0;
            return result;
        }
        bool WaitStarted(const int count)
        {
            std::unique_lock<std::mutex> guard(_lock);
            return _signal.wait_for(guard, std::chrono::seconds(5), [&] { return _started >= count; });
        }
        void Open()
        {
            std::lock_guard<std::mutex> guard(_lock);
            _open = true;
            _signal.notify_all();
        }
    private:
        std::mutex _lock;
        std::condition_variable _signal;
        int _started = 0;
        bool _open = false;
    };

    const char* PingOutput =
        "PING 8.8.8.8 (8.8.8.8): 56 data bytes\n"
        "64 bytes from 8.8.8.8: seq=0 ttl=119 time=23.363 ms\n"
        "\n"
        "--- 8.8.8.8 ping statistics ---\n"
        "3 packets transmitted, 3 packets received, 0% packet loss\n"
        "round-trip min/avg/max/mdev = 23.363/23.395/23.440/0.179 ms\n";

    const char* TraceOutput =
        "traceroute to 8.8.8.8 (8.8.8.8), 6 hops max, 52 byte packets\n"
        "1  gateway (10.46.5.1)  0.448 ms\n"
        "2  *\n";

    JsonObject Parse(const string& json)
    {
        JsonObject object;
        object.FromString(json);
        return object;
    }

    Core::JSONRPC::Context DirectCaller()
    {
        return Core::JSONRPC::Context(Plugin::DeferredReply::DirectCallerChannel, 7, "");
    }

    template <typename RUNNER>
    std::shared_ptr<RUNNER> UseRunner(std::shared_ptr<RUNNER> runner)
    {
        EXPECT_NE(nullptr, Plugin::_instance);
        if (Plugin::_instance != nullptr)
            Plugin::_instance->SetDiagnosticsRunner(runner);
        return runner;
    }
}

TEST_F(NetworkManagerTest, Ping_DirectCaller_ReturnsParsedResult)
{
    auto runner = UseRunner(std::make_shared<CannedRunner>(PingOutput, 0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Ping"),
        _T("{\"endpoint\":\"8.8.8.8\",\"ipversion\":\"IPv4\",\"count\":3,\"timeout\":2}"), response));

    JsonObject result = Parse(response);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_EQ("8.8.8.8", result["endpoint"].String());
    EXPECT_EQ(3, result["packetsTransmitted"].Number());
    EXPECT_EQ(3, result["packetsReceived"].Number());
    EXPECT_EQ("23.395", result["tripAvg"].String());

    const auto calls = runner->Calls();
    ASSERT_EQ(1u, calls.size());
    EXPECT_EQ("ping", calls[0].front());
    EXPECT_EQ(1, std::count(calls[0].begin(), calls[0].end(), "8.8.8.8"));
}

TEST_F(NetworkManagerTest, Ping_IPv6_RunsPing6)
{
    auto runner = UseRunner(std::make_shared<CannedRunner>(PingOutput, 0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Ping"),
        _T("{\"endpoint\":\"2404:6800:4007:80b::200e\",\"ipversion\":\"IPv6\"}"), response));

    EXPECT_EQ("2404:6800:4007:80b::200e", Parse(response)["endpoint"].String());
    ASSERT_EQ(1u, runner->Calls().size());
    EXPECT_EQ("ping6", runner->Calls()[0].front());
}

TEST_F(NetworkManagerTest, Ping_NonZeroExit_ReportsFailure)
{
    UseRunner(std::make_shared<CannedRunner>("3 packets transmitted, 0 packets received, 100% packet loss\n", 1));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Ping"), _T("{\"endpoint\":\"192.0.0.1\"}"), response));

    JsonObject result = Parse(response);
    EXPECT_FALSE(result["success"].Boolean());
    EXPECT_TRUE(result.HasLabel("error"));
    EXPECT_EQ(0, result["packetsReceived"].Number());
}

TEST_F(NetworkManagerTest, Ping_UnsafeEndpoint_RejectedWithoutSpawning)
{
    auto runner = UseRunner(std::make_shared<CannedRunner>(PingOutput, 0));

    for (const string endpoint : { "8.8.8.8; reboot", "8.8.8.8 $(id)", "-f" }) {
        JsonObject params;
        params["endpoint"] = endpoint;
        string paramString;
        params.ToString(paramString);
        EXPECT_EQ(Core::ERROR_BAD_REQUEST, handler.Invoke(connection, _T("Ping"), paramString, response)) << endpoint;
    }
    EXPECT_TRUE(runner->Calls().empty());
}

TEST_F(NetworkManagerTest, Ping_MissingEndpoint_ReturnsError)
{
    EXPECT_NE(Core::ERROR_NONE, handler.Invoke(connection, _T("Ping"), _T("{\"ipversion\":\"IPv4\"}"), response));
}

TEST_F(NetworkManagerTest, Ping_SocketCaller_RepliesLaterOnSameChannel)
{
    UseRunner(std::make_shared<CannedRunner>(PingOutput, 0));
    std::promise<string> sent;
    EXPECT_CALL(service, Submit(::testing::Eq(1u), ::testing::_))
        .WillOnce(::testing::Invoke([&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& element) {
            string message;
            element->ToString(message);
            sent.set_value(message);
            return Core::ERROR_NONE;
        }));

    const Core::JSONRPC::Context socketCaller(1, 42, "");
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, handler.Invoke(socketCaller, _T("Ping"), _T("{\"endpoint\":\"8.8.8.8\"}"), response));

    auto reply = sent.get_future();
    ASSERT_EQ(std::future_status::ready, reply.wait_for(std::chrono::seconds(5)));
    JsonObject message = Parse(reply.get());
    EXPECT_EQ(42, message["id"].Number());
    EXPECT_FALSE(message.HasLabel("error"));
    JsonObject result = message["result"].Object();
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_EQ("8.8.8.8", result["endpoint"].String());
}

TEST_F(NetworkManagerTest, Trace_DirectCaller_ReturnsOneElementPerLine)
{
    auto runner = UseRunner(std::make_shared<CannedRunner>(TraceOutput, 0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Trace"),
        _T("{\"endpoint\":\"8.8.8.8\",\"ipversion\":\"IPv4\",\"packets\":1}"), response));

    JsonObject result = Parse(response);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_EQ("8.8.8.8", result["endpoint"].String());
    JsonArray lines;
    lines.FromString(result["results"].String());
    ASSERT_EQ(3, lines.Length());
    EXPECT_THAT(lines[0].String(), ::testing::HasSubstr("6 hops max, 52 byte packets"));

    ASSERT_EQ(1u, runner->Calls().size());
    EXPECT_EQ("traceroute", runner->Calls()[0].front());
}

TEST_F(NetworkManagerTest, Trace_StderrTextIsInResults)
{
    auto runner = UseRunner(std::make_shared<CannedRunner>("traceroute6: bad address '8.8.8.8'", 1));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Trace"),
        _T("{\"endpoint\":\"8.8.8.8\",\"ipversion\":\"IPv6\",\"packets\":1}"), response));

    EXPECT_THAT(Parse(response)["results"].String(), ::testing::HasSubstr("bad address '8.8.8.8'"));
    ASSERT_EQ(1u, runner->Calls().size());
    EXPECT_EQ("traceroute6", runner->Calls()[0].front());
}

TEST_F(NetworkManagerTest, Trace_Timeout_ReportsFailure)
{
    UseRunner(std::make_shared<CannedRunner>("1  *\n", 0, true));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Trace"),
        _T("{\"endpoint\":\"8.8.8.8\",\"ipversion\":\"IPv4\",\"packets\":1}"), response));

    EXPECT_FALSE(Parse(response)["success"].Boolean());
}

TEST_F(NetworkManagerTest, Trace_InvalidIpversion_ReportsFailureWithoutSpawning)
{
    auto runner = UseRunner(std::make_shared<CannedRunner>(TraceOutput, 0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("Trace"), _T("{\"endpoint\":\"8.8.8.8\",\"ipversion\":\"IPv5\"}"), response));

    EXPECT_FALSE(Parse(response)["success"].Boolean());
    EXPECT_TRUE(runner->Calls().empty());
}

TEST_F(NetworkManagerTest, Trace_QueueFull_ReturnsInProgressError)
{
    auto runner = UseRunner(std::make_shared<GatedRunner>());
    std::mutex lock;
    std::condition_variable signal;
    size_t replies = 0;
    ON_CALL(service, Submit(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>&) {
            std::lock_guard<std::mutex> guard(lock);
            ++replies;
            signal.notify_all();
            return Core::ERROR_NONE;
        }));
    const string params = _T("{\"endpoint\":\"8.8.8.8\",\"ipversion\":\"IPv4\",\"packets\":1}");

    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, handler.Invoke(Core::JSONRPC::Context(1, 100, ""), _T("Trace"), params, response));
    ASSERT_TRUE(runner->WaitStarted(1));
    for (uint32_t id = 101; id <= 104; ++id)
        EXPECT_EQ(Plugin::DeferredReply::ReplyLater, handler.Invoke(Core::JSONRPC::Context(1, id, ""), _T("Trace"), params, response));

    EXPECT_EQ(Core::ERROR_INPROGRESS, handler.Invoke(Core::JSONRPC::Context(1, 105, ""), _T("Trace"), params, response));

    runner->Open();
    std::unique_lock<std::mutex> guard(lock);
    EXPECT_TRUE(signal.wait_for(guard, std::chrono::seconds(5), [&] { return replies >= 5; }));
}

// DeferredReply::Table, driven directly with this plugin as the JSON-RPC owner.
class DeferredReplyTest : public NetworkManagerTest {
protected:
    std::promise<string> sent;

    void ExpectOneReplyOnChannel1()
    {
        EXPECT_CALL(service, Submit(::testing::Eq(1u), ::testing::_))
            .WillOnce(::testing::Invoke([&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& element) {
                string message;
                element->ToString(message);
                sent.set_value(message);
                return Core::ERROR_NONE;
            }));
    }

    static uint32_t PassThrough(const uint32_t rc, const string& result, string& reply)
    {
        reply = result;
        return rc;
    }
};

TEST_F(DeferredReplyTest, CloseAnswersPendingRequestWithErrorExactlyOnce)
{
    ExpectOneReplyOnChannel1();
    auto table = std::make_shared<Plugin::DeferredReply::Table>(*plugin);
    Plugin::DeferredReply::Callback* held = nullptr;

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, table->Invoke(Core::JSONRPC::Context(1, 5, ""),
        [&](Plugin::DeferredReply::Callback* callback) { held = callback; held->AddRef(); return Core::ERROR_NONE; },
        PassThrough, reply));

    table->Close(Core::ERROR_UNAVAILABLE);
    held->Completed(Core::ERROR_NONE, "{\"late\":true}");
    held->Release();

    auto message = sent.get_future();
    ASSERT_EQ(std::future_status::ready, message.wait_for(std::chrono::seconds(1)));
    JsonObject error = Parse(message.get())["error"].Object();
    EXPECT_EQ(static_cast<int>(Core::ERROR_UNAVAILABLE), error["code"].Number());
}

TEST_F(DeferredReplyTest, CallbackReleasedWithoutCompletedRepliesWithError)
{
    ExpectOneReplyOnChannel1();
    auto table = std::make_shared<Plugin::DeferredReply::Table>(*plugin);
    Plugin::DeferredReply::Callback* held = nullptr;

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, table->Invoke(Core::JSONRPC::Context(1, 6, ""),
        [&](Plugin::DeferredReply::Callback* callback) { held = callback; held->AddRef(); return Core::ERROR_NONE; },
        PassThrough, reply));
    held->Release();

    auto message = sent.get_future();
    ASSERT_EQ(std::future_status::ready, message.wait_for(std::chrono::seconds(1)));
    EXPECT_TRUE(Parse(message.get()).HasLabel("error"));
}

TEST_F(DeferredReplyTest, StartFailureIsReturnedSynchronously)
{
    EXPECT_CALL(service, Submit(::testing::_, ::testing::_)).Times(0);
    auto table = std::make_shared<Plugin::DeferredReply::Table>(*plugin);

    string reply;
    EXPECT_EQ(Core::ERROR_INPROGRESS, table->Invoke(Core::JSONRPC::Context(1, 8, ""),
        [](Plugin::DeferredReply::Callback*) { return Core::ERROR_INPROGRESS; },
        PassThrough, reply));
}

TEST_F(DeferredReplyTest, StartFailureAfterDrainDoesNotReplyTwice)
{
    ExpectOneReplyOnChannel1();
    auto table = std::make_shared<Plugin::DeferredReply::Table>(*plugin);

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, table->Invoke(Core::JSONRPC::Context(1, 9, ""),
        [&](Plugin::DeferredReply::Callback*) { table->Drain(Core::ERROR_UNAVAILABLE); return Core::ERROR_GENERAL; },
        PassThrough, reply));

    auto message = sent.get_future();
    ASSERT_EQ(std::future_status::ready, message.wait_for(std::chrono::seconds(1)));
    JsonObject error = Parse(message.get())["error"].Object();
    EXPECT_EQ(static_cast<int>(Core::ERROR_UNAVAILABLE), error["code"].Number());
}

TEST_F(DeferredReplyTest, DirectCallerWaitsForCompletion)
{
    EXPECT_CALL(service, Submit(::testing::_, ::testing::_)).Times(0);
    auto table = std::make_shared<Plugin::DeferredReply::Table>(*plugin);
    std::thread worker;

    string reply;
    EXPECT_EQ(Core::ERROR_NONE, table->Invoke(DirectCaller(),
        [&](Plugin::DeferredReply::Callback* callback) {
            callback->AddRef();
            worker = std::thread([callback]() { callback->Completed(Core::ERROR_NONE, "{\"done\":true}"); callback->Release(); });
            return Core::ERROR_NONE;
        },
        PassThrough, reply));
    worker.join();

    EXPECT_TRUE(Parse(reply)["done"].Boolean());
}

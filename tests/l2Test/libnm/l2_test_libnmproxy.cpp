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
#include <sys/stat.h>
#include <fstream>

#include "FactoriesImplementation.h"
#include "WrapsMock.h"
#include "LibnmWrapsMock.h"
#include "GLibWrapsMock.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "COMLinkMock.h"
#include "WorkerPoolImplementation.h"
#include "NetworkManagerImplementation.h"
#include "NetworkManagerLogger.h"
#include "NetworkManager.h"
#include <libnm/NetworkManager.h>

using namespace WPEFramework;
using ::testing::NiceMock;

namespace WPEFramework { namespace Plugin { extern NetworkManagerImplementation* _instance; } }

class NetworkManagerTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::NetworkManager> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    NMClient* gNmClient = NULL;

    WrapsImplMock *p_wrapsImplMock = nullptr;
    GLibWrapsImplMock *p_gLibWrapsImplMock = nullptr;
    LibnmWrapsImplMock *p_libnmWrapsImplMock = nullptr;
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

        p_libnmWrapsImplMock = new NiceMock <LibnmWrapsImplMock>;
        LibnmWraps::setImpl(p_libnmWrapsImplMock);

        p_gLibWrapsImplMock = new NiceMock <GLibWrapsImplMock>;
        GLibWraps::setImpl(p_gLibWrapsImplMock);

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
                "  \"loglevel\":4"
                " }"
                "}"
            ));

        EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
            .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

        EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
            .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_UNMANAGED));

        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                    [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        NetworkManagerImpl = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
                        return &NetworkManagerImpl;
                }));

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();


        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);
        response = plugin->Initialize(&service);
        EXPECT_EQ(string(""), response);
        NetworkManagerLogger::SetLevel(static_cast<NetworkManagerLogger::LogLevel>(NetworkManagerLogger::DEBUG_LEVEL));
    }

    virtual void SetUp() override
    {
        struct stat buffer;
        if (stat("/etc/device.properties", &buffer) != 0) {
            std::ofstream file("/etc/device.properties");
            if (file.is_open()) {
                file << "ETHERNET_INTERFACE=eth0\nWIFI_INTERFACE=wlan0\n";
                file.close();
            }
            else {
                std::cerr << "Failed to create /etc/device.properties file." << std::endl;
            }
        }
    }

    virtual ~NetworkManagerTest() override
    {
        plugin->Deinitialize(&service);
        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }

        LibnmWraps::setImpl(nullptr);
        if (p_libnmWrapsImplMock != nullptr)
        {
            delete p_libnmWrapsImplMock;
            p_libnmWrapsImplMock = nullptr;
        }

        GLibWraps::setImpl(nullptr);
        if (p_gLibWrapsImplMock != nullptr)
        {
            delete p_gLibWrapsImplMock;
            p_gLibWrapsImplMock = nullptr;
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

TEST_F(NetworkManagerTest, SetLogLevel)
{
    EXPECT_CALL(*p_wrapsImplMock, popen(::testing::_, ::testing::_))
    .Times(1)
    .WillOnce(::testing::Invoke(
            [&](const char* command, const char* type) -> FILE* {
            EXPECT_THAT(string(command), ::testing::MatchesRegex("nmcli general logging level TRACE domains PLATFORM,DEVICE,WIFI,ETHER,DNS,DHCP4,DHCP6,DEVICE,SETTINGS,DISPATCH"));
                        // Create a temporary file with the mock output
            FILE* tempFile = tmpfile();
            return tempFile;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetLogLevel"), _T("{\"level\":4}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetHostName_null)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetHostname"), _T("{\"hostname\":\"\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetHostName_ClientCon_Null)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(NULL)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetHostname"), _T("{\"hostname\":\"test_host\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetHostName_iface_null)
{
    GPtrArray* dummyConns = g_ptr_array_new();
    NMConnection *conn = static_cast<NMConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyConns, conn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return(nullptr));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetHostname"), _T("{\"hostname\":\"test_host\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    //g_object_unref(conn);
    g_ptr_array_free(dummyConns, TRUE);
}

TEST_F(NetworkManagerTest, SetHostName_iface_unknown)
{
    GPtrArray* dummyConns = g_ptr_array_new();
    NMConnection *conn = static_cast<NMConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyConns, conn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("unknown"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetHostname"), _T("{\"hostname\":\"test_host\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    g_object_unref(conn);
    g_ptr_array_free(dummyConns, TRUE);
}

TEST_F(NetworkManagerTest, SetHostName_iface_wlan0_alradyset)
{
    GPtrArray* dummyConns = g_ptr_array_new();
    NMConnection *conn = static_cast<NMConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyConns, conn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("wlan0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(0x100173)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip6_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(0x100174)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_ip_config_get_dhcp_hostname(::testing::_))
        .WillOnce(::testing::Return("test_host"))
        .WillOnce(::testing::Return("test_host"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_ip_config_get_dhcp_send_hostname(::testing::_))
        .WillOnce(::testing::Return(TRUE))
        .WillOnce(::testing::Return(TRUE));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetHostname"), _T("{\"hostname\":\"test_host\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    g_object_unref(conn);
    g_ptr_array_free(dummyConns, TRUE);
}

TEST_F(NetworkManagerTest, SetHostName_iface_wlan0)
{
    GPtrArray* dummyConns = g_ptr_array_new();
    NMConnection *conn = static_cast<NMConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyConns, conn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("wlan0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(NULL)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip6_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_id(::testing::_))
        .WillOnce(::testing::Return("test_wlan"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_remote_connection_commit_changes(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](NMRemoteConnection *connection, gboolean save_to_disk, GCancellable *cancellable, GError **error) -> gboolean {
                return TRUE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetHostname"), _T("{\"hostname\":\"test_host\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
    g_object_unref(conn);
    g_ptr_array_free(dummyConns, TRUE);
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_empty)
{
    string interface{};
    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_ethEnabled = false;
    NetworkManagerImpl2->m_wlanEnabled = false;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(interface, "");
}

TEST_F(NetworkManagerTest, GetInterfaceState_Failed)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetInterfaceState_WifiEth)
{
    GPtrArray* fakeDevices = g_ptr_array_new();

    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"))
        .WillOnce(::testing::Return("eth0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNMANAGED)); // disabled

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_EQ(response, _T("{\"enabled\":true,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_EQ(response, _T("{\"enabled\":false,\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, GetInterfaceState_WifiEthNotFound)
{
    GPtrArray* fakeDevices = g_ptr_array_new();

    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan1"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, GetInterfaceState_unknown)
{
    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    Exchange::INetworkManager* interface = static_cast<Exchange::INetworkManager*>(NetworkManagerImpl2->QueryInterface(Exchange::INetworkManager::ID));
    ASSERT_TRUE(interface != nullptr);

    bool isEnabled;
    std::string ifaceName = "wlan1";
    uint32_t result = interface->GetInterfaceState(ifaceName, isEnabled);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(isEnabled, false);
}

TEST_F(NetworkManagerTest, GetAvailableInterfaces_DevicesNull)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillOnce(::testing::Return(nullptr));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T(""), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetAvailableInterfaces_Enabled)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *ethDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    NMDevice* wifiDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, wifiDevice);
    g_ptr_array_add(fakeDevices, ethDevice);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillOnce(::testing::Return(fakeDevices));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(ethDevice))
        .WillOnce(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(wifiDevice))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_hw_address(ethDevice))
        .WillOnce(::testing::Return("00:11:22:33:44:55"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_hw_address(wifiDevice))
        .WillOnce(::testing::Return("66:77:88:99:AA:BB"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(ethDevice))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(wifiDevice))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNAVAILABLE));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T(""), response));
    EXPECT_TRUE(response.find("wlan0") != std::string::npos);
    EXPECT_TRUE(response.find("eth0") != std::string::npos);

    std::string expectedResponse =
        _T("{\"interfaces\":[")
        _T("{\"type\":\"WIFI\",\"name\":\"wlan0\",\"mac\":\"66:77:88:99:AA:BB\",\"enabled\":true,\"connected\":false},")
        _T("{\"type\":\"ETHERNET\",\"name\":\"eth0\",\"mac\":\"00:11:22:33:44:55\",\"enabled\":true,\"connected\":true}")
        _T("],\"success\":true}");
    EXPECT_EQ(response, expectedResponse);

    g_object_unref(ethDevice);
    g_object_unref(wifiDevice);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, GetAvailableInterfaces_disabled)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *ethDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    NMDevice* wifiDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, wifiDevice);
    g_ptr_array_add(fakeDevices, ethDevice);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillOnce(::testing::Return(fakeDevices));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(ethDevice))
        .WillOnce(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(wifiDevice))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_hw_address(ethDevice))
        .WillOnce(::testing::Return("00:11:22:33:44:55"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_hw_address(wifiDevice))
        .WillOnce(::testing::Return("66:77:88:99:AA:BB"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(ethDevice))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNMANAGED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(wifiDevice))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNMANAGED));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T(""), response));
    EXPECT_TRUE(response.find("wlan0") != std::string::npos);
    EXPECT_TRUE(response.find("eth0") != std::string::npos);

    std::string expectedResponse =
        _T("{\"interfaces\":[")
        _T("{\"type\":\"WIFI\",\"name\":\"wlan0\",\"mac\":\"66:77:88:99:AA:BB\",\"enabled\":false,\"connected\":false},")
        _T("{\"type\":\"ETHERNET\",\"name\":\"eth0\",\"mac\":\"00:11:22:33:44:55\",\"enabled\":false,\"connected\":false}")
        _T("],\"success\":true}");
    EXPECT_EQ(response, expectedResponse);

    g_object_unref(ethDevice);
    g_object_unref(wifiDevice);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, GetIPSettings_unknown_iface)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth1\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_emptyCache)
{
    /* With no cache populated, GetIPSettings should still succeed but return no IP data */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"interface\":\"eth0\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipversion\":\"IPv4\"") != std::string::npos);
    /* No ipaddress key when cache is empty */
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_interface_Empty)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_GetPrimary_failed)
{
    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    string interface; const string ipversion; Exchange::INetworkManager::IPAddress address;
    EXPECT_EQ(Core::ERROR_GENERAL, NetworkManagerImpl2->GetIPSettings(interface, ipversion, address));
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv4_fromCache)
{
    /* Populate IPv4 cache for eth0, then verify GetIPSettings returns cached data */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.globalAddresses["192.168.1.2"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    cache.gateway = "192.168.1.1";
    cache.primarydns = "8.8.8.8";
    cache.secondarydns = "8.8.4.4";
    cache.dhcpserver = "192.168.1.11";
    cache.autoconfig = true;
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"interface\":\"eth0\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipversion\":\"IPv4\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"autoconfig\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"192.168.1.2\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"prefix\":24") != std::string::npos);
    EXPECT_TRUE(response.find("\"gateway\":\"192.168.1.1\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"primarydns\":\"8.8.8.8\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"secondarydns\":\"8.8.4.4\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"dhcpserver\":\"192.168.1.11\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv4_autoconfig)
{
    /* Populate IPv4 cache with autoconfig=true but no IP address */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    std::string expectedResponse =
        _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv4_staticConfig)
{
    /* Populate IPv4 cache with autoconfig=false (static config) */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = false;
    cache.globalAddresses["192.168.1.100"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    cache.gateway = "192.168.1.1";
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"autoconfig\":false") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"192.168.1.100\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_wlan0_fromCache)
{
    /* Populate IPv4 cache for wlan0 */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    cache.globalAddresses["10.0.0.5"] = Plugin::GlobalAddressInfo(8, Plugin::ADDR_GLOBAL);
    cache.gateway = "10.0.0.1";
    cache.primarydns = "1.1.1.1";
    Plugin::_instance->swapIpCache("wlan0", "IPv4", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"interface\":\"wlan0\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"10.0.0.5\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv4_config_valid)
{
    /* Populate full IPv4 cache for eth0 and verify all fields */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    cache.globalAddresses["192.168.1.2"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    cache.gateway = "192.168.1.0";
    cache.primarydns = "8.8.8.8";
    cache.secondarydns = "8.8.4.4";
    cache.dhcpserver = "192.168.1.11";
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));

    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"secondarydns\":\"8.8.4.4\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"primarydns\":\"8.8.8.8\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"interface\":\"eth0\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"192.168.1.2\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ula\":\"\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"dhcpserver\":\"192.168.1.11\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"gateway\":\"192.168.1.0\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv6_config_valid)
{
    /* Populate full IPv6 cache for eth0 and verify all fields */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    cache.globalAddresses["2001:db8:1:2:3:4:5:6"] = Plugin::GlobalAddressInfo(64, Plugin::ADDR_GLOBAL);
    cache.uniqueLocalAddresses.insert("fd12::1234:5678:abcd:ef01");
    cache.gateway = "2001:4860:4860::1";
    cache.primarydns = "2001:4860:4860::8888";
    cache.secondarydns = "2001:4860:4860::8844";
    cache.dhcpserver = "2001:db8::1";
    Plugin::_instance->swapIpCache("eth0", "IPv6", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"IPv6\"}"), response));

    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipversion\":\"IPv6\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"secondarydns\":\"2001:4860:4860::8844\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"primarydns\":\"2001:4860:4860::8888\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"interface\":\"eth0\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"2001:db8:1:2:3:4:5:6\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ula\":\"fd12::1234:5678:abcd:ef01\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"prefix\":64") != std::string::npos);
    EXPECT_TRUE(response.find("\"dhcpserver\":\"2001:db8::1\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"gateway\":\"2001:4860:4860::1\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv6_mac_based_fallback)
{
    /* When all global addresses are MAC-based, toIPAddress should use the MAC-based one */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    cache.globalAddresses["2001:db8::aabb:ccff:fedd:eeff"] = Plugin::GlobalAddressInfo(64, Plugin::ADDR_GLOBAL_MAC_BASED);
    cache.gateway = "fe80::1";
    Plugin::_instance->swapIpCache("eth0", "IPv6", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"IPv6\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"2001:db8::aabb:ccff:fedd:eeff\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"prefix\":64") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv6_prefer_non_mac_global)
{
    /* ADDR_GLOBAL should be preferred over ADDR_GLOBAL_MAC_BASED */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    /* Insert MAC-based first to ensure it's not selected by insertion order */
    cache.globalAddresses["2001:db8::aabb:ccff:fedd:eeff"] = Plugin::GlobalAddressInfo(64, Plugin::ADDR_GLOBAL_MAC_BASED);
    cache.globalAddresses["2001:db8::1234:5678"] = Plugin::GlobalAddressInfo(64, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv6", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"IPv6\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"2001:db8::1234:5678\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv6_only_cached_request_ipv4)
{
    /* Only IPv6 cached, but IPv4 requested — should return success with no IP */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    cache.globalAddresses["2001:db8::1"] = Plugin::GlobalAddressInfo(64, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv6", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"IPv4\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipversion\":\"IPv4\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_cache_invalidated)
{
    /* Cache exists but valid=false — should return success with no IP data */
    Plugin::IpFamilyCache cache;
    cache.valid = false;
    cache.globalAddresses["192.168.1.5"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipversion_case_insensitive)
{
    /* ipversion "ipv6" (lowercase) should be treated as IPv6 */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    cache.globalAddresses["2001:db8::99"] = Plugin::GlobalAddressInfo(128, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv6", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"ipv6\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"2001:db8::99\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv6_ula_only)
{
    /* Cache has only ULA addresses, no global — ipaddress is empty so the
       JSON-RPC layer (NetworkManagerJsonRpc.cpp) skips all address fields
       including ula, gateway, etc.  Only interface/ipversion/autoconfig/success
       are returned. */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.autoconfig = true;
    cache.uniqueLocalAddresses.insert("fd00::1234:abcd");
    cache.gateway = "fe80::1";
    Plugin::_instance->swapIpCache("eth0", "IPv6", cache);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"IPv6\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    /* No global address → ipaddress empty → JSON-RPC omits address detail fields */
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);
    EXPECT_TRUE(response.find("\"ula\"") == std::string::npos);
    EXPECT_TRUE(response.find("\"gateway\"") == std::string::npos);
    EXPECT_TRUE(response.find("\"autoconfig\":true") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_swapIpCache_returns_old_keys)
{
    /* Verify swapIpCache returns the old global address keys */
    Plugin::IpFamilyCache cache1;
    cache1.valid = true;
    cache1.globalAddresses["192.168.1.10"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    cache1.globalAddresses["192.168.1.20"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache1);

    /* Now swap with a new cache and check old keys are returned */
    Plugin::IpFamilyCache cache2;
    cache2.valid = true;
    cache2.globalAddresses["10.0.0.1"] = Plugin::GlobalAddressInfo(8, Plugin::ADDR_GLOBAL);
    std::set<std::string> oldKeys = Plugin::_instance->swapIpCache("eth0", "IPv4", cache2);

    EXPECT_EQ(oldKeys.size(), 2u);
    EXPECT_TRUE(oldKeys.count("192.168.1.10") == 1);
    EXPECT_TRUE(oldKeys.count("192.168.1.20") == 1);

    /* Verify the new cache is now active */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\":\"10.0.0.1\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_separate_ipv4_ipv6_caches)
{
    /* IPv4 and IPv6 caches for the same interface should be independent */
    Plugin::IpFamilyCache cache4;
    cache4.valid = true;
    cache4.autoconfig = true;
    cache4.globalAddresses["192.168.1.50"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache4);

    Plugin::IpFamilyCache cache6;
    cache6.valid = true;
    cache6.autoconfig = true;
    cache6.globalAddresses["2001:db8::50"] = Plugin::GlobalAddressInfo(64, Plugin::ADDR_GLOBAL);
    cache6.uniqueLocalAddresses.insert("fd12::50");
    Plugin::_instance->swapIpCache("eth0", "IPv6", cache6);

    /* Query IPv4 */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"IPv4\"}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\":\"192.168.1.50\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipversion\":\"IPv4\"") != std::string::npos);

    /* Query IPv6 */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"IPv6\"}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\":\"2001:db8::50\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipversion\":\"IPv6\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ula\":\"fd12::50\"") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_cache_cleared)
{
    /* Populate cache, then swap with empty/invalid cache — simulates disconnect */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.globalAddresses["192.168.1.99"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache);

    /* Verify it's there */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\":\"192.168.1.99\"") != std::string::npos);

    /* Clear cache by swapping with default (valid=false) */
    Plugin::IpFamilyCache empty;
    Plugin::_instance->swapIpCache("eth0", "IPv4", empty);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);
}

TEST_F(NetworkManagerTest, SetInterfaceState_deviceFailed_wlan0)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), _T("{\"interface\":\"eth0\",\"enabled\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, SetInterfaceState_wlan0_already_diconnected)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_UNMANAGED));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), _T("{\"interface\":\"wlan0\",\"enabled\":false}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, SetInterfaceState_wlan0_activated)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillRepeatedly(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_disconnect_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_disconnect_async(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(device);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_object_get_path(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<char*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, GAsyncResult *result, GError **error) -> gboolean {
            *error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED, "WiFi scanning failed: %s", "Network interface is busy or unavailable");
            return FALSE;
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, const char *object_path, const char *interface_name, const char *property_name, GVariant *value, int timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(client);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), _T("{\"interface\":\"wlan0\",\"enabled\":false}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, SetInterfaceState_wlan0_deactivated_ErroCode)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillRepeatedly(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_disconnect_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_disconnect_async(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(device);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_object_get_path(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<char*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, GAsyncResult *result, GError **error) -> gboolean {
            *error = g_error_new(G_IO_ERROR, G_IO_ERROR_CANCELLED, "WiFi scanning failed: %s", "Network interface is busy or unavailable");
            return FALSE;
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, const char *object_path, const char *interface_name, const char *property_name, GVariant *value, int timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(client);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), _T("{\"interface\":\"wlan0\",\"enabled\":false}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, SetInterfaceState_wlan0_deactivated_success)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillRepeatedly(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_disconnect_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_disconnect_async(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(device);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_object_get_path(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<char*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, GAsyncResult *result, GError **error) -> gboolean {
            return TRUE;
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, const char *object_path, const char *interface_name, const char *property_name, GVariant *value, int timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(client);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), _T("{\"interface\":\"wlan0\",\"enabled\":false}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, SetInterfaceState_wlan0_enable_success)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNMANAGED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillRepeatedly(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_object_get_path(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<char*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, GAsyncResult *result, GError **error) -> gboolean {
            return TRUE;
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, const char *object_path, const char *interface_name, const char *property_name, GVariant *value, int timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(client);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_,::testing::_))
        .WillRepeatedly(::testing::Return(static_cast<NMDevice*>(NULL)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), _T("{\"interface\":\"wlan0\",\"enabled\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerTest, SetInterfaceState_eth0_enable_success)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNMANAGED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillRepeatedly(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_object_get_path(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<char*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, GAsyncResult *result, GError **error) -> gboolean {
            return TRUE;
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_dbus_set_property(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMClient *client, const char *object_path, const char *interface_name, const char *property_name, GVariant *value, int timeout_msec, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(client);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_,::testing::_))
        .WillRepeatedly(::testing::Return(static_cast<NMDevice*>(NULL)));
    
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetInterfaceState"), _T("{\"interface\":\"eth0\",\"enabled\":true}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}


TEST_F(NetworkManagerTest, SetIPSettings_interface_empty)
{
    // Test setting static IP configuration
    std::string request = _T("{\"interface\":\"\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_device_NULL)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(NULL)));
    // Test setting static IP configuration
    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_Connection_NULL)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x778392)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_available_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(NULL)));

    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_settingsNull)
{
    GPtrArray* dummyConns = g_ptr_array_new();
    NMConnection *conn = static_cast<NMConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyConns, conn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x778392)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_available_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));

    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_Connection)
{
    GPtrArray* dummyConns = g_ptr_array_new();
    NMConnection *conn = static_cast<NMConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyConns, conn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x778392)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(0x778394)));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("wlan0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_available_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));

    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_Connection_eth0)
{
    GPtrArray* dummyConns = g_ptr_array_new();
    NMConnection *conn = static_cast<NMConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyConns, conn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x778392)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(0x778394)));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("eth0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_available_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_ip_config_get_method(::testing::_))
        .WillOnce(::testing::Return("auto"));

    std::string request = _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerTest, SetIPSettings_static_wlan0_activeNull)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_active_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMActiveConnection*>(NULL)));

    std::string request = _T("{\"interface\":\"wlan0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(dummyActiveConn);
}

TEST_F(NetworkManagerTest, SetIPSettings_static_wlan0_active_remoteNull)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_active_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMActiveConnection*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(NULL)));

    std::string request = _T("{\"interface\":\"wlan0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(dummyActiveConn);
}

TEST_F(NetworkManagerTest, SetIPSettings_static_wlan0_autoConfTrue)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection *dummyRemoteConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_active_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMActiveConnection*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(dummyRemoteConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_ip_config_get_method(::testing::_))
        .WillOnce(::testing::Return("auto"));

    std::string request = _T("{\"interface\":\"wlan0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    g_object_unref(dummyActiveConn);
    g_object_unref(dummyRemoteConn);
}

TEST_F(NetworkManagerTest, SetIPSettings_static_wlan0_autoConffalse)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection *dummyRemoteConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_active_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMActiveConnection*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(dummyRemoteConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_remote_connection_commit_changes(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](NMRemoteConnection *connection, gboolean save_to_disk, GCancellable *cancellable, GError **error) -> gboolean {
                *error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED, "connection delete failed");
                return false;
            }));

    std::string request = _T("{\"interface\":\"wlan0\",\"ipversion\":\"IPv4\",\"autoconfig\":false,\"ipaddress\":\"192.168.1.100\",\"prefix\":24,\"gateway\":\"192.168.1.1\",\"primarydns\":\"8.8.8.8\",\"secondarydns\":\"8.8.4.4\"}");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("SetIPSettings"), request, response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(dummyActiveConn);
    g_object_unref(dummyRemoteConn);
}

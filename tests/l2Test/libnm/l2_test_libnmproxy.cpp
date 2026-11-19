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
#include <thread>
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
#include "NetworkManagerDiagnosticsReply.h"
#include "NetworkManagerGnomeEvents.h"
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

//        EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
//            .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

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

        /* Interface-state cache is a process-global static; reset the known
           interfaces so each test starts from a deterministic empty state. */
        Plugin::GnomeNetworkManagerEvents::removeInterfaceStateCache("eth0");
        Plugin::GnomeNetworkManagerEvents::removeInterfaceStateCache("wlan0");
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
        .WillRepeatedly(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));

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
        .WillRepeatedly(::testing::Return(reinterpret_cast<GPtrArray*>(dummyConns)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("wlan0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(NULL)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip6_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_id(::testing::_))
        .WillRepeatedly(::testing::Return("test_wlan"));

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
    /* wlan0 not present in the event cache -> state unknown -> failure */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetInterfaceState_WifiEth)
{
    Plugin::GnomeNetworkManagerEvents::updateInterfaceStateCache("wlan0", NM_DEVICE_STATE_ACTIVATED, "66:77:88:99:AA:BB");
    Plugin::GnomeNetworkManagerEvents::updateInterfaceStateCache("eth0", NM_DEVICE_STATE_UNMANAGED, "00:11:22:33:44:55"); // disabled

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_EQ(response, _T("{\"enabled\":true,\"success\":true}"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_EQ(response, _T("{\"enabled\":false,\"success\":true}"));
}

TEST_F(NetworkManagerTest, GetInterfaceState_WifiEthNotFound)
{
    /* Only eth0 known to the cache; querying wlan0 -> not found -> failure */
    Plugin::GnomeNetworkManagerEvents::updateInterfaceStateCache("eth0", NM_DEVICE_STATE_ACTIVATED, "00:11:22:33:44:55");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"wlan0\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
}

TEST_F(NetworkManagerTest, GetInterfaceState_emptyInterface)
{
    /* Empty interface name is rejected before any cache lookup */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetInterfaceState"), _T("{\"interface\":\"\"}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));
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

TEST_F(NetworkManagerTest, GetAvailableInterfaces_emptyCache)
{
    /* No interfaces reported by NM yet -> empty list is a valid (successful) result */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T(""), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("eth0") == std::string::npos);
    EXPECT_TRUE(response.find("wlan0") == std::string::npos);
}

TEST_F(NetworkManagerTest, GetAvailableInterfaces_Enabled)
{
    Plugin::GnomeNetworkManagerEvents::updateInterfaceStateCache("eth0", NM_DEVICE_STATE_ACTIVATED, "00:11:22:33:44:55");
    Plugin::GnomeNetworkManagerEvents::updateInterfaceStateCache("wlan0", NM_DEVICE_STATE_UNAVAILABLE, "66:77:88:99:AA:BB");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T(""), response));
    EXPECT_TRUE(response.find("wlan0") != std::string::npos);
    EXPECT_TRUE(response.find("eth0") != std::string::npos);

    std::string expectedResponse =
        _T("{\"interfaces\":[")
        _T("{\"type\":\"ETHERNET\",\"name\":\"eth0\",\"mac\":\"00:11:22:33:44:55\",\"enabled\":true,\"connected\":true},")
        _T("{\"type\":\"WIFI\",\"name\":\"wlan0\",\"mac\":\"66:77:88:99:AA:BB\",\"enabled\":true,\"connected\":false}")
        _T("],\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, GetAvailableInterfaces_disabled)
{
    Plugin::GnomeNetworkManagerEvents::updateInterfaceStateCache("eth0", NM_DEVICE_STATE_UNMANAGED, "00:11:22:33:44:55");
    Plugin::GnomeNetworkManagerEvents::updateInterfaceStateCache("wlan0", NM_DEVICE_STATE_UNMANAGED, "66:77:88:99:AA:BB");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T(""), response));
    EXPECT_TRUE(response.find("wlan0") != std::string::npos);
    EXPECT_TRUE(response.find("eth0") != std::string::npos);

    std::string expectedResponse =
        _T("{\"interfaces\":[")
        _T("{\"type\":\"ETHERNET\",\"name\":\"eth0\",\"mac\":\"00:11:22:33:44:55\",\"enabled\":false,\"connected\":false},")
        _T("{\"type\":\"WIFI\",\"name\":\"wlan0\",\"mac\":\"66:77:88:99:AA:BB\",\"enabled\":false,\"connected\":false}")
        _T("],\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(NetworkManagerTest, GetAvailableInterfaces_onlyEthernet)
{
    /* Only eth0 reported by NM; wlan0 (absent from cache) is omitted from the list */
    Plugin::GnomeNetworkManagerEvents::updateInterfaceStateCache("eth0", NM_DEVICE_STATE_ACTIVATED, "00:11:22:33:44:55");

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T(""), response));
    EXPECT_TRUE(response.find("wlan0") == std::string::npos);

    std::string expectedResponse =
        _T("{\"interfaces\":[")
        _T("{\"type\":\"ETHERNET\",\"name\":\"eth0\",\"mac\":\"00:11:22:33:44:55\",\"enabled\":true,\"connected\":true}")
        _T("],\"success\":true}");
    EXPECT_EQ(response, expectedResponse);
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

/* ────────────────────────────────────────────────────────────────────────────
 * Utility function tests — isIPv4LinkLocal, isIPv6LinkLocal, isIPv6ULA,
 * isIPv6MacBased (which exercises parseMac internally).
 * ──────────────────────────────────────────────────────────────────────────── */

TEST_F(NetworkManagerTest, isIPv4LinkLocal_true_for_169_254)
{
    EXPECT_TRUE(Plugin::isIPv4LinkLocal("169.254.0.1"));
    EXPECT_TRUE(Plugin::isIPv4LinkLocal("169.254.255.255"));
    EXPECT_TRUE(Plugin::isIPv4LinkLocal("169.254.100.50"));
}

TEST_F(NetworkManagerTest, isIPv4LinkLocal_false_for_non_link_local)
{
    EXPECT_FALSE(Plugin::isIPv4LinkLocal("192.168.1.1"));
    EXPECT_FALSE(Plugin::isIPv4LinkLocal("10.0.0.1"));
    EXPECT_FALSE(Plugin::isIPv4LinkLocal("169.253.255.255"));
    EXPECT_FALSE(Plugin::isIPv4LinkLocal("169.255.0.1"));
    EXPECT_FALSE(Plugin::isIPv4LinkLocal("0.0.0.0"));
}

TEST_F(NetworkManagerTest, isIPv4LinkLocal_false_for_invalid_input)
{
    EXPECT_FALSE(Plugin::isIPv4LinkLocal(""));
    EXPECT_FALSE(Plugin::isIPv4LinkLocal("not_an_ip"));
    EXPECT_FALSE(Plugin::isIPv4LinkLocal("fe80::1"));
}

TEST_F(NetworkManagerTest, isIPv6LinkLocal_true_for_fe80)
{
    EXPECT_TRUE(Plugin::isIPv6LinkLocal("fe80::1"));
    EXPECT_TRUE(Plugin::isIPv6LinkLocal("fe80::abcd:1234:5678:9abc"));
    EXPECT_TRUE(Plugin::isIPv6LinkLocal("fe80::ffff:ffff:ffff:ffff"));
}

TEST_F(NetworkManagerTest, isIPv6LinkLocal_false_for_non_link_local)
{
    EXPECT_FALSE(Plugin::isIPv6LinkLocal("2001:db8::1"));
    EXPECT_FALSE(Plugin::isIPv6LinkLocal("fd00::1"));
    EXPECT_FALSE(Plugin::isIPv6LinkLocal("::1"));
    EXPECT_FALSE(Plugin::isIPv6LinkLocal("fc00::1"));
    EXPECT_FALSE(Plugin::isIPv6LinkLocal("fec0::1"));
}

TEST_F(NetworkManagerTest, isIPv6LinkLocal_false_for_invalid_input)
{
    EXPECT_FALSE(Plugin::isIPv6LinkLocal(""));
    EXPECT_FALSE(Plugin::isIPv6LinkLocal("not_an_ip"));
    EXPECT_FALSE(Plugin::isIPv6LinkLocal("169.254.1.1"));
}

TEST_F(NetworkManagerTest, isIPv6ULA_true_for_fc_fd)
{
    EXPECT_TRUE(Plugin::isIPv6ULA("fc00::1"));
    EXPECT_TRUE(Plugin::isIPv6ULA("fd00::1"));
    EXPECT_TRUE(Plugin::isIPv6ULA("fd12:3456:789a::1"));
    EXPECT_TRUE(Plugin::isIPv6ULA("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
}

TEST_F(NetworkManagerTest, isIPv6ULA_false_for_non_ula)
{
    EXPECT_FALSE(Plugin::isIPv6ULA("2001:db8::1"));
    EXPECT_FALSE(Plugin::isIPv6ULA("fe80::1"));
    EXPECT_FALSE(Plugin::isIPv6ULA("::1"));
    EXPECT_FALSE(Plugin::isIPv6ULA("fb00::1"));
    EXPECT_FALSE(Plugin::isIPv6ULA("fe00::1"));
}

TEST_F(NetworkManagerTest, isIPv6ULA_false_for_invalid_input)
{
    EXPECT_FALSE(Plugin::isIPv6ULA(""));
    EXPECT_FALSE(Plugin::isIPv6ULA("garbage"));
    EXPECT_FALSE(Plugin::isIPv6ULA("192.168.1.1"));
}

TEST_F(NetworkManagerTest, isIPv6MacBased_true_for_eui64_colon_mac)
{
    /* MAC AA:BB:CC:DD:EE:FF → EUI-64: A8:BB:CC:FF:FE:DD:EE:FF
       (byte 0: 0xAA ^ 0x02 = 0xA8, insert FF:FE in middle) */
    EXPECT_TRUE(Plugin::isIPv6MacBased("2001:db8::a8bb:ccff:fedd:eeff", "AA:BB:CC:DD:EE:FF"));
}

TEST_F(NetworkManagerTest, isIPv6MacBased_true_for_eui64_plain_mac)
{
    /* Same MAC in plain hex format */
    EXPECT_TRUE(Plugin::isIPv6MacBased("2001:db8::a8bb:ccff:fedd:eeff", "aabbccddeeff"));
}

TEST_F(NetworkManagerTest, isIPv6MacBased_false_for_non_eui64)
{
    /* Address that doesn't match the MAC's EUI-64 */
    EXPECT_FALSE(Plugin::isIPv6MacBased("2001:db8::1234:5678:9abc:def0", "AA:BB:CC:DD:EE:FF"));
}

TEST_F(NetworkManagerTest, isIPv6MacBased_false_for_privacy_address)
{
    /* Privacy extension address — random interface ID, not MAC-based */
    EXPECT_FALSE(Plugin::isIPv6MacBased("2001:db8::4f2a:8c91:e3d7:b560", "00:11:22:33:44:55"));
}

TEST_F(NetworkManagerTest, isIPv6MacBased_false_for_invalid_inputs)
{
    EXPECT_FALSE(Plugin::isIPv6MacBased("", "AA:BB:CC:DD:EE:FF"));
    EXPECT_FALSE(Plugin::isIPv6MacBased("2001:db8::1", ""));
    EXPECT_FALSE(Plugin::isIPv6MacBased("not_ipv6", "AA:BB:CC:DD:EE:FF"));
    EXPECT_FALSE(Plugin::isIPv6MacBased("2001:db8::1", "not_a_mac"));
    EXPECT_FALSE(Plugin::isIPv6MacBased("192.168.1.1", "AA:BB:CC:DD:EE:FF"));
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

// ---- Ping/Trace and deferred replies (backend-independent: NetworkManagerImplementation and the plugin) ----

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

// Exact result bytes seen by JSON-RPC callers; these must never change.
TEST_F(NetworkManagerTest, Ping_Golden_ResultBytes)
{
    UseRunner(std::make_shared<CannedRunner>(PingOutput, 0));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Ping"), _T("{\"endpoint\":\"8.8.8.8\",\"count\":3}"), response));
    EXPECT_EQ(R"({"packetsTransmitted":3,"packetsReceived":3,"packetLoss":" 0","tripMin":" 23.363","tripAvg":"23.395","tripMax":"23.440","tripStdDev":"0.179 ms","success":true,"endpoint":"8.8.8.8"})", response);
}

TEST_F(NetworkManagerTest, Ping_Golden_FailureResultBytes)
{
    UseRunner(std::make_shared<CannedRunner>("ping: bad address 'nowhere.invalid'\n", 1));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Ping"), _T("{\"endpoint\":\"nowhere.invalid\"}"), response));
    EXPECT_EQ(R"({"success":false,"error":"Could not ping endpoint","endpoint":"nowhere.invalid"})", response);
}

TEST_F(NetworkManagerTest, Trace_Golden_ResultBytes)
{
    UseRunner(std::make_shared<CannedRunner>(TraceOutput, 0));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Trace"), _T("{\"endpoint\":\"8.8.8.8\",\"ipversion\":\"IPv4\",\"packets\":1}"), response));
    EXPECT_EQ(R"({"endpoint":"8.8.8.8","results":"[\"traceroute to 8.8.8.8 (8.8.8.8), 6 hops max, 52 byte packets \",\"1  gateway (10.46.5.1)  0.448 ms \",\"2  * \"]","success":true})", response);
}

TEST_F(NetworkManagerTest, Trace_Golden_TimeoutResultBytes)
{
    UseRunner(std::make_shared<CannedRunner>("1  *\n", 0, true));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(DirectCaller(), _T("Trace"), _T("{\"endpoint\":\"8.8.8.8\",\"ipversion\":\"IPv4\",\"packets\":1}"), response));
    EXPECT_EQ(R"({"success":false})", response);
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

// DeferredReply::Registry, driven directly with this plugin as the JSON-RPC owner.
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

    JsonObject SentMessage()
    {
        auto message = sent.get_future();
        const auto status = message.wait_for(std::chrono::seconds(1));
        EXPECT_EQ(std::future_status::ready, status);
        if (status != std::future_status::ready)
            return JsonObject();
        return Parse(message.get());
    }

    static uint32_t ErrorReply(const uint32_t rc, string& reply)
    {
        reply.clear();
        return rc;
    }

    // Shaper that turns every failure into a successful {"success":false} result, like NetworkManager.1.Trace.
    static uint32_t FailedResult(const uint32_t, string& reply)
    {
        reply = "{\"success\":false}";
        return Core::ERROR_NONE;
    }
};

TEST_F(DeferredReplyTest, CloseAnswersPendingRequestWithErrorExactlyOnce)
{
    ExpectOneReplyOnChannel1();
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);
    Plugin::DeferredReply::PendingReply held;

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, registry->Invoke(Core::JSONRPC::Context(1, 5, ""),
        [&](Plugin::DeferredReply::PendingReply&& pending) { held = std::move(pending); return Core::ERROR_NONE; },
        ErrorReply, reply));

    EXPECT_EQ(1u, registry->Close(Core::ERROR_UNAVAILABLE));
    held.Respond("{\"late\":true}");
    EXPECT_FALSE(held.IsPending());

    JsonObject error = SentMessage()["error"].Object();
    EXPECT_EQ(static_cast<int>(Core::ERROR_UNAVAILABLE), error["code"].Number());
}

TEST_F(DeferredReplyTest, ReplyAbandonedAfterStartRepliesWithError)
{
    ExpectOneReplyOnChannel1();
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);
    Plugin::DeferredReply::PendingReply held;

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, registry->Invoke(Core::JSONRPC::Context(1, 6, ""),
        [&](Plugin::DeferredReply::PendingReply&& pending) { held = std::move(pending); return Core::ERROR_NONE; },
        ErrorReply, reply));
    held = Plugin::DeferredReply::PendingReply();

    JsonObject error = SentMessage()["error"].Object();
    EXPECT_EQ(static_cast<int>(Core::ERROR_UNAVAILABLE), error["code"].Number());
}

TEST_F(DeferredReplyTest, ReplyAbandonedInsideSuccessfulStartRepliesWithError)
{
    ExpectOneReplyOnChannel1();
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, registry->Invoke(Core::JSONRPC::Context(1, 7, ""),
        [](Plugin::DeferredReply::PendingReply&&) { return Core::ERROR_NONE; },
        ErrorReply, reply));

    JsonObject error = SentMessage()["error"].Object();
    EXPECT_EQ(static_cast<int>(Core::ERROR_UNAVAILABLE), error["code"].Number());
}

TEST_F(DeferredReplyTest, StartFailureIsReturnedSynchronouslyEvenWhenReplyIsReleasedInsideStart)
{
    EXPECT_CALL(service, Submit(::testing::_, ::testing::_)).Times(0);
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);

    string reply;
    EXPECT_EQ(Core::ERROR_INPROGRESS, registry->Invoke(Core::JSONRPC::Context(1, 8, ""),
        [](Plugin::DeferredReply::PendingReply&& pending) {
            Plugin::DeferredReply::PendingReply released(std::move(pending));
            return Core::ERROR_INPROGRESS;
        },
        ErrorReply, reply));
}

TEST_F(DeferredReplyTest, CallbackAdapterReleasedInsideFailedStartDoesNotReply)
{
    EXPECT_CALL(service, Submit(::testing::_, ::testing::_)).Times(0);
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);

    string reply;
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, registry->Invoke(Core::JSONRPC::Context(1, 9, ""),
        [](Plugin::DeferredReply::PendingReply&& pending) {
            auto* callback = Core::Service<Plugin::Diagnostics::PingReply>::Create<Exchange::INetworkManager::IPingCallback>(std::move(pending), string("8.8.8.8"), nullptr);
            Core::IUnknown* unknown = callback->QueryInterface<Core::IUnknown>();
            EXPECT_NE(nullptr, unknown);
            if (unknown != nullptr)
                unknown->Release();
            callback->Release();
            return Core::ERROR_BAD_REQUEST;
        },
        ErrorReply, reply));
}

TEST_F(DeferredReplyTest, StartFailureAfterDrainDoesNotReplyTwice)
{
    ExpectOneReplyOnChannel1();
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);
    Plugin::DeferredReply::PendingReply held;

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, registry->Invoke(Core::JSONRPC::Context(1, 10, ""),
        [&](Plugin::DeferredReply::PendingReply&& pending) {
            held = std::move(pending);
            EXPECT_EQ(1u, registry->Drain(Core::ERROR_UNAVAILABLE));
            return Core::ERROR_GENERAL;
        },
        ErrorReply, reply));
    held = Plugin::DeferredReply::PendingReply();

    JsonObject error = SentMessage()["error"].Object();
    EXPECT_EQ(static_cast<int>(Core::ERROR_UNAVAILABLE), error["code"].Number());
}

TEST_F(DeferredReplyTest, DirectCallerStartFailureAfterDrainKeepsDrainReply)
{
    EXPECT_CALL(service, Submit(::testing::_, ::testing::_)).Times(0);
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);
    Plugin::DeferredReply::PendingReply held;

    string reply;
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, registry->Invoke(DirectCaller(),
        [&](Plugin::DeferredReply::PendingReply&& pending) {
            held = std::move(pending);
            EXPECT_EQ(1u, registry->Drain(Core::ERROR_UNAVAILABLE));
            return Core::ERROR_GENERAL;
        },
        ErrorReply, reply));
}

TEST_F(DeferredReplyTest, CompletionInsideStartIsDeliveredOnce)
{
    ExpectOneReplyOnChannel1();
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, registry->Invoke(Core::JSONRPC::Context(1, 11, ""),
        [](Plugin::DeferredReply::PendingReply&& pending) {
            Plugin::DeferredReply::PendingReply local(std::move(pending));
            local.Respond("{\"fast\":true}");
            local.Fail(Core::ERROR_GENERAL);
            return Core::ERROR_NONE;
        },
        ErrorReply, reply));

    JsonObject message = SentMessage();
    EXPECT_FALSE(message.HasLabel("error"));
    EXPECT_TRUE(message["result"].Object()["fast"].Boolean());
}

TEST_F(DeferredReplyTest, FailUsesTheRequestsFailureShaper)
{
    ExpectOneReplyOnChannel1();
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);
    Plugin::DeferredReply::PendingReply held;

    string reply;
    EXPECT_EQ(Plugin::DeferredReply::ReplyLater, registry->Invoke(Core::JSONRPC::Context(1, 12, ""),
        [&](Plugin::DeferredReply::PendingReply&& pending) { held = std::move(pending); return Core::ERROR_NONE; },
        FailedResult, reply));
    held.Fail(Core::ERROR_TIMEDOUT);

    JsonObject message = SentMessage();
    EXPECT_FALSE(message.HasLabel("error"));
    EXPECT_FALSE(message["result"].Object()["success"].Boolean());
}

TEST_F(DeferredReplyTest, DirectCallerSurvivesOwnerReleasingRegistryDuringClose)
{
    EXPECT_CALL(service, Submit(::testing::_, ::testing::_)).Times(0);
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);
    std::weak_ptr<Plugin::DeferredReply::Registry> observer = registry;
    std::promise<void> started;
    Plugin::DeferredReply::PendingReply held;
    uint32_t rc = Core::ERROR_NONE;

    // Shutdown order from the review: waiter woken by Close(), then every other owner goes away. A use-after-free here only shows under ASan.
    std::thread caller([&, raw = registry.get()]() {
        string reply;
        rc = raw->Invoke(DirectCaller(),
            [&](Plugin::DeferredReply::PendingReply&& pending) { held = std::move(pending); started.set_value(); return Core::ERROR_NONE; },
            ErrorReply, reply);
    });
    started.get_future().wait();
    registry->Close(Core::ERROR_UNAVAILABLE);
    registry.reset();
    held = Plugin::DeferredReply::PendingReply();
    caller.join();

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_TRUE(observer.expired());
}

TEST_F(DeferredReplyTest, DirectCallerWaitsForRespond)
{
    EXPECT_CALL(service, Submit(::testing::_, ::testing::_)).Times(0);
    auto registry = std::make_shared<Plugin::DeferredReply::Registry>(*plugin);
    std::thread worker;

    string reply;
    EXPECT_EQ(Core::ERROR_NONE, registry->Invoke(DirectCaller(),
        [&](Plugin::DeferredReply::PendingReply&& pending) {
            worker = std::thread([held = std::move(pending)]() mutable { held.Respond("{\"done\":true}"); });
            return Core::ERROR_NONE;
        },
        ErrorReply, reply));
    worker.join();

    EXPECT_TRUE(Parse(reply)["done"].Boolean());
}

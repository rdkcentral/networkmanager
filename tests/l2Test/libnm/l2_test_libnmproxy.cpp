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
#include "IarmBusMock.h"
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
            EXPECT_THAT(string(command), ::testing::MatchesRegex("nmcli general logging level TRACE domains ALL"));
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

TEST_F(NetworkManagerTest, GetPrimaryInterface_eth0)
{
    string interface{};
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(reinterpret_cast<NMDevice*>(0x100178)))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));
    
    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_ethEnabled = true;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);

    EXPECT_EQ(interface, "eth0");
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_wlan0)
{
    string interface{};
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(reinterpret_cast<NMDevice*>(0x100178)))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNAVAILABLE));

    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_wlanEnabled = true;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);

    EXPECT_EQ(interface, "wlan0");
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_ActiveConnection_null_eth0)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    string interface{};

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(reinterpret_cast<NMDevice*>(0x100178)))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_primary_connection(::testing::_))
        .WillOnce(::testing::Return(dummyActiveConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(dummyActiveConn))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(NULL)));

    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_ethEnabled = true;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(interface, "eth0");

    g_object_unref(dummyActiveConn);
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_ActiveConnection_null_wlan0)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    string interface{};

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(reinterpret_cast<NMDevice*>(0x100178)))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNAVAILABLE));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_primary_connection(::testing::_))
        .WillOnce(::testing::Return(dummyActiveConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(dummyActiveConn))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(NULL)));

    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_wlanEnabled = true;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(interface, "wlan0");
    g_object_unref(dummyActiveConn);
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_ActiveConnection_wlan0)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection *dummyRemoteConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    string interface{};

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_primary_connection(::testing::_))
        .WillOnce(::testing::Return(dummyActiveConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(dummyActiveConn))
            .WillOnce(::testing::Return(dummyRemoteConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(dummyRemoteConn))
        .WillOnce(::testing::Return("wlan0"));

    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_wlanEnabled = true;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(interface, "wlan0");

    g_object_unref(dummyActiveConn);
    g_object_unref(dummyRemoteConn);
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_ActiveConnection_eth0)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection *dummyRemoteConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    string interface{};

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_primary_connection(::testing::_))
        .WillOnce(::testing::Return(dummyActiveConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(dummyActiveConn))
            .WillOnce(::testing::Return(dummyRemoteConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(dummyRemoteConn))
        .WillOnce(::testing::Return("eth0"));

    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_ethEnabled = true;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(interface, "eth0");

    g_object_unref(dummyActiveConn);
    g_object_unref(dummyRemoteConn);
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_ActiveConnection_unknown)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection *dummyRemoteConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    string interface{};

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_primary_connection(::testing::_))
        .WillOnce(::testing::Return(dummyActiveConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(dummyActiveConn))
            .WillOnce(::testing::Return(dummyRemoteConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(dummyRemoteConn))
        .WillOnce(::testing::Return("unknown"));

    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_ethEnabled = true;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(interface, "wlan0");

    g_object_unref(dummyActiveConn);
    g_object_unref(dummyRemoteConn);
}

TEST_F(NetworkManagerTest, GetPrimaryInterface_ActiveConnection_iface_null)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection *dummyRemoteConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    string interface{};

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_primary_connection(::testing::_))
        .WillOnce(::testing::Return(dummyActiveConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(dummyActiveConn))
            .WillOnce(::testing::Return(dummyRemoteConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(dummyRemoteConn))
        .WillOnce(::testing::Return(static_cast<const char*>(NULL)));

    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_ethEnabled = true;
    uint32_t result = NetworkManagerImpl2->GetPrimaryInterface(interface);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(interface, "wlan0");

    g_object_unref(dummyActiveConn);
    g_object_unref(dummyRemoteConn);
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

TEST_F(NetworkManagerTest, GetIPSettings_invalidDevice)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(NULL)));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_invalid_state)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNMANAGED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_interface_Empty)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_GetPrimary_failed)
{
    NMActiveConnection *dummyActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection *dummyRemoteConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100179)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_primary_connection(::testing::_))
        .WillOnce(::testing::Return(dummyActiveConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(dummyActiveConn))
            .WillOnce(::testing::Return(dummyRemoteConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_interface_name(dummyRemoteConn))
        .WillOnce(::testing::Return("unknown"));

    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl2 = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
    NetworkManagerImpl2->m_ethEnabled = true;

    string interface; const string ipversion; Exchange::INetworkManager::IPAddress address;
    EXPECT_EQ(Core::ERROR_GENERAL, NetworkManagerImpl2->GetIPSettings(interface, ipversion, address));

    g_object_unref(dummyActiveConn);
    g_object_unref(dummyRemoteConn);
}

TEST_F(NetworkManagerTest, GetIPSettings_Invalid_ActiveConnection)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_active_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);
}

TEST_F(NetworkManagerTest, GetIPSettings_Invalid_Connection)
{
    GPtrArray* dummyActiveConn = g_ptr_array_new();
    NMActiveConnection *nullConnection = static_cast<NMActiveConnection*>(NULL);
    NMActiveConnection *ethActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    g_ptr_array_add(dummyActiveConn, nullConnection);
    g_ptr_array_add(dummyActiveConn, ethActiveConn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_active_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);

    g_object_unref(ethActiveConn);
    g_ptr_array_free(dummyActiveConn, TRUE);
}

TEST_F(NetworkManagerTest, GetIPSettings_valid_ConnectionSettingsEmpty)
{
    GPtrArray* dummyActiveConn = g_ptr_array_new();
    NMActiveConnection *ethActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection* retConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyActiveConn, ethActiveConn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(retConn));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_active_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    EXPECT_TRUE(response.find("\"success\":false") != std::string::npos);

    g_object_unref(ethActiveConn);
    g_ptr_array_free(dummyActiveConn, TRUE);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv4_config)
{
    GPtrArray* dummyActiveConn = g_ptr_array_new();
    NMActiveConnection *ethActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection* retConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyActiveConn, ethActiveConn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMIPConfig*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("eth0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(retConn))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_active_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    std::string expectedResponse =
        _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"success\":true}");
    EXPECT_EQ(response, expectedResponse);

    g_object_unref(ethActiveConn);
    g_ptr_array_free(dummyActiveConn, TRUE);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv4_configAutoConftrue)
{
    GPtrArray* dummyActiveConn = g_ptr_array_new();
    NMActiveConnection *ethActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection* retConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyActiveConn, ethActiveConn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMIPConfig*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("eth0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_ip_config_get_method(::testing::_))
        .WillOnce(::testing::Return("auto"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(retConn))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(retConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_active_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    std::string expectedResponse =
        _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"success\":true}");
    EXPECT_EQ(response, expectedResponse);

    g_object_unref(ethActiveConn);
    g_ptr_array_free(dummyActiveConn, TRUE);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv4_configAutoConfNull)
{
    GPtrArray* dummyActiveConn = g_ptr_array_new();
    NMActiveConnection *ethActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection* retConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    g_ptr_array_add(dummyActiveConn, ethActiveConn);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMIPConfig*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("eth0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_ip_config_get_method(::testing::_))
        .WillOnce(::testing::Return("not auto"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingIPConfig*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(retConn))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(retConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_active_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));
    std::string expectedResponse =
        _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\",\"autoconfig\":true,\"success\":true}");
    EXPECT_EQ(response, expectedResponse);

    g_object_unref(ethActiveConn);
    g_ptr_array_free(dummyActiveConn, TRUE);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv4_config_valid)
{
    NMActiveConnection *ethActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection* retConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    NMIPAddress* ipv4Addr = static_cast<NMIPAddress*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));

    GPtrArray* dummyActiveConn = g_ptr_array_new();
    GPtrArray* ipvAddr = g_ptr_array_new();
    g_ptr_array_add(dummyActiveConn, ethActiveConn);
    g_ptr_array_add(ipvAddr, ipv4Addr);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_dhcp_config_get_one_option(::testing::_, ::testing::_))
        .WillOnce(::testing::Return("192.168.1.11"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_dhcp4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDhcpConfig*>(0x100170)));

    const char* fakeDnsServers[] = {"8.8.8.8", "8.8.4.4", nullptr};
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_config_get_nameservers(::testing::_))
        .WillOnce(::testing::Return(fakeDnsServers));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_config_get_gateway(::testing::_))
        .WillOnce(::testing::Return("192.168.1.0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_address_get_address(::testing::_))
        .WillOnce(::testing::Return("192.168.1.2"))
        .WillOnce(::testing::Return("192.168.1.2"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_config_get_addresses(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(ipvAddr)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_ip4_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMIPConfig*>(0x100171)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("eth0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(retConn))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_active_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\"}"), response));

    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"secondarydns\":\"8.8.4.4\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"primarydns\":\"8.8.8.8\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"interface\":\"eth0\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"192.168.1.2\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ula\":\"\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"dhcpserver\":\"192.168.1.11\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"gateway\":\"192.168.1.0\"") != std::string::npos);

    g_object_unref(ethActiveConn);
    g_object_unref(retConn);
    g_object_unref(ipv4Addr);
    g_ptr_array_free(dummyActiveConn, TRUE);
    g_ptr_array_free(ipvAddr, TRUE);
}

TEST_F(NetworkManagerTest, GetIPSettings_ipv6_config_valid)
{
    NMActiveConnection *ethActiveConn = static_cast<NMActiveConnection*>(g_object_new(NM_TYPE_ACTIVE_CONNECTION, NULL));
    NMRemoteConnection* retConn = static_cast<NMRemoteConnection*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));
    NMIPAddress* ipv6Addr = static_cast<NMIPAddress*>(g_object_new(NM_TYPE_REMOTE_CONNECTION, NULL));

    GPtrArray* dummyActiveConn = g_ptr_array_new();
    GPtrArray* ipvAddr = g_ptr_array_new();
    g_ptr_array_add(dummyActiveConn, ethActiveConn);
    g_ptr_array_add(ipvAddr, ipv6Addr);
    g_ptr_array_add(ipvAddr, reinterpret_cast<NMIPAddress*>(0x100176));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_dhcp_config_get_one_option(::testing::_, ::testing::_))
        .WillOnce(::testing::Return("2001:db8::1"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_dhcp6_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDhcpConfig*>(0x100170)));

    const char* fakeDnsServers[] = {"2001:4860:4860::8888", "2001:4860:4860::8844", nullptr};
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_config_get_nameservers(::testing::_))
        .WillOnce(::testing::Return(fakeDnsServers));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_config_get_gateway(::testing::_))
        .WillOnce(::testing::Return("2001:4860:4860::1"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_address_get_prefix(::testing::_))
        .WillOnce(::testing::Return(64));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_address_get_address(::testing::_))
        .WillOnce(::testing::Return("2001:db8:1:2:3:4:5:6"))
        .WillOnce(::testing::Return("fe80::1234:5678:abcd:ef01"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_ip_config_get_addresses(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(ipvAddr)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_ip6_config(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMIPConfig*>(0x100171)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_setting_connection_get_interface_name(::testing::_))
        .WillOnce(::testing::Return("eth0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_connection_get_setting_connection(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMSettingConnection*>(0x100173)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_active_connection_get_connection(::testing::_))
        .WillOnce(::testing::Return(retConn))
        .WillOnce(::testing::Return(reinterpret_cast<NMRemoteConnection*>(NULL)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_active_connections(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GPtrArray*>(dummyActiveConn)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\", \"ipversion\":\"IPv6\"}"), response));

    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipversion\":\"IPv6\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"secondarydns\":\"2001:4860:4860::8844\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"primarydns\":\"2001:4860:4860::8888\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"interface\":\"eth0\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\":\"2001:db8:1:2:3:4:5:6\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"ula\":\"fe80::1234:5678:abcd:ef01\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"prefix\":64") != std::string::npos);
    EXPECT_TRUE(response.find("\"dhcpserver\":\"2001:db8::1\"") != std::string::npos);
    EXPECT_TRUE(response.find("\"gateway\":\"2001:4860:4860::1\"") != std::string::npos);

    g_object_unref(ethActiveConn);
    g_object_unref(retConn);
    g_object_unref(ipv6Addr);
    g_ptr_array_free(dummyActiveConn, TRUE);
    g_ptr_array_free(ipvAddr, TRUE);
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

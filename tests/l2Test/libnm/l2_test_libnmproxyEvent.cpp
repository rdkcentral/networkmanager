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
#include "NetworkManagerGnomeEvents.h"
#include "NetworkManagerLogger.h"
#include "NetworkManager.h"
#include <libnm/NetworkManager.h>

using namespace WPEFramework;
using ::testing::NiceMock;

class NetworkManagerEventTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::NetworkManager> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;

    /* every event callback */
    GCallback nmClientRunCb = NULL;
    GCallback nmStateChange = NULL;

    WrapsImplMock *p_wrapsImplMock = nullptr;
    GLibWrapsImplMock *p_gLibWrapsImplMock = nullptr;
    LibnmWrapsImplMock *p_libnmWrapsImplMock = nullptr;
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
                "}"
            ));

     EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

     EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
            .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_UNMANAGED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_signal_connect_data(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(
        [&](gpointer instance, const gchar *detailed_signal, GCallback c_handler,
            gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags) -> gulong {

            if(detailed_signal == NULL)
                printf("g_signal_connect_data: NULL\n");
            else
                printf("g_signal_connect_data - siganal: %s\n", detailed_signal);
            return 0;
        }));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_signal_handlers_disconnect_by_data(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_signal_handlers_disconnect_by_func(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(0));

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
        EXPECT_EQ(string(""), (response = plugin->Initialize(&service)));
        sleep(1); // Allow some time for initialization
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

        printf("~NetworkManagerEventTest\n");
    }
};

TEST_F(NetworkManagerEventTest, onInterfaceStateChangeCb)
{
    // Testing all interface state change events for Ethernet interface
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ADDED, "eth0");
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_UP, "eth0");
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_LINK_DOWN, "eth0");
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_ACQUIRING_IP, "eth0");
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_REMOVED, "eth0");
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onInterfaceStateChangeCb(Exchange::INetworkManager::INTERFACE_DISABLED, "eth0");
}

TEST_F(NetworkManagerEventTest, onAddressChangeCb)
{
    // Test acquiring IPv4 address
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAddressChangeCb("eth0", "192.168.1.100", true, false);
    
    // Test acquiring IPv6 address
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAddressChangeCb("eth0", "2001:db8::1", true, true);
    
    // Test acquiring same IPv6 address again (should skip posting)
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAddressChangeCb("eth0", "2001:db8::1", true, true);
    
    // Test acquiring different IPv6 address
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAddressChangeCb("eth0", "2001:db8::2", true, true);
    
    // Test losing IPv4 address
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAddressChangeCb("eth0", "", false, false);
    
    // Test losing IPv6 address
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAddressChangeCb("eth0", "", false, true);
    
    // Test losing IP on interface with empty cache (should skip posting)
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAddressChangeCb("eth1", "", false, false);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAddressChangeCb("eth1", "", false, true);
}

TEST_F(NetworkManagerEventTest, onAvailableSSIDsCb)
{
    NMDeviceWifi *deviceDummy = static_cast<NMDeviceWifi*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));

    GPtrArray* fakeAccessPoints = g_ptr_array_new();
    g_ptr_array_add(fakeAccessPoints, reinterpret_cast<NMAccessPoint*>(0x833332));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_get_access_points(::testing::_))
        .WillOnce(::testing::Return(fakeAccessPoints));

    // Create a fake SSID GBytes object
    const char* ssid_str = "TestWiFiNetwork";
    GBytes* fake_ssid = g_bytes_new_static(ssid_str, strlen(ssid_str));

    // AP property mocks
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApFlags>(NM_802_11_AP_FLAGS_PRIVACY)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_wpa_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApSecurityFlags>(NM_802_11_AP_SEC_KEY_MGMT_PSK)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_rsn_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApSecurityFlags>(NM_802_11_AP_SEC_KEY_MGMT_SAE)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_ssid(::testing::_))
        .WillOnce(::testing::Return(fake_ssid));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_frequency(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(2462))); // Channel 11 (2.4GHz)

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_strength(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(85))); // 85% signal strength

    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAvailableSSIDsCb(nullptr, nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAvailableSSIDsCb(deviceDummy, nullptr, nullptr);

    g_ptr_array_free(fakeAccessPoints, TRUE);
}

TEST_F(NetworkManagerEventTest, onWIFIStateChanged)
{
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onWIFIStateChanged(1);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNKNOWN))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_DISCONNECTED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_PREPARE))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_IP_CONFIG));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("eth0"))
        .WillOnce(::testing::Return("eth0"))
        .WillOnce(::testing::Return("eth0"))
        .WillOnce(::testing::Return("eth0"))
        .WillOnce(::testing::Return("eth0"))
        .WillOnce(::testing::Return("wlan0"))
        .WillOnce(::testing::Return("wlan0"))
        .WillOnce(::testing::Return("wlan0"))
        .WillOnce(::testing::Return("wlan0"))
        .WillOnce(::testing::Return("wlan0"))
        .WillOnce(::testing::Return("wlan0"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_SUPPLICANT_AVAILABLE))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_SSID_NOT_FOUND))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_SUPPLICANT_TIMEOUT))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_SUPPLICANT_FAILED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_SUPPLICANT_CONFIG_FAILED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_SUPPLICANT_DISCONNECT));

    // Test with nullptr and with mock device
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(nullptr, nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(0x100179), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(0x100179), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(0x100179), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(0x100179), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(0x100179), nullptr, nullptr);

    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

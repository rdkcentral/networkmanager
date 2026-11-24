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
#include <chrono>
#include <thread>
#include <atomic>
#include <pthread.h>

#include "FactoriesImplementation.h"
#include "WrapsMock.h"
#include "CurlWrapsMock.h"
#include "LibnmWrapsMock.h"
#include "GLibWrapsMock.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "COMLinkMock.h"
#include "WorkerPoolImplementation.h"
#include "NetworkManagerImplementation.h"
#include "NetworkManagerGnomeEvents.h"
#include "NetworkManagerGnomePropertyFollower.h"
#include "NetworkManagerLogger.h"
#include "NetworkManager.h"
#include <libnm/NetworkManager.h>

using namespace WPEFramework;
using ::testing::NiceMock;

namespace WPEFramework { namespace Plugin { extern NetworkManagerImplementation* _instance; } }

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
    CurlWrapsImplMock *p_curlWrapsImplMock = nullptr;
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

        p_curlWrapsImplMock = new NiceMock <CurlWrapsImplMock>;
        CurlWraps::setImpl(p_curlWrapsImplMock);
        ON_CALL(*p_curlWrapsImplMock, curl_multi_perform(::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(
                ::testing::SetArgPointee<1>(0),
                ::testing::Return(CURLM_OK)));
        ON_CALL(*p_curlWrapsImplMock, curl_multi_info_read(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(nullptr));
        ON_CALL(*p_curlWrapsImplMock, curl_multi_poll(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(CURLM_OK));

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
                printf("g_signal_connect_data - signal: %s\n", detailed_signal);
            return 0;
        }));

        EXPECT_CALL(*p_gLibWrapsImplMock, signal_handlers_disconnect_by_data(::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Return(0));

        EXPECT_CALL(*p_gLibWrapsImplMock, signal_handlers_disconnect_by_func(::testing::_, ::testing::_, ::testing::_))
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
        /* WiFi connection snapshot and follower are process-wide; isolate tests. */
        Plugin::GnomeNetworkManagerEvents::stopWifiConnTracking();
    }

    virtual void TearDown() override
    {
        Plugin::GnomeNetworkManagerEvents::stopWifiConnTracking();
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

        CurlWraps::setImpl(nullptr);
        if (p_curlWrapsImplMock != nullptr)
        {
            delete p_curlWrapsImplMock;
            p_curlWrapsImplMock = nullptr;
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

TEST_F(NetworkManagerEventTest, onAvailableSSIDsCb)
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

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

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

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_bssid(::testing::_))
        .WillOnce(::testing::Return("00:11:22:33:44:55"));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_frequency(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(2462))); // Channel 11 (2.4GHz)

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_strength(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(85))); // 85% signal strength

    g_ptr_array_free(fakeDevices, TRUE);

    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAvailableSSIDsCb(nullptr, nullptr, nullptr);
    NMDeviceWifi *deviceWifiDummy = static_cast<NMDeviceWifi*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onAvailableSSIDsCb(deviceWifiDummy, nullptr, nullptr);

    g_object_unref(deviceDummy);
    g_object_unref(deviceWifiDummy);
    g_ptr_array_free(fakeAccessPoints, TRUE);
}

TEST_F(NetworkManagerEventTest, onWIFIStateChanged)
{
    WPEFramework::Plugin::GnomeNetworkManagerEvents::onWIFIStateChanged(1);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_activated)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_ACTIVATED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_disconnected)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_unmanaged)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_UNMANAGED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_prepare)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_PREPARE));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_config)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_CONFIG));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_ip_config)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_IP_CONFIG));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_ip_check)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_IP_CHECK));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_secondaries)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_SECONDARIES));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_deactivating)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_DEACTIVATING));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_failed)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_FAILED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_need_auth)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_NEED_AUTH));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_unknown)
{
    NMDevice *wifiDummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_UNKNOWN));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(wifiDummyDevice), nullptr, nullptr);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_eth0_unmanaged)
{
    NMDevice *DummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_UNMANAGED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(DummyDevice), nullptr, nullptr);
    g_object_unref(DummyDevice);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_eth0_disconnected)
{
    NMDevice *DummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(DummyDevice), nullptr, nullptr);
    g_object_unref(DummyDevice);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_eth0_prepare)
{
    NMDevice *DummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_PREPARE));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(DummyDevice), nullptr, nullptr);
    g_object_unref(DummyDevice);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_eth0_ipconfig)
{
    NMDevice *DummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_IP_CONFIG));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(DummyDevice), nullptr, nullptr);
    g_object_unref(DummyDevice);
}

TEST_F(NetworkManagerEventTest, deviceStateChangeCb_eth0_activated)
{
    NMDevice *DummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillOnce(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(DummyDevice, nullptr, nullptr);
    g_object_unref(DummyDevice);
}

/* ────────────────────────────────────────────────────────────────────────────
 * Cache-clearing tests — verify that disconnect events clear the IP cache.
 * These test observable behavior (cache state after disconnect) rather than
 * internal NM API call sequences, so they remain stable across refactors.
 * ──────────────────────────────────────────────────────────────────────────── */

TEST_F(NetworkManagerEventTest, disconnect_clears_ipv4_cache_eth0)
{
    /* Pre-populate the IPv4 cache for eth0 */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.globalAddresses["192.168.1.50"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    cache.gateway = "192.168.1.1";
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache);

    /* Verify cache is populated */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\"}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\":\"192.168.1.50\"") != std::string::npos);

    /* Trigger eth0 disconnect — refreshIpFamilyCache runs with skipRead=true,
       swapping an empty cache and clearing the old addresses. */
    NMDevice *DummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(DummyDevice, nullptr, nullptr);
    g_object_unref(DummyDevice);

    /* Cache should now be empty — GetIPSettings returns success but no address fields */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);
}

TEST_F(NetworkManagerEventTest, disconnect_clears_ipv6_cache_wlan0)
{
    /* Pre-populate the IPv6 cache for wlan0 */
    Plugin::IpFamilyCache cache;
    cache.valid = true;
    cache.globalAddresses["2001:db8::1"] = Plugin::GlobalAddressInfo(64, Plugin::ADDR_GLOBAL);
    cache.gateway = "fe80::1";
    Plugin::_instance->swapIpCache("wlan0", "IPv6", cache);

    /* Verify cache is populated */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"wlan0\",\"ipversion\":\"IPv6\"}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\":\"2001:db8::1\"") != std::string::npos);

    /* Trigger wlan0 disconnect */
    NMDevice *DummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(reinterpret_cast<NMDevice*>(DummyDevice), nullptr, nullptr);
    g_object_unref(DummyDevice);

    /* Cache should now be empty */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"wlan0\",\"ipversion\":\"IPv6\"}"), response));
    EXPECT_TRUE(response.find("\"success\":true") != std::string::npos);
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);
}

TEST_F(NetworkManagerEventTest, disconnect_clears_both_ip_family_caches)
{
    /* Pre-populate both IPv4 and IPv6 caches for eth0 */
    Plugin::IpFamilyCache cache4;
    cache4.valid = true;
    cache4.globalAddresses["192.168.1.50"] = Plugin::GlobalAddressInfo(24, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv4", cache4);

    Plugin::IpFamilyCache cache6;
    cache6.valid = true;
    cache6.globalAddresses["2001:db8::99"] = Plugin::GlobalAddressInfo(64, Plugin::ADDR_GLOBAL);
    Plugin::_instance->swapIpCache("eth0", "IPv6", cache6);

    /* Trigger disconnect — refreshIpFamilyCache is called for BOTH families */
    NMDevice *DummyDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_ETHERNET, NULL));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("eth0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    WPEFramework::Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(DummyDevice, nullptr, nullptr);
    g_object_unref(DummyDevice);

    /* Both families should be cleared */
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv4\"}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetIPSettings"), _T("{\"interface\":\"eth0\",\"ipversion\":\"IPv6\"}"), response));
    EXPECT_TRUE(response.find("\"ipaddress\"") == std::string::npos);
}

/* ────────────────────────────────────────────────────────────────────────────
 * WiFi connection cache tests — events update the snapshot that serves
 * GetConnectedSSID / GetWifiState. Real NMDeviceWifi / NMAccessPoint objects;
 * libnm getters are stubbed per object with WillRepeatedly / ON_CALL (no
 * call-order assumptions); assertions are on the JSON-RPC responses.
 * ──────────────────────────────────────────────────────────────────────────── */

static const char* const kCacheTestSsid = "TestWiFiNetwork";

static void stubAccessPoint(LibnmWrapsImplMock* mock, NMAccessPoint* ap, GBytes* ssid, const char* bssid,
                            guint32 frequency, guint8 strength, NM80211ApFlags flags,
                            NM80211ApSecurityFlags wpaFlags, NM80211ApSecurityFlags rsnFlags)
{
    ON_CALL(*mock, nm_access_point_get_ssid(ap)).WillByDefault(::testing::Return(ssid));
    ON_CALL(*mock, nm_access_point_get_bssid(ap)).WillByDefault(::testing::Return(bssid));
    ON_CALL(*mock, nm_access_point_get_frequency(ap)).WillByDefault(::testing::Return(frequency));
    ON_CALL(*mock, nm_access_point_get_strength(ap)).WillByDefault(::testing::Return(strength));
    ON_CALL(*mock, nm_access_point_get_flags(ap)).WillByDefault(::testing::Return(flags));
    ON_CALL(*mock, nm_access_point_get_wpa_flags(ap)).WillByDefault(::testing::Return(wpaFlags));
    ON_CALL(*mock, nm_access_point_get_rsn_flags(ap)).WillByDefault(::testing::Return(rsnFlags));
    ON_CALL(*mock, nm_access_point_get_mode(ap)).WillByDefault(::testing::Return(NM_802_11_MODE_INFRA));
    ON_CALL(*mock, nm_access_point_get_max_bitrate(ap)).WillByDefault(::testing::Return(54000));
}

class WifiConnCacheEventTest : public NetworkManagerEventTest {
protected:
    NMDevice *wifiDevice = nullptr;
    NMAccessPoint *apA = nullptr;
    NMAccessPoint *apB = nullptr;
    NMAccessPoint *activeAP = nullptr;
    NMDeviceState deviceState = NM_DEVICE_STATE_ACTIVATED;
    GBytes *ssid = nullptr;

    void SetUp() override
    {
        NetworkManagerEventTest::SetUp();
        wifiDevice = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
        apA = static_cast<NMAccessPoint*>(g_object_new(NM_TYPE_ACCESS_POINT, NULL));
        apB = static_cast<NMAccessPoint*>(g_object_new(NM_TYPE_ACCESS_POINT, NULL));
        ssid = g_bytes_new_static(kCacheTestSsid, strlen(kCacheTestSsid));

        EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
            .WillRepeatedly(::testing::Invoke([this](NMDevice*) { return deviceState; }));
        EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_get_active_access_point(::testing::_))
            .WillRepeatedly(::testing::Invoke([this](NMDeviceWifi*) { return activeAP; }));
        ON_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_)).WillByDefault(::testing::Return("wlan0"));
        ON_CALL(*p_libnmWrapsImplMock, nm_device_get_state_reason(::testing::_))
            .WillByDefault(::testing::Return(NM_DEVICE_STATE_REASON_NONE));
    }

    void TearDown() override
    {
        NetworkManagerEventTest::TearDown();
        g_clear_object(&apA);
        g_clear_object(&apB);
        g_clear_object(&wifiDevice);
        g_bytes_unref(ssid);
    }

    /* Real GLib signal connections (the fixture stubs g_signal_connect_data to 0). */
    void useRealSignals()
    {
        EXPECT_CALL(*p_gLibWrapsImplMock, g_signal_connect_data(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Invoke(__real_g_signal_connect_data));
    }

    string connectedSsid()
    {
        EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T(""), response));
        return response;
    }
};

TEST_F(WifiConnCacheEventTest, refreshWifiConnCache_psk)
{
    stubAccessPoint(p_libnmWrapsImplMock, apA, ssid, "AA:BB:CC:DD:EE:FF", 2462, 85,
                    NM_802_11_AP_FLAGS_PRIVACY, NM_802_11_AP_SEC_KEY_MGMT_PSK, NM_802_11_AP_SEC_KEY_MGMT_SAE);
    activeAP = apA;
    Plugin::GnomeNetworkManagerEvents::refreshWifiConnCache(wifiDevice);
    EXPECT_EQ(connectedSsid(), _T("{\"ssid\":\"TestWiFiNetwork\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"security\":2,\"strength\":-39,\"frequency\":2.462,\"rate\":54000,\"noise\":0,\"success\":true}"));
}

TEST_F(WifiConnCacheEventTest, refreshWifiConnCache_eap)
{
    stubAccessPoint(p_libnmWrapsImplMock, apA, ssid, "AA:BB:CC:DD:EE:FF", 2462, 85,
                    NM_802_11_AP_FLAGS_PRIVACY, NM_802_11_AP_SEC_KEY_MGMT_802_1X, NM_802_11_AP_SEC_KEY_MGMT_802_1X);
    activeAP = apA;
    Plugin::GnomeNetworkManagerEvents::refreshWifiConnCache(wifiDevice);
    EXPECT_EQ(connectedSsid(), _T("{\"ssid\":\"TestWiFiNetwork\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"security\":3,\"strength\":-39,\"frequency\":2.462,\"rate\":54000,\"noise\":0,\"success\":true}"));
}

TEST_F(WifiConnCacheEventTest, refreshWifiConnCache_hiddenSsid)
{
    stubAccessPoint(p_libnmWrapsImplMock, apA, nullptr, "AA:BB:CC:DD:EE:FF", 2462, 20,
                    NM_802_11_AP_FLAGS_PRIVACY, NM_802_11_AP_SEC_NONE, NM_802_11_AP_SEC_NONE);
    activeAP = apA;
    Plugin::GnomeNetworkManagerEvents::refreshWifiConnCache(wifiDevice);
    EXPECT_EQ(connectedSsid(), _T("{\"ssid\":\"\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"security\":1,\"strength\":-78,\"frequency\":2.462,\"rate\":54000,\"noise\":0,\"success\":true}"));
}

TEST_F(WifiConnCacheEventTest, deviceStateChangeCb_wlan_updates_wifi_cache)
{
    stubAccessPoint(p_libnmWrapsImplMock, apA, ssid, "AA:BB:CC:DD:EE:FF", 2462, 85,
                    NM_802_11_AP_FLAGS_PRIVACY, NM_802_11_AP_SEC_KEY_MGMT_PSK, NM_802_11_AP_SEC_KEY_MGMT_SAE);
    activeAP = apA;

    deviceState = NM_DEVICE_STATE_ACTIVATED;
    Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(wifiDevice, nullptr, nullptr);
    EXPECT_EQ(connectedSsid(), _T("{\"ssid\":\"TestWiFiNetwork\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"security\":2,\"strength\":-39,\"frequency\":2.462,\"rate\":54000,\"noise\":0,\"success\":true}"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":5,\"status\":\"WIFI_STATE_CONNECTED\",\"success\":true}"));

    deviceState = NM_DEVICE_STATE_DISCONNECTED;
    Plugin::GnomeNetworkManagerEvents::deviceStateChangeCb(wifiDevice, nullptr, nullptr);
    EXPECT_EQ(connectedSsid(), _T("{\"ssid\":\"\",\"bssid\":\"\",\"security\":0,\"strength\":0,\"frequency\":0,\"rate\":0,\"noise\":0,\"success\":true}"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":2,\"status\":\"WIFI_STATE_DISCONNECTED\",\"success\":true}"));
}

TEST_F(WifiConnCacheEventTest, stopWifiConnTracking_reports_absent)
{
    stubAccessPoint(p_libnmWrapsImplMock, apA, ssid, "AA:BB:CC:DD:EE:FF", 2462, 85,
                    NM_802_11_AP_FLAGS_PRIVACY, NM_802_11_AP_SEC_KEY_MGMT_PSK, NM_802_11_AP_SEC_KEY_MGMT_SAE);
    activeAP = apA;
    Plugin::GnomeNetworkManagerEvents::startWifiConnTracking(wifiDevice);
    EXPECT_TRUE(connectedSsid().find("\"success\":true") != std::string::npos);

    Plugin::GnomeNetworkManagerEvents::stopWifiConnTracking();
    EXPECT_EQ(connectedSsid(), _T("{\"success\":false}"));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":1,\"status\":\"WIFI_STATE_DISABLED\",\"success\":true}"));
}

/* Set from an idle callback dispatched by the event monitor's running main loop
   (default context, thread "nm_event_thrd"). Static so a late dispatch after a
   timed-out wait still writes to valid storage. */
static std::atomic<bool> s_monitorLoopRunning{false};

static gboolean markMonitorLoopRunning(gpointer)
{
    char name[16] = {0};
    pthread_getname_np(pthread_self(), name, sizeof(name));
    if (std::string(name) == "nm_event_thrd")
        s_monitorLoopRunning.store(true);
    return G_SOURCE_REMOVE;
}

TEST_F(WifiConnCacheEventTest, startupWalk_seedsDisconnectedWifiDevice)
{
    /* The fixture's event monitor walked an empty device list; restart it with an
       existing wlan0 already DISCONNECTED (no active AP). The startup walk must seed
       the snapshot on its own: no state or access-point notification follows. */
    Plugin::GnomeNetworkManagerEvents* monitor = Plugin::GnomeNetworkManagerEvents::getInstance();
    monitor->stopNetworkMangerEventMonitor();
    Plugin::GnomeNetworkManagerEvents::stopWifiConnTracking();

    GPtrArray* devices = g_ptr_array_new();
    g_ptr_array_add(devices, wifiDevice);
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(devices));
    deviceState = NM_DEVICE_STATE_DISCONNECTED;
    activeAP = nullptr;

    /* The walk seeds the snapshot before g_main_loop_run(); a quit issued before the
       loop runs is lost and the join would hang. Stop only once the loop has
       dispatched our idle callback (which also implies the walk has finished). */
    s_monitorLoopRunning.store(false);
    g_idle_add(markMonitorLoopRunning, nullptr);
    ASSERT_TRUE(monitor->startNetworkMangerEventMonitor());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!s_monitorLoopRunning.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!s_monitorLoopRunning.load())
    {
        /* Do not stop (could hang) or free what the monitor thread may still use;
           the singleton's destructor stops the monitor at process exit. */
        g_object_ref(wifiDevice);
        FAIL() << "event monitor main loop did not start";
    }

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":2,\"status\":\"WIFI_STATE_DISCONNECTED\",\"success\":true}"));
    EXPECT_EQ(connectedSsid(), _T("{\"ssid\":\"\",\"bssid\":\"\",\"security\":0,\"strength\":0,\"frequency\":0,\"rate\":0,\"noise\":0,\"success\":true}"));

    /* Loop is running, so the quit is not lost; join before the device list goes away. */
    monitor->stopNetworkMangerEventMonitor();
    g_ptr_array_free(devices, FALSE);
}

TEST_F(WifiConnCacheEventTest, wifi_roam_ignores_old_ap)
{
    useRealSignals();
    stubAccessPoint(p_libnmWrapsImplMock, apA, ssid, "AA:AA:AA:AA:AA:AA", 2412, 20,
                    NM_802_11_AP_FLAGS_PRIVACY, NM_802_11_AP_SEC_KEY_MGMT_PSK, NM_802_11_AP_SEC_KEY_MGMT_SAE);
    stubAccessPoint(p_libnmWrapsImplMock, apB, ssid, "BB:BB:BB:BB:BB:BB", 2462, 85,
                    NM_802_11_AP_FLAGS_PRIVACY, NM_802_11_AP_SEC_KEY_MGMT_PSK, NM_802_11_AP_SEC_KEY_MGMT_SAE);
    const string bOnly = _T("{\"ssid\":\"TestWiFiNetwork\",\"bssid\":\"BB:BB:BB:BB:BB:BB\",\"security\":2,\"strength\":-39,\"frequency\":2.462,\"rate\":54000,\"noise\":0,\"success\":true}");

    activeAP = apA;
    Plugin::GnomeNetworkManagerEvents::startWifiConnTracking(wifiDevice);
    EXPECT_EQ(connectedSsid(), _T("{\"ssid\":\"TestWiFiNetwork\",\"bssid\":\"AA:AA:AA:AA:AA:AA\",\"security\":2,\"strength\":-78,\"frequency\":2.412,\"rate\":54000,\"noise\":0,\"success\":true}"));

    /* Roam: the device stays ACTIVATED, only the active access point changes. */
    activeAP = apB;
    g_object_notify(G_OBJECT(wifiDevice), NM_DEVICE_WIFI_ACTIVE_ACCESS_POINT);
    EXPECT_EQ(connectedSsid(), bOnly);

    /* The old AP is still alive (scan list) and keeps reporting changes. */
    ON_CALL(*p_libnmWrapsImplMock, nm_access_point_get_strength(apA)).WillByDefault(::testing::Return(85));
    g_object_notify(G_OBJECT(apA), NM_ACCESS_POINT_STRENGTH);
    EXPECT_EQ(connectedSsid(), bOnly);
}

TEST_F(WifiConnCacheEventTest, wifi_ap_strength_change_updates_cache)
{
    useRealSignals();
    stubAccessPoint(p_libnmWrapsImplMock, apB, ssid, "BB:BB:BB:BB:BB:BB", 2462, 85,
                    NM_802_11_AP_FLAGS_PRIVACY, NM_802_11_AP_SEC_KEY_MGMT_PSK, NM_802_11_AP_SEC_KEY_MGMT_SAE);
    activeAP = apB;
    Plugin::GnomeNetworkManagerEvents::startWifiConnTracking(wifiDevice);
    EXPECT_TRUE(connectedSsid().find("\"strength\":-39") != std::string::npos);

    ON_CALL(*p_libnmWrapsImplMock, nm_access_point_get_strength(apB)).WillByDefault(::testing::Return(20));
    g_object_notify(G_OBJECT(apB), NM_ACCESS_POINT_STRENGTH);
    EXPECT_EQ(connectedSsid(), _T("{\"ssid\":\"TestWiFiNetwork\",\"bssid\":\"BB:BB:BB:BB:BB:BB\",\"security\":2,\"strength\":-78,\"frequency\":2.462,\"rate\":54000,\"noise\":0,\"success\":true}"));
}

/* ────────────────────────────────────────────────────────────────────────────
 * PropertyFollower tests — real GObjects and real GLib signals (the GLib mock
 * forwards g_signal_connect_data to GLib by default); g_object_notify emits.
 * Assertions are on observable effects only: onChange counts, pending
 * handlers, and object lifetime.
 * ──────────────────────────────────────────────────────────────────────────── */

class PropertyFollowerTest : public ::testing::Test {
protected:
    GLibWrapsImplMock *p_gLibWrapsImplMock = nullptr;
    GObject *owner = nullptr;
    GObject *apA = nullptr;
    GObject *apB = nullptr;
    int changes = 0;

    static GObject* currentChild;
    static GObject* getChild(GObject*) { return currentChild; }
    static void onChange(GObject*, gpointer userData) { ++*static_cast<int*>(userData); }

    static bool hasStrengthHandler(GObject* ap)
    {
        guint notifyId = g_signal_lookup("notify", G_OBJECT_TYPE(ap));
        return g_signal_has_handler_pending(ap, notifyId, g_quark_from_string("strength"), FALSE);
    }

    void SetUp() override
    {
        p_gLibWrapsImplMock = new NiceMock<GLibWrapsImplMock>;
        GLibWraps::setImpl(p_gLibWrapsImplMock);
        owner = G_OBJECT(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
        apA = G_OBJECT(g_object_new(NM_TYPE_ACCESS_POINT, NULL));
        apB = G_OBJECT(g_object_new(NM_TYPE_ACCESS_POINT, NULL));
        currentChild = apA;
    }

    void TearDown() override
    {
        currentChild = nullptr;
        g_clear_object(&apA);
        g_clear_object(&apB);
        g_clear_object(&owner);
        GLibWraps::setImpl(nullptr);
        delete p_gLibWrapsImplMock;
        p_gLibWrapsImplMock = nullptr;
    }
};

GObject* PropertyFollowerTest::currentChild = nullptr;

TEST_F(PropertyFollowerTest, PropertyFollower_bindsCurrentChild)
{
    Plugin::PropertyFollower follower("active-access-point", &getChild, {"strength", "frequency"}, &onChange, &changes);
    follower.start(owner);

    EXPECT_EQ(apA, follower.current());
    EXPECT_EQ(0, changes); // start() does not call onChange

    g_object_notify(apA, "strength");
    g_object_notify(apA, "frequency");
    EXPECT_EQ(2, changes);

    g_object_notify(apA, "bssid"); // not watched
    EXPECT_EQ(2, changes);
}

TEST_F(PropertyFollowerTest, PropertyFollower_rebindIgnoresOldChild)
{
    Plugin::PropertyFollower follower("active-access-point", &getChild, {"strength", "frequency"}, &onChange, &changes);
    follower.start(owner);

    currentChild = apB;
    g_object_notify(owner, "active-access-point");
    EXPECT_EQ(apB, follower.current());
    EXPECT_EQ(1, changes);

    EXPECT_FALSE(hasStrengthHandler(apA));
    EXPECT_TRUE(hasStrengthHandler(apB));

    g_object_notify(apA, "strength");
    EXPECT_EQ(1, changes);
    g_object_notify(apB, "strength");
    EXPECT_EQ(2, changes);
}

TEST_F(PropertyFollowerTest, PropertyFollower_rebindToNull)
{
    Plugin::PropertyFollower follower("active-access-point", &getChild, {"strength", "frequency"}, &onChange, &changes);
    follower.start(owner);

    currentChild = nullptr;
    g_object_notify(owner, "active-access-point");
    EXPECT_EQ(nullptr, follower.current());
    EXPECT_EQ(1, changes);
    EXPECT_FALSE(hasStrengthHandler(apA));

    g_object_notify(apA, "strength");
    EXPECT_EQ(1, changes);
}

TEST_F(PropertyFollowerTest, PropertyFollower_stopSilencesAndReleasesRefs)
{
    Plugin::PropertyFollower follower("active-access-point", &getChild, {"strength", "frequency"}, &onChange, &changes);
    follower.start(owner);

    /* Drop the test's own references: the follower must keep both objects alive. */
    GObject *weakOwner = owner;
    GObject *weakAp = apA;
    g_object_add_weak_pointer(weakOwner, reinterpret_cast<gpointer*>(&weakOwner));
    g_object_add_weak_pointer(weakAp, reinterpret_cast<gpointer*>(&weakAp));
    GObject *keepOwner = G_OBJECT(g_object_ref(owner)); // to notify after stop
    g_clear_object(&owner);
    g_clear_object(&apA);
    EXPECT_NE(nullptr, weakAp);

    follower.stop();
    EXPECT_EQ(nullptr, follower.current());
    EXPECT_EQ(nullptr, weakAp); // child released by stop()

    g_object_notify(keepOwner, "active-access-point");
    EXPECT_EQ(0, changes);

    g_object_unref(keepOwner);
    EXPECT_EQ(nullptr, weakOwner); // owner released by stop()
}

TEST_F(PropertyFollowerTest, PropertyFollower_restartWhenHoldingLastOwnerRef)
{
    Plugin::PropertyFollower follower("active-access-point", &getChild, {"strength", "frequency"}, &onChange, &changes);
    follower.start(owner);

    /* Leave the follower holding the only reference to the owner, then restart with it. */
    GObject *weakOwner = owner;
    g_object_add_weak_pointer(weakOwner, reinterpret_cast<gpointer*>(&weakOwner));
    g_clear_object(&owner);
    follower.start(weakOwner);
    ASSERT_NE(nullptr, weakOwner);

    g_object_notify(weakOwner, "active-access-point");
    EXPECT_EQ(1, changes);

    follower.stop();
    EXPECT_EQ(nullptr, weakOwner);
}

TEST_F(PropertyFollowerTest, PropertyFollower_startTwiceDoesNotDuplicateHandlers)
{
    Plugin::PropertyFollower follower("active-access-point", &getChild, {"strength", "frequency"}, &onChange, &changes);
    follower.start(owner);
    follower.start(owner);

    g_object_notify(apA, "strength");
    EXPECT_EQ(1, changes);
    g_object_notify(owner, "active-access-point");
    EXPECT_EQ(2, changes);
}

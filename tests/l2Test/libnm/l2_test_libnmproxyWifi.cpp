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

class NetworkManagerWifiTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::NetworkManager> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    NMClient* gNmClient = NULL;

    WrapsImplMock *p_wrapsImplMock = nullptr;
    LibnmWrapsImplMock *p_libnmWrapsImplMock = nullptr;
    GLibWrapsImplMock *p_gLibWrapsImplMock = nullptr;
    Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImpl;

    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;

    NetworkManagerWifiTest()
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
        usleep(1000); // wait for the plugin to initialize
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

    virtual ~NetworkManagerWifiTest() override
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

TEST_F(NetworkManagerWifiTest, RegisteredMethods)
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

TEST_F(NetworkManagerWifiTest, get_wifi_device_devices_null)
{
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(reinterpret_cast<GPtrArray*>(NULL)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiDisconnect"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));
}

TEST_F(NetworkManagerWifiTest, WiFiAlreadyDisconnected)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_DISCONNECTED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiDisconnect"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, WiFiAlreadyConnected_failed)
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
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillOnce(::testing::Return(true))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_disconnect_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(false));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_disconnect_async(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDevice *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(device);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiDisconnect"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, WiFiAlreadyConnected_success)
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
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillOnce(::testing::Return(true))
        .WillOnce(::testing::Return(true));

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

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("WiFiDisconnect"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetConnectedSSID_deviceError)
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

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetConnectedSSID_ApNotConnected)
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

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T(""), response));
    EXPECT_EQ(response, _T("{\"ssid\":\"\",\"bssid\":\"\",\"security\":0,\"strength\":\"\",\"frequency\":\"\",\"rate\":\"\",\"noise\":\"\",\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetConnectedSSID_ApNotNull)
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
        .WillOnce(::testing::Return(NM_DEVICE_STATE_IP_CHECK));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_get_active_access_point(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMAccessPoint*>(NULL)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetConnectedSSID_Connected_psk)
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
        .WillOnce(::testing::Return(NM_DEVICE_STATE_IP_CHECK));

    // Create a fake SSID GBytes object
    const char* ssid_str = "TestWiFiNetwork";
    GBytes* fake_ssid = g_bytes_new_static(ssid_str, strlen(ssid_str));

    // Setup the Access Point mock calls
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_get_active_access_point(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMAccessPoint*>(0x833332)));
    
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
        .WillOnce(::testing::Return("AA:BB:CC:DD:EE:FF"));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_frequency(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(2462))); // Channel 11 (2.4GHz)
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_mode(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211Mode>(NM_802_11_MODE_INFRA)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_max_bitrate(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(54000))); // 54 Mbps
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_strength(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(85))); // 85% signal strength

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T(""), response));
    
    // Update expected response to match the mock data
    EXPECT_EQ(response, _T("{\"ssid\":\"TestWiFiNetwork\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"security\":2,\"strength\":\"-39\",\"frequency\":\"2.462\",\"rate\":\"54000\",\"noise\":\"0\",\"success\":true}"));

    // Free the GBytes object
    g_bytes_unref(fake_ssid);
    
    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetConnectedSSID_Connected_EAP)
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
        .WillOnce(::testing::Return(NM_DEVICE_STATE_IP_CHECK));

    // Create a fake SSID GBytes object
    const char* ssid_str = "TestWiFiNetwork";
    GBytes* fake_ssid = g_bytes_new_static(ssid_str, strlen(ssid_str));

    // Setup the Access Point mock calls
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_get_active_access_point(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMAccessPoint*>(0x833332)));
    
    // AP property mocks
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApFlags>(NM_802_11_AP_FLAGS_PRIVACY)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_wpa_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApSecurityFlags>(NM_802_11_AP_SEC_KEY_MGMT_802_1X)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_rsn_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApSecurityFlags>(NM_802_11_AP_SEC_KEY_MGMT_802_1X)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_ssid(::testing::_))
        .WillOnce(::testing::Return(fake_ssid));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_bssid(::testing::_))
        .WillOnce(::testing::Return("AA:BB:CC:DD:EE:FF"));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_frequency(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(2462))); // Channel 11 (2.4GHz)
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_mode(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211Mode>(NM_802_11_MODE_INFRA)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_max_bitrate(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(54000))); // 54 Mbps
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_strength(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(85))); // 85% signal strength

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T(""), response));
    
    // Update expected response to match the mock data
    EXPECT_EQ(response, _T("{\"ssid\":\"TestWiFiNetwork\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"security\":3,\"strength\":\"-39\",\"frequency\":\"2.462\",\"rate\":\"54000\",\"noise\":\"0\",\"success\":true}"));

    // Free the GBytes object
    g_bytes_unref(fake_ssid);
    
    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetConnectedSSID_Connected_ssidNull)
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
        .WillOnce(::testing::Return(NM_DEVICE_STATE_IP_CHECK));

    // Create a fake SSID GBytes object
    const char* ssid_str = "";
    GBytes* fake_ssid = g_bytes_new_static(ssid_str, 0);

    // Setup the Access Point mock calls
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_get_active_access_point(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<NMAccessPoint*>(0x833332)));
    
    // AP property mocks
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApFlags>(NM_802_11_AP_FLAGS_PRIVACY)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_wpa_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApSecurityFlags>(NM_802_11_AP_SEC_NONE)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_rsn_flags(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211ApSecurityFlags>(NM_802_11_AP_SEC_NONE)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_ssid(::testing::_))
        .WillOnce(::testing::Return(reinterpret_cast<GBytes*>(NULL)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_bssid(::testing::_))
        .WillOnce(::testing::Return("AA:BB:CC:DD:EE:FF"));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_frequency(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(2462))); // Channel 11 (2.4GHz)
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_mode(::testing::_))
        .WillOnce(::testing::Return(static_cast<NM80211Mode>(NM_802_11_MODE_INFRA)));
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_max_bitrate(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(54000))); // 54 Mbps
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_access_point_get_strength(::testing::_))
        .WillOnce(::testing::Return(static_cast<guint32>(20))); // 20% signal strength

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetConnectedSSID"), _T(""), response));
    
    // Update expected response to match the mock data
    EXPECT_EQ(response, _T("{\"ssid\":\"\",\"bssid\":\"AA:BB:CC:DD:EE:FF\",\"security\":1,\"strength\":\"-78\",\"frequency\":\"2.462\",\"rate\":\"54000\",\"noise\":\"0\",\"success\":true}"));

    // Free the GBytes object
    g_bytes_unref(fake_ssid);
    
    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetWifiState_Disconnect)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_DISCONNECTED))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_DISCONNECTED));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":2,\"status\":\"WIFI_STATE_DISCONNECTED\",\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetWifiState_Connected)
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
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":5,\"status\":\"WIFI_STATE_CONNECTED\",\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetWifiState_Pairing)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_PREPARE))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_CONFIG));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":3,\"status\":\"WIFI_STATE_PAIRING\",\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetWifiState_unknown)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_NEED_AUTH))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_UNKNOWN));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":1,\"status\":\"WIFI_STATE_DISABLED\",\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetWifiState_Connecting)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_NEED_AUTH))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_IP_CONFIG));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":4,\"status\":\"WIFI_STATE_CONNECTING\",\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, GetWifiState_Disabled)
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

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("GetWifiState"), _T(""), response));
    EXPECT_EQ(response, _T("{\"state\":1,\"status\":\"WIFI_STATE_DISABLED\",\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, StartWiFiScan_Failed)
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

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, StartWiFiScan_with_SSIDList)
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

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), _T("{\"ssid\":[\"Testssid_1\", \"Testssid_2\"]}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, StartWiFiScan_with_Frequency)
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

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), _T("{\"frequency\":\"5\", \"ssids\":[\"Testssid_1\", \"Testssid_2\"]}"), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, StartWiFiScan_with_failed_with_error)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);
    
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED)); // Device must be active for scanning

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_request_scan_async(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDeviceWifi *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
            if (callback) {
                GObject* source_object = G_OBJECT(device);
                GAsyncResult* result = nullptr;
                callback(source_object, result, user_data);
            }
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_request_scan_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDeviceWifi *device, GAsyncResult *result, GError **error) -> gboolean {
            *error = g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED, "WiFi scanning failed: %s", "Network interface is busy or unavailable");
            return FALSE;
        }));

    // Execute the test
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":false}"));

    // Cleanup
    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, StartWiFiScan_success)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED)); // Device must be active for scanning

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_request_scan_async(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDeviceWifi *device, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
            if (callback) {
                GObject* source_object = G_OBJECT(device);
                GAsyncResult* result = nullptr;
                callback(source_object, result, user_data);
            }
        }));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_request_scan_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDeviceWifi *device, GAsyncResult *result, GError **error) -> gboolean {
            return true;
        }));

    // Execute the test
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    // Cleanup
    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}

TEST_F(NetworkManagerWifiTest, StartWiFiScan_with_option_success)
{
    GPtrArray* fakeDevices = g_ptr_array_new();
    NMDevice *deviceDummy = static_cast<NMDevice*>(g_object_new(NM_TYPE_DEVICE_WIFI, NULL));
    g_ptr_array_add(fakeDevices, deviceDummy);

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_devices(::testing::_))
        .WillRepeatedly(::testing::Return(fakeDevices));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_iface(::testing::_))
        .WillRepeatedly(::testing::Return("wlan0"));
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED)); // Device must be active for scanning

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_request_scan_options_async(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDeviceWifi *device, GVariant *options, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) {
                if (callback) {
                    GObject* source_object = G_OBJECT(device);
                    GAsyncResult* result = nullptr;
                    callback(source_object, result, user_data);
                }
            }
    ));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_wifi_request_scan_finish(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](NMDeviceWifi *device, GAsyncResult *result, GError **error) -> gboolean {
            return true;
        }));

    // Execute the test
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), _T("{\"ssids\":[\"Testssid_1\"]}"), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    // Cleanup
    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}


/*
TEST_F(NetworkManagerWifiTest, StartWiFiScan_Failed)
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
        .WillOnce(::testing::Return(NM_DEVICE_STATE_ACTIVATED));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_main_loop_is_running(::testing::_))
        .WillOnce(::testing::Return(true))
        .WillOnce(::testing::Return(true));

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

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("StartWiFiScan"), _T(""), response));
    EXPECT_EQ(response, _T("{\"success\":true}"));

    g_object_unref(deviceDummy);
    g_ptr_array_free(fakeDevices, TRUE);
}
*/
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

class NetworkManagerEventTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::NetworkManager> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    NMClient* gNmClient = NULL;
    /* every event callback */
    GCallback nmClientRunCb = NULL;
    GCallback nmStateChange = NULL;
    GCallback nmPrimaryConnCb = NULL;
    GCallback nmDeviceAdd = NULL;
    GCallback nmDeviceRemove = NULL;

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

TEST_F(NetworkManagerEventTest, EventCallbacksTest)
{
     EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_device_by_iface(::testing::_, ::testing::_))
            .WillOnce(::testing::Return(reinterpret_cast<NMDevice*>(0x100178)));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_device_get_state(::testing::_))
        .WillRepeatedly(::testing::Return(NM_DEVICE_STATE_UNMANAGED));

    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_state(::testing::_))
        .WillOnce(::testing::Return(NM_STATE_DISCONNECTED))
        .WillOnce(::testing::Return(NM_STATE_CONNECTED_GLOBAL)); 
    EXPECT_CALL(*p_libnmWrapsImplMock, nm_client_get_nm_running(::testing::_))
        .WillOnce(::testing::Return(FALSE));

    EXPECT_CALL(*p_gLibWrapsImplMock, g_signal_connect_data(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(
        [&](gpointer instance, const gchar *detailed_signal, GCallback c_handler,
            gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags) -> gulong {

            if(detailed_signal == NULL)
                printf("g_signal_connect_data: NULL\n");
            else
                printf("g_signal_connect_data - siganal: %s\n", detailed_signal);

            std::string signal(detailed_signal ? detailed_signal : "");

            if (signal == "notify::nm-running") {
                ((void (*)(NMClient *, GParamSpec *, gpointer))c_handler)(NULL, NULL, NULL);
            } else if (signal == "notify::state") {
                ((void (*)(NMClient *, GParamSpec *, gpointer))c_handler)(NULL, NULL, NULL);
                ((void (*)(NMClient *, GParamSpec *, gpointer))c_handler)(NULL, NULL, NULL);
            } else if (signal == "notify::primary-connection") {
                nmPrimaryConnCb = c_handler;
            } else if (signal == "device-added") {
                nmDeviceAdd = c_handler;
            } else if (signal == "device-removed") {
                nmDeviceRemove = c_handler;
            }
            return 0;
        }));

        EXPECT_EQ(string(""), (response = plugin->Initialize(&service)));
}

TEST_F(NetworkManagerEventTest, EventCallbacksTest2)
{
}
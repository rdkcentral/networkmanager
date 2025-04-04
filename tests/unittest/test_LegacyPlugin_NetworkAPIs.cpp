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
#include <iostream>
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include "NetworkManagerLogger.h"
#include "LegacyNetworkAPIs.h"
#include "ThunderPortability.h"

using namespace std;
using namespace WPEFramework;
using ::testing::NiceMock;

class NetworkTest : public ::testing::Test  {
protected:
    Core::ProxyType<Plugin::Network> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    ServiceMock services;
     
    NetworkTest()
      : plugin(Core::ProxyType<Plugin::Network>::Create())
       , handler(*(plugin))
       , INIT_CONX(1, 0)
   {
   	
   }
   virtual ~NetworkTest() override
    {
    }
};

class NetworkInitializedTest : public NetworkTest {
protected:
    NiceMock<ServiceMock> service;
    NetworkInitializedTest()
    {
        EXPECT_EQ(string("Failed to get IShell for 'NetworkManager'"), plugin->Initialize(&service));
 
         EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::Invoke(
                [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return nullptr;
                }));
    }
 
    virtual ~NetworkInitializedTest() override
    {
        plugin->Deinitialize(&service);
    }
   
};
 

TEST_F(NetworkTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getStbIp")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getInterfaces")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isInterfaceEnabled")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setInterfaceEnabled")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDefaultInterface")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getIPSettings")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isConnectedToInternet")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPublicIP")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSTBIPFamily")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setConnectivityTestEndpoints")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getCaptivePortalURI")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("startConnectivityMonitoring")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("stopConnectivityMonitoring")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setDefaultInterface")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setIPSettings")));
}



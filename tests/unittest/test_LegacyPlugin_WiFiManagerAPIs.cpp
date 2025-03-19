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
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include "NetworkManagerLogger.h"
#include "LegacyWiFiManagerAPIs.h"
#include "ThunderPortability.h"
 
using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::NiceMock;
 
class WiFiManagerTest : public ::testing::Test  {
protected:
    Core::ProxyType<Plugin::WiFiManager> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    ServiceMock services;
     
    WiFiManagerTest()
      : plugin(Core::ProxyType<Plugin::WiFiManager>::Create())
       , handler(*(plugin))
       , INIT_CONX(1, 0)
   {
   
   }
 
   virtual ~WiFiManagerTest() override
    {
    }
};
 
class WiFiManagerInitializedTest : public WiFiManagerTest {
protected:
    NiceMock<ServiceMock> service;
    WiFiManagerInitializedTest()
    {
        EXPECT_EQ(string(""), plugin->Initialize(&service));
   
        EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::Invoke(
                [&](const uint32_t, const string& name) -> void* {
                    EXPECT_EQ(name, string(_T("org.rdk.NetworkManager.1")));
                    return nullptr;
                }));
    }
 
    virtual ~WiFiManagerInitializedTest() override
    {
        plugin->Deinitialize(&service);
    }
   
};
 
TEST_F(WiFiManagerTest, TestedAPIsShouldExist)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("startScan")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("stopScan")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("connect")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("disconnect")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("initiateWPSPairing")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("cancelWPSPairing")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("saveSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("clearSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPairedSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPairedSSIDInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isPaired")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getCurrentState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getConnectedSSID")));
}
 
TEST_F(WiFiManagerInitializedTest, startScan)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("startScan"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}
 
TEST_F(WiFiManagerInitializedTest, stopScan)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("stopScan"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(WiFiManagerInitializedTest, isPaired)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("isPaired"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(WiFiManagerInitializedTest, disconnect)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("disconnect"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(WiFiManagerInitializedTest, initiateWPSPairing)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("initiateWPSPairing"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(WiFiManagerInitializedTest, cancelWPSPairing)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("cancelWPSPairing"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(WiFiManagerInitializedTest, clearSSID)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("clearSSID"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}
TEST_F(WiFiManagerInitializedTest, getPairedSSID)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("getPairedSSID"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(WiFiManagerInitializedTest, getPairedSSIDInfo)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("getPairedSSIDInfo"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(WiFiManagerInitializedTest, getCurrentState)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("getCurrentState"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}
TEST_F(WiFiManagerInitializedTest, getConnectedSSID)
{
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, handler.Invoke(connection, _T("getConnectedSSID"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}


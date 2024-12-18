#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include "NetworkManagerLogger.h"
#include "LegacyPlugin_WiFiManagerAPIs.h"
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
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("cancelWPSPairing")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("saveSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("clearSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPairedSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getPairedSSIDInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("isPaired")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getCurrentState")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getConnectedSSID")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSupportedSecurityModes")));
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

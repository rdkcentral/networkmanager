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

class WiFiInitializedEventTest : public WiFiManagerTest {
protected:
    FactoriesImplementation factoriesImplementation;
    PLUGINHOST_DISPATCHER* dispatcher;

    WiFiInitializedEventTest()
        : WiFiManagerTest()
    {
        PluginHost::IFactories::Assign(&factoriesImplementation);

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
           plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&this->services);
    }

    virtual ~WiFiInitializedEventTest() override
    {

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






#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include "NetworkManagerLogger.h"
#include "LegacyPlugin_NetworkAPIs.h"
#include "ThunderPortability.h"

using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;

 
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






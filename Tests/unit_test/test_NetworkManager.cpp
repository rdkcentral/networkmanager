#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <iostream>
#include "NetworkManagerLogger.h"
#include "NetworkManager.h"
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include "IarmBusMock.h"
#include "ThunderPortability.h"

using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::NiceMock;

class NetworkManagerTestBase : public ::testing::Test {
public:
	IarmBusImplMock   *p_iarmBusImplMock = nullptr ;
	NetworkManagerTestBase()
	{
		 p_iarmBusImplMock  = new NiceMock <IarmBusImplMock>;
                 IarmBus::setImpl(p_iarmBusImplMock);
	}
	
       ~NetworkManagerTestBase() {
        	delete p_iarmBusImplMock;
        	IarmBus::setImpl(nullptr);
       }

};

class NetworkManagerTest : public NetworkManagerTestBase {
protected:

    Core::ProxyType<Plugin::NetworkManager> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    string response;
    
    Core::JSONRPC::Message message;
    
    NetworkManagerTest() : plugin(Core::ProxyType<Plugin::NetworkManager>::Create()),
		           handler(*plugin),
		           INIT_CONX(1, 0)
    {
	    
    }
    
};

TEST_F(NetworkManagerTest, TestFunShouldExist)
{
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("SetLogLevel")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetLogLevel")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetAvailableInterfaces")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetPrimaryInterface")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("SetPrimaryInterface")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetInterfaceState")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("SetInterfaceState")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetIPSettings")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("SetIPSettings")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetStunEndpoint")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("SetStunEndpoint")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetConnectivityTestEndpoints")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("SetConnectivityTestEndpoints")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("IsConnectedToInternet")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetCaptivePortalURI")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("StartConnectivityMonitoring")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("StopConnectivityMonitoring")));
    EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Exists(_T("GetPublicIP")));
} 


TEST_F(NetworkManagerTest, SetLogLevel)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("SetLogLevel"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetLogLevel)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetLogLevel"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetAvailableInterfaces)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetAvailableInterfaces"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetPrimaryInterface)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetPrimaryInterface"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, SetPrimaryInterface)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("SetPrimaryInterface"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetInterfaceState)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetInterfaceState"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 


TEST_F(NetworkManagerTest, SetInterfaceState)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("SetInterfaceState"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetIPSettings)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetIPSettings"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, SetIPSettings)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("SetIPSettings"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetStunEndpoint)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetStunEndpoint"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, SetStunEndpoint)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("SetStunEndpoint"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetConnectivityTestEndpoints)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetCOnnectivityTestEndpoints"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, SetConnectivityTestEndpoints)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("SetConnectivityTestEndpoints"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, IsConnectedToInternet)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("IsConnectedToInternet"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetCaptivePortalURI)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetCaptivePortalURI"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, StartConnectivityMonitoring)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("StartConnectivityMonitoring"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, StopConnectivityMonitoring)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("StopConnectivityMonitoring"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 

TEST_F(NetworkManagerTest, GetPublicIP)
{ 
	EXPECT_EQ(Core::ERROR_UNKNOWN_KEY, handler.Invoke(connection, _T("GetPublicIP"), _T("{ \"success\": true, \"error\": \"...		\" }"), response));
 	EXPECT_EQ(response, string(""));
 } 




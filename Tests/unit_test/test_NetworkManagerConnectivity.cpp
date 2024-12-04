#include "NetworkManagerImplementation.h"
#include "NetworkManagerConnectivity.h"

#include <cstring>
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>
#include <map>
#include <curl/curl.h>
#include <condition_variable>
#include <mutex>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace std;
using namespace WPEFramework;

class ConnectivityMonitorTest : public ::testing::Test {
protected:
   WPEFramework::Plugin::ConnectivityMonitor cm;
};

TEST_F(ConnectivityMonitorTest, StartContinuousMonitor_Success) {
    int timeout = 30;
    bool result = cm.startContinuousConnectivityMonitor(timeout);
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, StartContinuousMonitor_FailureNegativeTimeout) {
    int timeout = -1;
    bool result = cm.startContinuousConnectivityMonitor(timeout);
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, StartMonitorWithTimeoutLessThanMinimum) {
    int timeout = 3;
    bool result = cm.startContinuousConnectivityMonitor(timeout);
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, MonitorFailsToStart) {
    int timeout = 0;  
    bool result = cm.startContinuousConnectivityMonitor(timeout);
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, StopContinuousMonitor_WhenStarted) {
    int timeout = 30;
    cm.startContinuousConnectivityMonitor(timeout);  
    bool result = cm.stopContinuousConnectivityMonitor(); 
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, StopContinuousMonitor_WhenNotStarted) {
    bool result = cm.stopContinuousConnectivityMonitor();  
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, StopContinuousMonitor_AfterMultipleStartsAndStops) {
    int timeout = 30;
    cm.startContinuousConnectivityMonitor(timeout);  
    bool result = cm.stopContinuousConnectivityMonitor();
    EXPECT_TRUE(result);
    
    cm.startContinuousConnectivityMonitor(timeout);
    result = cm.stopContinuousConnectivityMonitor();
    EXPECT_TRUE(result);
    
    cm.startContinuousConnectivityMonitor(timeout);
    result = cm.stopContinuousConnectivityMonitor();
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, StopContinuousMonitor_LongRunningMonitor) {
    int timeout = 1000;
    cm.startContinuousConnectivityMonitor(timeout);  
    std::this_thread::sleep_for(std::chrono::seconds(2)); 
 
    bool result = cm.stopContinuousConnectivityMonitor();
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, StartMonitor_WithInterfaceStatus) {
    bool result = cm.startConnectivityMonitor();
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, StartMonitor_NotifyIfAlreadyMonitoring) {
    bool result =  false;
    result = cm.startConnectivityMonitor();
    EXPECT_TRUE(result);
    result = cm.startConnectivityMonitor(); 
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, SetEndpoints_Valid) {
    std::vector<std::string> endpoints = {"https://github.com/rdkcentral", "https://github.com/rdkcentral/rdkservices"};
    cm.setConnectivityMonitorEndpoints(endpoints);
    EXPECT_EQ(cm.getConnectivityMonitorEndpoints(), endpoints);  
}

TEST_F(ConnectivityMonitorTest, SetEndpoints_EmptyList) {
    std::vector<std::string> endpoints;  
    cm.setConnectivityMonitorEndpoints(endpoints);
    EXPECT_TRUE(cm.getConnectivityMonitorEndpoints().empty());  
}

TEST_F(ConnectivityMonitorTest, SetEndpoints_InvalidShortEndpoints) {
    std::vector<std::string> endpoints = {"ab", "htt", "xyz"};
    cm.setConnectivityMonitorEndpoints(endpoints);
    EXPECT_TRUE(cm.getConnectivityMonitorEndpoints().empty());
}

TEST_F(ConnectivityMonitorTest, SetEndpoints_DuplicateEndpoints) {
    std::vector<std::string> endpoints = {"https://github.com", "https://github.com"};
    cm.setConnectivityMonitorEndpoints(endpoints);
    EXPECT_EQ(cm.getConnectivityMonitorEndpoints().size(), 2);
}

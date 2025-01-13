/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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
#include "NetworkManagerImplementation.h"
#include "NetworkManagerConnectivity.h"
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

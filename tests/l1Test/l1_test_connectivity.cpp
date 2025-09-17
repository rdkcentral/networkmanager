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
#include "NetworkManagerImplementation.h"
#include "NetworkManagerConnectivity.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
namespace WPEFramework
{
   namespace Plugin
    {
        NetworkManagerImplementation* _instance = nullptr;
        void NetworkManagerImplementation::ReportInternetStatusChange(const InternetStatus prevState, const InternetStatus currState, const string interface)
        {
            return;
        }
    }
}

class ConnectivityMonitorTest : public ::testing::Test {
protected:
   WPEFramework::Plugin::ConnectivityMonitor cm;
};

TEST_F(ConnectivityMonitorTest, StartConnectivityMonitor_Success) {
    bool result = cm.startConnectivityMonitor();
    EXPECT_TRUE(result);
    result = cm.startConnectivityMonitor();
    EXPECT_TRUE(result);
}

TEST_F(ConnectivityMonitorTest, SetEndpoints_Valid) {
    std::vector<std::string> endpoints = {"https://github.com/rdkcentral", "https://github.com/rdkcentral/rdkservices"};
    cm.setConnectivityMonitorEndpoints(endpoints);
    EXPECT_EQ(cm.getConnectivityMonitorEndpoints(), endpoints);  
}

TEST_F(ConnectivityMonitorTest, SetEndpoints_InvalidShortEndpoints) {
    std::vector<std::string> endpoints = {"ab", "htt", "xyz"};
    cm.setConnectivityMonitorEndpoints(endpoints);
    EXPECT_TRUE(cm.getConnectivityMonitorEndpoints().empty());
}

TEST_F(ConnectivityMonitorTest, SetEndpoints_DuplicateEndpoints) {
    std::vector<std::string> endpoints = {"https://github.com", "https://github.com"};
    cm.setConnectivityMonitorEndpoints(endpoints);
    EXPECT_EQ((int)cm.getConnectivityMonitorEndpoints().size(), 2);
}

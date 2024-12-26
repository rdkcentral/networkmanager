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
    EXPECT_EQ(cm.getConnectivityMonitorEndpoints().size(), 2);
}

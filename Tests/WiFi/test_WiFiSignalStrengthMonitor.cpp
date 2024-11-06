#include "WiFiSignalStrengthMonitor.h"
#include "NetworkManagerImplementation.h"
#include "NetworkManagerLogger.h"
#include "INetworkManager.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <list>
#include <string>

using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
namespace WPEFramework
{
   namespace Plugin
    {
       NetworkManagerImplementation* _instance;
        void NetworkManagerImplementation::ReportWiFiSignalStrengthChangedEvent(const string ssid, const string signalLevel, const WiFiSignalQuality signalQuality)
       {
            return;
        }
        void NetworkManagerImplementation::ReportInternetStatusChangedEvent(const InternetStatus oldState, const InternetStatus newstate)
        {
            return;
        }
    }
}

class WiFiSignalStrengthMonitorTest : public ::testing::Test {
 protected:
     WPEFramework::Plugin::WiFiSignalStrengthMonitor monitor;
     void SetUp() override {
     }
     void TearDown() override {
    }
};
#define BUFFER_SIZE 1024

TEST_F(WiFiSignalStrengthMonitorTest, GetSignalData_Connected) {
    std::string ssid = "TestSSID";
    Exchange::INetworkManager::WiFiSignalQuality quality;
    std::string strengthOut= "-55";
    monitor.getSignalData(ssid, quality, strengthOut);
}

TEST_F(WiFiSignalStrengthMonitorTest, StartWiFiSignalStrengthMonitor) {
    monitor.startWiFiSignalStrengthMonitor(1);  

}

int main(int argc, char **argv) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}


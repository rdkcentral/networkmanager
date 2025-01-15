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
        NetworkManagerImplementation* _instance = nullptr;
        void NetworkManagerImplementation::ReportWiFiSignalStrengthChange(const string ssid, const string strength, const WiFiSignalQuality quality)
        {
            return;
        }

        void NetworkManagerImplementation::ReportInternetStatusChange(const InternetStatus prevState, const InternetStatus currState)
        {
            return;
        }
    }
}

class WiFiSignalStrengthMonitorTest : public ::testing::Test {
 protected:
     WPEFramework::Plugin::WiFiSignalStrengthMonitor monitor;
};

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

/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <UpnpDiscoveryManager.h>
#include <thread>

using ::testing::_;
using ::testing::Return;
using ::testing::Mock;

class UpnpDiscoveryManagerTest : public ::testing::Test {
protected:
   UpnpDiscoveryManager upnpDiscover;
   GMainLoop*           mainLoop;
   void SetUp() override {
        mainLoop = g_main_loop_new(nullptr, FALSE);
    }
};

std::string getInterfaceWithDefaultRoute() {
    std::string command = "ip -4 route show | grep default | awk '{print $5}'";
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Remove trailing newline character
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

TEST_F(UpnpDiscoveryManagerTest, FindGatewayDeviceTest)
{
    // Invalid interface name
    EXPECT_EQ(false, upnpDiscover.findGatewayDevice("123"));
 
    // Get router details on actual interface
    std::string interfaceName = getInterfaceWithDefaultRoute();
    if (!interfaceName.empty()) {
        std::cout << "Interface with default route: " << interfaceName << std::endl;
    } else {
        std::cout << "No default route interface found." << std::endl;
    }

    // Valid interface name
    EXPECT_EQ(true, upnpDiscover.findGatewayDevice(interfaceName));
}

TEST_F(UpnpDiscoveryManagerTest, routerDiscovery)
{   
   auto threadHandler = [&]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << "Thread finished sleeping." << std::endl;
	if (this->mainLoop)
        {
            g_main_loop_quit(this->mainLoop);
            g_main_loop_unref(this->mainLoop);
        }
    };
    // Get router details on actual interface
    std::string interfaceName = getInterfaceWithDefaultRoute();
    if (!interfaceName.empty()) {
        std::cout << "Interface with default route: " << interfaceName << std::endl;
    } else {
        std::cout << "No default route interface found." << std::endl;
    }
    mainLoop = g_main_loop_new(NULL, FALSE);
    std::thread workerThread(threadHandler);
    // Get the router details, mostly will timeout in corporate network
    EXPECT_EQ(true, upnpDiscover.findGatewayDevice(interfaceName));
    g_main_loop_run(mainLoop);
    workerThread.join();
}

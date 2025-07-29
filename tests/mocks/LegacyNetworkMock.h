/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#ifndef MOCKNETWORK_H
#define MOCKNETWORK_H

#include <gmock/gmock.h>
#include "Module.h"

class StubNetwork : public WPEFramework::Plugin::Network {
public:
    void onInterfaceStateChange(const JsonObject& parameters) {
        WPEFramework::Plugin::Network::ReportonInterfaceStateChange(parameters); // Call the method on this object
    }

    void onActiveInterfaceChange(const JsonObject& parameters) {
        EXPECT_EQ(parameters["prevActiveInterface"].String(), "eth0");
        EXPECT_EQ(parameters["currentActiveInterface"].String(), "wlan0");
        WPEFramework::Plugin::Network::ReportonActiveInterfaceChange(parameters);
    }

    void onIPAddressChange(const JsonObject& parameters) {
        WPEFramework::Plugin::Network::ReportonIPAddressChange(parameters);
    }

    void onInternetStatusChange(const JsonObject& parameters) {
        EXPECT_EQ(parameters["state"].String(), "CONNECTED");
        EXPECT_EQ(parameters["status"].String(), "OK");
        WPEFramework::Plugin::Network::ReportonInternetStatusChange(parameters);
    }

    string Information() const
    {
        WPEFramework::Plugin::Network::Information();
        return(string());
    }

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (uint32_t), (override));
    MOCK_METHOD(const std::string, Initialize, (WPEFramework::PluginHost::IShell*), (override));
    MOCK_METHOD(void, Deinitialize, (WPEFramework::PluginHost::IShell*), (override));
    MOCK_METHOD(void, ReportonInterfaceStateChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonActiveInterfaceChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonIPAddressChange, (const JsonObject&), ());
    MOCK_METHOD(void, ReportonInternetStatusChange, (const JsonObject&), ());
    MOCK_METHOD(void, subscribeToEvents, (), ());
};
#endif

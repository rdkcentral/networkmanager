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

#ifndef MOCKWIFIMANAGER_H
#define MOCKWIFIMANAGER_H

#include <gmock/gmock.h>
#include "Module.h"

class StubWiFi : public WPEFramework::Plugin::WiFiManager {
public:
    void onWiFiStateChange(const JsonObject& parameters) {
        WPEFramework::Plugin::WiFiManager::onWiFiStateChange(parameters); // Call the method on this object
    }

    void onAvailableSSIDs(const JsonObject& parameters) {
        WPEFramework::Plugin::WiFiManager::onAvailableSSIDs(parameters); // Call the method on this object
    }

    void onWiFiSignalQualityChange(const JsonObject& parameters) {
        WPEFramework::Plugin::WiFiManager::onWiFiSignalQualityChange(parameters); // Call the method on this object
    }

    string Information() const
    {
        WPEFramework::Plugin::WiFiManager::Information();
        return(string());
    }

   // MOCK_METHOD(uint32_t, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (uint32_t), (override));
    MOCK_METHOD(const std::string, Initialize, (WPEFramework::PluginHost::IShell*), (override));
    MOCK_METHOD(void, Deinitialize, (WPEFramework::PluginHost::IShell*), (override));
    MOCK_METHOD(void, subscribeToEvents, (), ());
    MOCK_METHOD(uint32_t, cancelWPSPairing, (const JsonObject& parameters, JsonObject& response), ());
};
#endif

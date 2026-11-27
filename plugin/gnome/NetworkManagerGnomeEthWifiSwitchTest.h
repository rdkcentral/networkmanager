/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2026 RDK Management
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

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "INetworkManager.h"

namespace WPEFramework
{
    namespace Plugin
    {
        /*
         * Spike test only (Eth and Wi-Fi co-existence), not the production failover.
         * NO_INTERNET on the Ethernet interface -> if Wi-Fi is connected and its gateway
         * has a complete ARP entry, set the Ethernet route-metric to 20100 (runtime
         * Reapply, not saved) so Wi-Fi (600) takes the default route.
         * One direction only, no back-off, hardcoded values.
         */
        class EthWifiSwitchTest
        {
        public:
            static EthWifiSwitchTest& getInstance();
            void onInternetStatusChange(Exchange::INetworkManager::InternetStatus status, const std::string& iface);

        private:
            EthWifiSwitchTest() = default;
            ~EthWifiSwitchTest();
            EthWifiSwitchTest(const EthWifiSwitchTest&) = delete;
            EthWifiSwitchTest& operator=(const EthWifiSwitchTest&) = delete;

            void switchToWifi();

            std::mutex m_mutex;
            std::thread m_worker;
            std::atomic<bool> m_running{false};
        };
    }
}

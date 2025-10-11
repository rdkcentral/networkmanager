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

#pragma once

#include <string>

typedef enum _SsidSecurity
{
    NET_WIFI_SECURITY_NONE = 0,
    NET_WIFI_SECURITY_WEP_64,
    NET_WIFI_SECURITY_WEP_128,
    NET_WIFI_SECURITY_WPA_PSK_TKIP,
    NET_WIFI_SECURITY_WPA_PSK_AES,
    NET_WIFI_SECURITY_WPA2_PSK_TKIP,
    NET_WIFI_SECURITY_WPA2_PSK_AES,
    NET_WIFI_SECURITY_WPA_ENTERPRISE_TKIP,
    NET_WIFI_SECURITY_WPA_ENTERPRISE_AES,
    NET_WIFI_SECURITY_WPA2_ENTERPRISE_TKIP,
    NET_WIFI_SECURITY_WPA2_ENTERPRISE_AES,
    NET_WIFI_SECURITY_WPA_WPA2_PSK,
    NET_WIFI_SECURITY_WPA_WPA2_ENTERPRISE,
    NET_WIFI_SECURITY_WPA3_PSK_AES,
    NET_WIFI_SECURITY_WPA3_SAE,
    NET_WIFI_SECURITY_NOT_SUPPORTED = 99,
} SsidSecurity;

namespace WPEFramework
{
    namespace Plugin
    {
        class NetworkManagerMfrManager
        {
        public:
            static NetworkManagerMfrManager* getInstance();
            bool saveWiFiSettingsToMfr();
            bool clearWiFiSettingsFromMfr();

        private:
            NetworkManagerMfrManager();
            ~NetworkManagerMfrManager();
            
            // Disable copy constructor and assignment operator
            NetworkManagerMfrManager(const NetworkManagerMfrManager&) = delete;
            NetworkManagerMfrManager& operator=(const NetworkManagerMfrManager&) = delete;

            bool saveWiFiSettingsToMfrSync(const std::string& ssid, const std::string& passphrase, int security);
        };
    } // namespace Plugin
} // namespace WPEFramework

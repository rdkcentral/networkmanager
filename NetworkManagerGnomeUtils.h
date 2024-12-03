/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#pragma once
#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include <string.h>
#include <iostream>
#include <atomic>
#include "Module.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class nmUtils
        {

            public:
               static bool getInterfacesName();
               static const char* wlanIface();
               static const char* ethIface();
               static const char* convertPercentageToSignalStrengtStr(int percentage);
               static bool caseInsensitiveCompare(const std::string& str1, const std::string& str2);
               static uint8_t wifiSecurityModeFromAp(guint32 flags, guint32 wpaFlags, guint32 rsnFlags);
               static std::string wifiFrequencyFromAp(guint32 apFreq);
               static std::string getSecurityModeString(guint32 flags, guint32 wpaFlags, guint32 rsnFlags);
               static bool apToJsonObject(NMAccessPoint *ap, JsonObject& ssidObj);
               static void printActiveSSIDsOnly(NMDeviceWifi *wifiDevice);
               static NMDeviceState ifaceState(NMClient *client, const char* interface);
        };

    }
}
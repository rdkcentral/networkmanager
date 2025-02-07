/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/
#include <cstdio>
#include <thread>
#include <chrono>
#include <atomic>
#include <stdlib.h>
#include "NetworkManagerLogger.h"
#include "NetworkManagerImplementation.h"
#include "WiFiSignalStrengthMonitor.h"

#define BUFFER_SIZE 512
#define rssid_command "wpa_cli signal_poll"
#define ssid_command "wpa_cli status"

namespace WPEFramework
{
    namespace Plugin
    {
        static const float signalStrengthThresholdExcellent = -50.0f;
        static const float signalStrengthThresholdGood = -60.0f;
        static const float signalStrengthThresholdFair = -67.0f;
        extern NetworkManagerImplementation* _instance;

        void WiFiSignalStrengthMonitor::getSignalData(std::string &ssid, Exchange::INetworkManager::WiFiSignalQuality &quality, std::string &strengthOut)
        {
            float signalStrengthOut = 0.0f;
            char buff[BUFFER_SIZE] = {'\0'};
            string signalStrength = "";

            if (_instance != nullptr)
            {
                Exchange::INetworkManager::WiFiSSIDInfo ssidInfo{};
                _instance->GetConnectedSSID(ssidInfo);
                ssid = ssidInfo.ssid;
                signalStrength = ssidInfo.strength;
            }
            else
            {
                NMLOG_FATAL("NetworkManagerImplementation pointer error !");
                return;
            }

            if (ssid.empty())
            {
                quality = Exchange::INetworkManager::WIFI_SIGNAL_DISCONNECTED;
                strengthOut = "0.00";
                return;
            }

            if (!signalStrength.empty()) {
                signalStrengthOut = std::stof(signalStrength.c_str());
                strengthOut = signalStrength;
            }
            else
                NMLOG_ERROR("signalStrength is empty");

            NMLOG_DEBUG("SSID = %s Signal Strength %f db", ssid.c_str(), signalStrengthOut);
            if (signalStrengthOut == 0.0f)
            {
                quality = Exchange::INetworkManager::WIFI_SIGNAL_DISCONNECTED;
                strengthOut = "0.00";
            }
            else if (signalStrengthOut >= signalStrengthThresholdExcellent && signalStrengthOut < 0)
            {
                quality = Exchange::INetworkManager::WIFI_SIGNAL_EXCELLENT;
            }
            else if (signalStrengthOut >= signalStrengthThresholdGood && signalStrengthOut < signalStrengthThresholdExcellent)
            {
                quality = Exchange::INetworkManager::WIFI_SIGNAL_GOOD;
            }
            else if (signalStrengthOut >= signalStrengthThresholdFair && signalStrengthOut < signalStrengthThresholdGood)
            {
                quality = Exchange::INetworkManager::WIFI_SIGNAL_FAIR;
            }
            else
            {
                quality = Exchange::INetworkManager::WIFI_SIGNAL_WEAK;
            };
        }

        void WiFiSignalStrengthMonitor::startWiFiSignalStrengthMonitor(int interval)
        {
            stopThread = false;
            if (isRunning) {
                NMLOG_INFO("WiFiSignalStrengthMonitor Thread is already running.");
                return;
            }
            isRunning = true;
            monitorThread = std::thread(&WiFiSignalStrengthMonitor::monitorThreadFunction, this, interval);
            monitorThread.detach();
        }

        void WiFiSignalStrengthMonitor::monitorThreadFunction(int interval)
        {
            static Exchange::INetworkManager::WiFiSignalQuality oldSignalQuality = Exchange::INetworkManager::WIFI_SIGNAL_DISCONNECTED;
            NMLOG_INFO("WiFiSignalStrengthMonitor thread started ! (%d)", interval);
            while (!stopThread)
            {
                string ssid = "";
                string signalStrength;
                Exchange::INetworkManager::WiFiSignalQuality newSignalQuality;
                if (_instance != nullptr)
                {
                    NMLOG_DEBUG("checking WiFi signal strength");
                    getSignalData(ssid, newSignalQuality, signalStrength);
                    if(oldSignalQuality != newSignalQuality)
                    {
                        NMLOG_INFO("Notifying WiFiSignalStrengthChangedEvent %s", signalStrength.c_str());
                        oldSignalQuality = newSignalQuality;
                        _instance->ReportWiFiSignalStrengthChange(ssid, signalStrength, newSignalQuality);
                    }

                    if(newSignalQuality == Exchange::INetworkManager::WIFI_SIGNAL_DISCONNECTED)
                    {
                        NMLOG_WARNING("WiFiSignalStrengthChanged to disconnect - WiFiSignalStrengthMonitor exiting");
                        stopThread= false;
                        break; // Let the thread exit naturally
                    }
                }
                else
                    NMLOG_FATAL("NetworkManagerImplementation pointer error !");
                // Wait for the specified interval or until notified to stop
                std::this_thread::sleep_for(std::chrono::seconds(interval));
            }
            isRunning = false;
        }
    }
}

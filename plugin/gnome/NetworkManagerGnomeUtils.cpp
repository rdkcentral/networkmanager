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
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <thread>
#include <string>
#include <map>
#include <NetworkManager.h>
#include <libnm/NetworkManager.h>
#include "Module.h"

#include "NetworkManagerLogger.h"
#include "NetworkManagerGnomeUtils.h"
#include "NetworkManagerImplementation.h"
#include "INetworkManager.h"

namespace WPEFramework
{
    namespace Plugin
    {
        static std::string m_ethifname = "eth0";
        static std::string m_wlanifname = "wlan0";
        static std::string m_deviceHostname = "rdk-device"; // Device name can be empty if not set in /etc/device.properties

        const char* nmUtils::wlanIface() {return m_wlanifname.c_str();}
        const char* nmUtils::ethIface() {return m_ethifname.c_str();}
        const char* nmUtils::deviceHostname() {return m_deviceHostname.c_str();}

        uint8_t nmUtils::wifiSecurityModeFromAp(const std::string& ssid, guint32 flags, guint32 wpaFlags, guint32 rsnFlags, bool doPrint)
        {
            uint8_t security = Exchange::INetworkManager::WIFI_SECURITY_NONE;
            if(!doPrint)
                NMLOG_DEBUG("ap [%s] security str %s", ssid.c_str(), nmUtils::getSecurityModeString(flags, wpaFlags, rsnFlags).c_str());
            else
                NMLOG_INFO("ap [%s] security str %s", ssid.c_str(), nmUtils::getSecurityModeString(flags, wpaFlags, rsnFlags).c_str());

            if ((flags != NM_802_11_AP_FLAGS_PRIVACY) && (wpaFlags == NM_802_11_AP_SEC_NONE) && (rsnFlags == NM_802_11_AP_SEC_NONE)) // Open network
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_NONE;
            else if((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK) && (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_SAE)) // WPA2/WPA3 Transition
            {
                /*
                 * NOTE: In WPA2/WPA3 transition mode, we're explicitly setting key-mgmt=sae instead of 
                 * using key-mgmt=wpa-psk which would be NetworkManager's recommended approach.
                 * 
                 * During testing with key-mgmt=wpa-psk, devices only connected using WPA2-PSK 
                 * even when WPA3-SAE was available. Setting key-mgmt=sae forces the use of WPA3-SAE.
                 * 
                 * Drawback: If an AP reverts to WPA2-only mode, connections will fail until manually changed.
                 *  https://github.com/NetworkManager/NetworkManager/blob/1.47.3-dev/src/core/supplicant/nm-supplicant-config.c#L904
                 */
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE;
            }
            else if (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_SAE)  // Pure WPA3 (SAE only): WPA3-Personal
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE;
            else if (wpaFlags & NM_802_11_AP_SEC_KEY_MGMT_802_1X || rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_802_1X) // WPA2/WPA3 Enterprise: EAP present in either WPA or RSN
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_EAP;
            else
                security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK; // WPA2-PSK

            return security;
        }

        // Function to convert percentage (0-100) to dBm string
        const char* nmUtils::convertPercentageToSignalStrengtStr(int percentage) {

            if (percentage <= 0 || percentage > 100) {
                return "";
            }

           /*
            * -30 dBm to -50 dBm: Excellent signal strength.
            * -50 dBm to -60 dBm: Very good signal strength.
            * -60 dBm to -70 dBm: Good signal strength; acceptable for basic internet browsing.
            * -70 dBm to -80 dBm: Weak signal; performance may degrade, slower speeds, and possible dropouts.
            * -80 dBm to -90 dBm: Very poor signal; likely unusable or highly unreliable.
            *  Below -90 dBm: Disconnected or too weak to establish a stable connection.
            */

            // dBm range: -30 dBm (strong) to -90 dBm (weak)
            const int max_dBm = -30;
            const int min_dBm = -90;
            int dBm_value = max_dBm + ((min_dBm - max_dBm) * (100 - percentage)) / 100;
            static char result[8]={0};
            snprintf(result, sizeof(result), "%d", dBm_value);
            return result;
        }

        std::string nmUtils::getSecurityModeString(guint32 flag, guint32 wpaFlags, guint32 rsnFlags)
        {
            std::string securityStr = "[AP type: ";
            if (flag == NM_802_11_AP_FLAGS_NONE)
                securityStr += "NONE ";
            else
            {
                if ((flag & NM_802_11_AP_FLAGS_PRIVACY) != 0)
                    securityStr += "PRIVACY ";
                if ((flag & NM_802_11_AP_FLAGS_WPS) != 0)
                    securityStr += "WPS ";
                if ((flag & NM_802_11_AP_FLAGS_WPS_PBC) != 0)
                    securityStr += "WPS_PBC ";
                if ((flag & NM_802_11_AP_FLAGS_WPS_PIN) != 0)
                    securityStr += "WPS_PIN ";
            }
            securityStr += "] ";

            if (!(flag & NM_802_11_AP_FLAGS_PRIVACY) && (wpaFlags != NM_802_11_AP_SEC_NONE) && (rsnFlags != NM_802_11_AP_SEC_NONE))
                securityStr += ("Encrypted: ");

            if ((flag & NM_802_11_AP_FLAGS_PRIVACY) && (wpaFlags == NM_802_11_AP_SEC_NONE)
                && (rsnFlags == NM_802_11_AP_SEC_NONE))
                securityStr += ("WEP ");
            if (wpaFlags != NM_802_11_AP_SEC_NONE)
                securityStr += ("WPA ");
            if ((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
                || (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)) {
                securityStr += ("WPA2 ");
            }
            if (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_SAE) {
                securityStr += ("WPA3 ");
            }
            if ((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_OWE)
                || (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM)) {
                securityStr += ("OWE ");
            }
            if ((wpaFlags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
                || (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)) {
                securityStr += ("802.1X ");
            }

            if (securityStr.empty())
            {
                securityStr = "None";
                return securityStr;
            }

            uint32_t flags[2] = { wpaFlags, rsnFlags };
            securityStr += "[WPA: ";
            
            for (int i = 0; i < 2; ++i)
            {
                if (flags[i] & NM_802_11_AP_SEC_PAIR_WEP40)
                    securityStr += "pair_wep40 ";
                if (flags[i] & NM_802_11_AP_SEC_PAIR_WEP104)
                    securityStr += "pair_wep104 ";
                if (flags[i] & NM_802_11_AP_SEC_PAIR_TKIP)
                    securityStr += "pair_tkip ";
                if (flags[i] & NM_802_11_AP_SEC_PAIR_CCMP)
                    securityStr += "pair_ccmp ";
                if (flags[i] & NM_802_11_AP_SEC_GROUP_WEP40)
                    securityStr += "group_wep40 ";
                if (flags[i] & NM_802_11_AP_SEC_GROUP_WEP104)
                    securityStr += "group_wep104 ";
                if (flags[i] & NM_802_11_AP_SEC_GROUP_TKIP)
                    securityStr += "group_tkip ";
                if (flags[i] & NM_802_11_AP_SEC_GROUP_CCMP)
                    securityStr += "group_ccmp ";
                if (flags[i] & NM_802_11_AP_SEC_KEY_MGMT_PSK)
                    securityStr += "psk ";
                if (flags[i] & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
                    securityStr += "802.1X ";
                if (flags[i] & NM_802_11_AP_SEC_KEY_MGMT_SAE)
                    securityStr += "sae ";
                if (flags[i] & NM_802_11_AP_SEC_KEY_MGMT_OWE)
                    securityStr += "owe ";
                if (flags[i] & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM)
                    securityStr += "owe_transition_mode ";
                if (flags[i] & NM_802_11_AP_SEC_KEY_MGMT_EAP_SUITE_B_192)
                    securityStr += "wpa-eap-suite-b-192 ";
                
                if (i == 0) {
                    securityStr += "] [RSN: ";
                }
            }
            securityStr +="] ";
            return securityStr;
        }

       std::string nmUtils::wifiFrequencyFromAp(guint32 apFreq)
       {
            std::string freq;
            if (apFreq >= 2400 && apFreq < 5000)
                freq = "2.4";
            else if (apFreq >= 5000 && apFreq < 6000)
                freq = "5";
            else if (apFreq >= 6000)
                freq = "6";
            else
                freq = "Not available";

            return freq;
       }

       bool nmUtils::caseInsensitiveCompare(const std::string& str1, const std::string& str2)
       {
            std::string upperStr1 = str1;
            std::string upperStr2 = str2;

            // Convert both strings to uppercase
            std::transform(upperStr1.begin(), upperStr1.end(), upperStr1.begin(), ::toupper);
            std::transform(upperStr2.begin(), upperStr2.end(), upperStr2.begin(), ::toupper);

            return upperStr1 == upperStr2;
        }

        bool nmUtils::getDeviceProperties()
        {
            std::string line;
            std::string wifiIfname;
            std::string ethIfname; // cached interface name
            std::string deviceHostname;

            std::ifstream file("/etc/device.properties");
            if (!file.is_open()) {
                NMLOG_FATAL("/etc/device.properties opening file Error");
                return false;
            }

            while (std::getline(file, line))
            {
                if (line.empty()) {
                    continue;
                }
                if (line.find("ETHERNET_INTERFACE=") != std::string::npos) {
                    ethIfname = line.substr(line.find('=') + 1);
                    ethIfname.erase(ethIfname.find_last_not_of("\r\n\t") + 1);
                    ethIfname.erase(0, ethIfname.find_first_not_of("\r\n\t"));
                    if(ethIfname.empty())
                    {
                        NMLOG_WARNING("ETHERNET_INTERFACE is empty in /etc/device.properties");
                        ethIfname = "eth0_missing"; // means device doesnot have ethernet interface invalid name
                    }
                }

                if (line.find("WIFI_INTERFACE=") != std::string::npos) {
                    wifiIfname = line.substr(line.find('=') + 1);
                    wifiIfname.erase(wifiIfname.find_last_not_of("\r\n\t") + 1);
                    wifiIfname.erase(0, wifiIfname.find_first_not_of("\r\n\t"));
                    if(wifiIfname.empty())
                    {
                        NMLOG_WARNING("WIFI_INTERFACE is empty in /etc/device.properties");
                        wifiIfname = "wlan0_missing"; // means device doesnot have wifi interface
                    }
                }

                if (line.find("DEVICE_NAME=") != std::string::npos) {
                    deviceHostname = line.substr(line.find('=') + 1);
                    deviceHostname.erase(deviceHostname.find_last_not_of("\r\n\t") + 1);
                    deviceHostname.erase(0, deviceHostname.find_first_not_of("\r\n\t"));
                    if(deviceHostname.empty())
                    {
                        NMLOG_WARNING("DEVICE_NAME is empty in /etc/device.properties");
                        deviceHostname = ""; // set empty device name
                    }
                }
            }
            file.close();

            m_wlanifname = wifiIfname;
            m_ethifname = ethIfname;
            m_deviceHostname = deviceHostname;
            NMLOG_INFO("/etc/device.properties eth: %s, wlan: %s, device name: %s", m_ethifname.c_str(), m_wlanifname.c_str(), m_deviceHostname.c_str());
            return true;
        }

        bool nmUtils::setNetworkManagerlogLevelToTrace()
        {
            /* set networkmanager daemon log level based on current plugin log level */
            const char* command = "nmcli general logging level TRACE domains ALL";

            // Execute the command using popen
            FILE *pipe = popen(command, "r");

            if (pipe == NULL) {
                NMLOG_ERROR("popen failed %s ", command);
                return false;
            }

            // Close the pipe and retrieve the command's exit status
            int status = pclose(pipe);
            if (status == -1) {
                perror("pclose failed");
                return false;
            }

            // Extract the exit status from the status code
            int exitCode = WEXITSTATUS(status);
            if (exitCode == 0)
                NMLOG_INFO("NetworkManager daemon log level changed to trace");
            else
                NMLOG_INFO(" '%s' failed with exit code %d.", command, exitCode);

            return exitCode == 0;
        }

        void nmUtils::setMarkerFile(const char* filename, bool unmark)
        {
            char fqn[64];
            snprintf(fqn, sizeof(fqn), "%s", filename);
            if (unmark)
            {
                 if (remove(fqn) != 0) {
                    NMLOG_ERROR("Failed to remove marker file");
                }
            }
            else
            {
                FILE *fp = fopen(fqn, "w");
                if (fp != NULL) {
                    fclose(fp);
                } else {
                    NMLOG_ERROR("Failed to open marker file %s", fqn);
                }
            }
        }

        

        bool nmUtils::isInterfaceEnabled(const std::string& interface)
        {
            std::string markerFile;
            if (interface == nmUtils::ethIface()) {
                markerFile = EthernetDisableMarker;
            }
            else if (interface == nmUtils::wlanIface()) {
                markerFile = WiFiDisableMarker;
            }
            else {
                NMLOG_ERROR("Invalid interface name: %s", interface.c_str());
                return false;
            }

            bool isAllowed = (access(markerFile.c_str(), F_OK) != 0);
            NMLOG_DEBUG("isInterfaceEnabled %s: %s", interface.c_str(), isAllowed ? "true" : "false");
            return isAllowed;
        }

        bool nmUtils::writePersistentHostname(const std::string& hostname)
        {
            if (hostname.empty()) {
                NMLOG_ERROR("Cannot write empty hostname to persistent storage");
                return false;
            }

            std::ofstream file(HostnameFile);
            if (!file.is_open()) {
                NMLOG_ERROR("Failed to open %s for writing", HostnameFile);
                return false;
            }

            file << hostname;
            bool success = !file.fail();
            file.close();

            if (success)
                NMLOG_DEBUG("Successfully wrote hostname '%s' to %s",hostname.c_str(), HostnameFile);
            else
                NMLOG_ERROR("Error writing hostname to %s", HostnameFile);

            return success;
        }

        bool nmUtils::readPersistentHostname(std::string& hostname)
        {
            std::ifstream file(HostnameFile);
            if (!file.is_open()) {
                NMLOG_DEBUG("Could not open %s - file may not exist yet", HostnameFile);
                hostname = "";
                return false;
            }

            std::string line;
            if (std::getline(file, line)) {
                // Remove any whitespace, newlines, etc.
                line.erase(line.find_last_not_of("\r\n\t") + 1);
                line.erase(0, line.find_first_not_of("\r\n\t"));
                hostname = line;
                file.close();

                NMLOG_INFO("Read persistent hostname: '%s'", hostname.c_str());
                return true;
            }

            NMLOG_WARNING("Persistent hostname file exists but is empty");
            hostname = "";
            file.close();
            return false;
        }

    }   // Plugin
}   // WPEFramework

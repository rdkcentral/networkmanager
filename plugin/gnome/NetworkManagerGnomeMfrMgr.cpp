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

#include "NetworkManagerGnomeMfrMgr.h"
#include "NetworkManagerLogger.h"
#include "NetworkManagerImplementation.h"
#include "INetworkManager.h"
#include "mfrMgr.h"
#include <thread>
#include <mutex>

#include "libIBus.h"

namespace WPEFramework
{
    namespace Plugin
    {
        extern NetworkManagerImplementation* _instance;

        static std::mutex iarm_mutex;
        static bool iarm_initialized = false;

        static bool ensureIARMConnection()
        {
            std::lock_guard<std::mutex> lock(iarm_mutex);
            
            if (!iarm_initialized) {
                IARM_Result_t ret = IARM_Bus_Init("NetworkMfrMgr");
                if (ret != IARM_RESULT_SUCCESS) {
                    NMLOG_ERROR("IARM_Bus_Init failed: %d", ret);
                    return false;
                }
                
                ret = IARM_Bus_Connect();
                if (ret != IARM_RESULT_SUCCESS) {
                    NMLOG_ERROR("IARM_Bus_Connect failed: %d", ret);
                    return false;
                }
                
                iarm_initialized = true;
                NMLOG_DEBUG("IARM Bus connection established for MfrMgr");
            }
            return true;
        }

        // Helper function to retrieve current WiFi connection details using nmcli and connection files
        static bool getCurrentWiFiConnectionDetails(std::string& ssid, std::string& passphrase, int& security)
        {
            // Use nmcli to get current WiFi connection info
            FILE* pipe = popen("nmcli device wifi show-password 2>/dev/null", "r");
            if (!pipe) {
                NMLOG_ERROR("Failed to execute nmcli command");
                return false;
            }

            char buffer[1024];
            std::string nmcliOutput;
            
            // Read nmcli output
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                nmcliOutput += buffer;
            }
            
            int exitCode = pclose(pipe);
            if (exitCode != 0) {
                NMLOG_ERROR("nmcli command failed with exit code: %d", exitCode);
                return false;
            }

            // Parse SSID from nmcli output
            size_t ssidPos = nmcliOutput.find("SSID:");
            if (ssidPos == std::string::npos) {
                NMLOG_ERROR("SSID not found in nmcli output");
                return false;
            }
            
            size_t ssidStart = nmcliOutput.find_first_not_of(" \t", ssidPos + 5);
            size_t ssidEnd = nmcliOutput.find_first_of("\n\r", ssidStart);
            if (ssidStart == std::string::npos || ssidEnd == std::string::npos) {
                NMLOG_ERROR("Failed to parse SSID from nmcli output");
                return false;
            }
            
            ssid = nmcliOutput.substr(ssidStart, ssidEnd - ssidStart);
            NMLOG_DEBUG("Parsed SSID from nmcli: %s", ssid.c_str());

            // Construct connection file path
            std::string connectionFile = "/etc/NetworkManager/system-connections/" + ssid + ".nmconnection";

            // Try to open the connection file
            std::ifstream file(connectionFile);
            if (!file.is_open()) {
                NMLOG_ERROR("Failed to open NetworkManager connection file: %s", connectionFile.c_str());
                return false;
            }

            // Read entire file into string buffer - more efficient than line-by-line
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            // Find [wifi-security] section using string search
            size_t sectionStart = content.find("[wifi-security]");
            if (sectionStart == std::string::npos) {
                NMLOG_DEBUG("No [wifi-security] section found in connection file");
                file.close();
                return false;
            }
            
            // Find the end of the wifi-security section (next section or end of file)
            size_t sectionEnd = content.find("\n[", sectionStart + 15); // 15 = length of "[wifi-security]"
            if (sectionEnd == std::string::npos) {
                sectionEnd = content.length();
            }

            // Extract just the wifi-security section
            std::string wifiSection = content.substr(sectionStart, sectionEnd - sectionStart);

            std::string keyMgmt;

            // Use regex or direct string search for key-mgmt
            size_t keyMgmtPos = wifiSection.find("key-mgmt=");
            if (keyMgmtPos != std::string::npos) {
                size_t valueStart = keyMgmtPos + 9; // Skip "key-mgmt="
                size_t valueEnd = wifiSection.find_first_of("\r\n", valueStart);
                if (valueEnd == std::string::npos) valueEnd = wifiSection.length();
                keyMgmt = wifiSection.substr(valueStart, valueEnd - valueStart);
                NMLOG_DEBUG("Found key-mgmt: %s", keyMgmt.c_str());
            }

            // Use direct string search for psk
            size_t pskPos = wifiSection.find("psk=");
            if (pskPos != std::string::npos) {
                size_t valueStart = pskPos + 4; // Skip "psk="
                size_t valueEnd = wifiSection.find_first_of("\r\n", valueStart);
                if (valueEnd == std::string::npos) valueEnd = wifiSection.length();
                passphrase = wifiSection.substr(valueStart, valueEnd - valueStart);
                NMLOG_DEBUG("Found psk in connection file");
            }

            file.close();


            // Map key management to security type
            if (keyMgmt.empty()) {
                security = NET_WIFI_SECURITY_NONE;
            } else if (keyMgmt == "none") {
                security = NET_WIFI_SECURITY_NONE;
            } else if (keyMgmt == "wpa-psk") {
                security = NET_WIFI_SECURITY_WPA2_PSK_AES;
            } else if (keyMgmt == "sae") {
                security = NET_WIFI_SECURITY_WPA3_SAE;
            } else {
                // Default to WPA PSK for unknown key management
                security = NET_WIFI_SECURITY_WPA2_PSK_AES;
            }

            NMLOG_DEBUG("Retrieved WiFi connection details - SSID: %s, Security: %d, Key-mgmt: %s", 
                       ssid.c_str(), security, keyMgmt.c_str());
            return true;
        }

        NetworkManagerMfrManager* NetworkManagerMfrManager::getInstance()
        {
            static NetworkManagerMfrManager instance;
            return &instance;
        }

        NetworkManagerMfrManager::NetworkManagerMfrManager()
        {
            // Initialize IARM connection
            if (ensureIARMConnection()) {
                NMLOG_DEBUG("NetworkManagerMfrManager initialized with IARM connection");
            } else {
                NMLOG_ERROR("NetworkManagerMfrManager initialization failed - IARM connection error");
            }
        }

        NetworkManagerMfrManager::~NetworkManagerMfrManager()
        {
            std::lock_guard<std::mutex> lock(iarm_mutex);
            if (iarm_initialized) {
                iarm_initialized = false;
                NMLOG_DEBUG("IARM Bus connection terminated");
            }
            NMLOG_DEBUG("NetworkManagerMfrManager destroyed");
        }

        bool NetworkManagerMfrManager::saveWiFiSettingsToMfr()
        {
            // Launch asynchronous operation to save current WiFi connection details
            std::thread saveThread([this]() {
                std::string ssid, passphrase;
                int security;
                
                if (!getCurrentWiFiConnectionDetails(ssid, passphrase, security)) {
                    NMLOG_ERROR("Failed to retrieve current WiFi connection details for MfrMgr save");
                    return;
                }
                
                NMLOG_INFO("Retrieved current WiFi connection details for MfrMgr save - SSID: %s, Security: %d", ssid.c_str(), security);
                
                // Save the retrieved details synchronously
                bool result = this->saveWiFiSettingsToMfrSync(ssid, passphrase, security);
                if (result) {
                    NMLOG_DEBUG("Background WiFi connection details retrieval and save completed successfully for SSID: %s", ssid.c_str());
                } else {
                    NMLOG_ERROR("Background WiFi connection details retrieval and save failed for SSID: %s", ssid.c_str());
                }
            });
            
            // Detach the thread to run independently
            saveThread.detach();
            
            NMLOG_DEBUG("WiFi connection details retrieval and save operation queued for background execution");
            return true; // Return immediately, actual retrieval and save happens asynchronously
        }

        bool NetworkManagerMfrManager::saveWiFiSettingsToMfrSync(const std::string& ssid, const std::string& passphrase, int security)
        {
            int securityMode;
            if (!ensureIARMConnection()) {
                NMLOG_ERROR("IARM connection not available for saving WiFi settings");
                return false;
            }
            
            NMLOG_INFO("Saving WiFi settings to MfrMgr via IARM - SSID: %s, Security: %d", ssid.c_str(), security);
            
            IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t param{0};
            IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t setParam{0};
            IARM_Result_t ret;
            param.requestType = WIFI_GET_CREDENTIALS;
            ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_Credentials,
                                              (void*)&param, sizeof(param));
            if(security == Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_NONE)
                securityMode = NET_WIFI_SECURITY_NONE;
            else if(security == Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE)
                securityMode = NET_WIFI_SECURITY_WPA3_SAE;
            else
                securityMode = NET_WIFI_SECURITY_WPA2_PSK_AES;
            if (ret == IARM_RESULT_SUCCESS)
            {
                if ((strcmp (param.wifiCredentials.cSSID, ssid.c_str()) == 0) &&
                (strcmp (param.wifiCredentials.cPassword, passphrase.c_str()) == 0) &&
                (param.wifiCredentials.iSecurityMode == securityMode))
                {
                    NMLOG_INFO("Same ssid info not storing it stored ssid %s new ssid %s", param.wifiCredentials.cSSID, ssid.c_str());
                    return true;
                }
                else
                {
                    NMLOG_INFO("ssid info is different continue to store ssid %s new ssid %s", param.wifiCredentials.cSSID, ssid.c_str());
                }
            }

            // Copy SSID
            strncpy(setParam.wifiCredentials.cSSID, ssid.c_str(), sizeof(setParam.wifiCredentials.cSSID));

            // Copy passphrase - only if not empty
            if (!passphrase.empty()) {
                strncpy(setParam.wifiCredentials.cPassword, passphrase.c_str(), sizeof(setParam.wifiCredentials.cPassword));
                NMLOG_DEBUG("WiFi passphrase set for MfrMgr save");
            } else {
                param.wifiCredentials.cPassword[0] = '\0'; // Ensure empty string
                NMLOG_DEBUG("Empty passphrase - setting empty string in MfrMgr");
            }

            setParam.wifiCredentials.iSecurityMode = securityMode;
           
            setParam.requestType = WIFI_SET_CREDENTIALS;
            NMLOG_INFO(" Set Params param.requestType = %d, param.wifiCredentials.cSSID = %s, param.wifiCredentials.iSecurityMode = %d", setParam.requestType, setParam.wifiCredentials.cSSID, setParam.wifiCredentials.iSecurityMode);
            // Make IARM Bus call to save credentials
            ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_Credentials,
                                              (void*)&setParam, sizeof(setParam));
            if(ret == IARM_RESULT_SUCCESS)
            {
                memset(&param,0,sizeof(param));
                param.requestType = WIFI_GET_CREDENTIALS;
                ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_Credentials, (void*)&param, sizeof(param));
                if(security == Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_NONE)
                    securityMode = NET_WIFI_SECURITY_NONE;
                else if(security == Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_SAE)
                    securityMode = NET_WIFI_SECURITY_WPA3_SAE;
                else
                    securityMode = NET_WIFI_SECURITY_WPA2_PSK_AES;

                if (ret == IARM_RESULT_SUCCESS)
                {
                    if ((strcmp (param.wifiCredentials.cSSID, ssid.c_str()) == 0) &&
                            (strcmp (param.wifiCredentials.cPassword, passphrase.c_str()) == 0) &&
                            (param.wifiCredentials.iSecurityMode == securityMode))
                    {
                        NMLOG_INFO("Successfully stored the credentails and verified stored ssid %s current ssid %s and security_mode %d", param.wifiCredentials.cSSID, ssid.c_str(), param.wifiCredentials.iSecurityMode);
                        return true;
                    }
                    else
                    {
                        NMLOG_INFO("failure in storing wifi credentials ssid %s", ssid.c_str());
                    }
                }
            }
            else
            {
                NMLOG_ERROR("IARM Bus call failed for WiFi credentials save: %d", ret);
                return false;
            }
            
            NMLOG_INFO("Successfully saved WiFi settings to MfrMgr via IARM - SSID: %s, Security: %d", ssid.c_str(), security);
            return true;
        }

        bool NetworkManagerMfrManager::clearWiFiSettingsFromMfr()
        {
            if (!ensureIARMConnection()) {
                NMLOG_ERROR("IARM connection not available for clearing WiFi settings");
                return false;
            }
            
            NMLOG_DEBUG("Clearing WiFi settings from MfrMgr via IARM");
            
            // Make IARM Bus call to clear credentials
            IARM_Result_t ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME,IARM_BUS_MFRLIB_API_WIFI_EraseAllData,0,0);
            if (ret != IARM_RESULT_SUCCESS) {
                NMLOG_ERROR("IARM Bus call failed for WiFi credentials clear: %d", ret);
                return false;
            } 
            
            NMLOG_INFO("Successfully cleared WiFi settings from MfrMgr via IARM");
            return true;
        }

    } // namespace Plugin
} // namespace WPEFramework

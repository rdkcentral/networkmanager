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
            std::string nmcli_output;
            
            // Read nmcli output
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                nmcli_output += buffer;
            }
            
            int exit_code = pclose(pipe);
            if (exit_code != 0) {
                NMLOG_ERROR("nmcli command failed with exit code: %d", exit_code);
                return false;
            }

            // Parse SSID from nmcli output
            size_t ssid_pos = nmcli_output.find("SSID:");
            if (ssid_pos == std::string::npos) {
                NMLOG_ERROR("SSID not found in nmcli output");
                return false;
            }
            
            size_t ssid_start = nmcli_output.find_first_not_of(" \t", ssid_pos + 5);
            size_t ssid_end = nmcli_output.find_first_of("\n\r", ssid_start);
            if (ssid_start == std::string::npos || ssid_end == std::string::npos) {
                NMLOG_ERROR("Failed to parse SSID from nmcli output");
                return false;
            }
            
            ssid = nmcli_output.substr(ssid_start, ssid_end - ssid_start);
            NMLOG_DEBUG("Parsed SSID from nmcli: %s", ssid.c_str());

            // Construct connection file path
            std::string connection_file = "/etc/NetworkManager/system-connections/" + ssid + ".nmconnection";
            
            // Try to open the connection file
            std::ifstream file(connection_file);
            if (!file.is_open()) {
                NMLOG_ERROR("Failed to open NetworkManager connection file: %s", connection_file.c_str());
                return false;
            }

            std::string line;
            std::string key_mgmt;
            bool in_wifi_security_section = false;
            
            // Parse the connection file
            while (std::getline(file, line)) {
                // Remove leading/trailing whitespace
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                
                // Check for wifi-security section
                if (line == "[wifi-security]") {
                    in_wifi_security_section = true;
                    continue;
                } else if (line.empty() || line[0] == '[') {
                    in_wifi_security_section = false;
                    continue;
                }
                
                if (in_wifi_security_section) {
                    // Parse key-mgmt
                    if (line.find("key-mgmt=") == 0) {
                        key_mgmt = line.substr(9); // Remove "key-mgmt="
                        NMLOG_DEBUG("Found key-mgmt: %s", key_mgmt.c_str());
                    }
                    // Parse psk
                    else if (line.find("psk=") == 0) {
                        passphrase = line.substr(4); // Remove "psk="
                        NMLOG_DEBUG("Found psk in connection file");
                    }
                }
            }
            
            file.close();

            // Map key management to security type
            if (key_mgmt.empty()) {
                security = NET_WIFI_SECURITY_NONE;
            } else if (key_mgmt == "none") {
                security = NET_WIFI_SECURITY_NONE;
                passphrase.clear();
            } else if (key_mgmt == "wpa-psk") {
                security = NET_WIFI_SECURITY_WPA2_PSK_AES;
            } else if (key_mgmt == "sae") {
                security = NET_WIFI_SECURITY_WPA3_SAE; // WPA3 SAE maps to WPA_PSK for our enum
            } else {
                // Default to WPA PSK for unknown key management
                security = NET_WIFI_SECURITY_WPA2_PSK_AES;
            }

            NMLOG_DEBUG("Retrieved WiFi connection details - SSID: %s, Security: %d, Key-mgmt: %s", 
                       ssid.c_str(), security, key_mgmt.c_str());
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
            // Launch asynchronous operation to retrieve and save current WiFi connection details
            std::thread saveThread([this]() {
                // Retrieve current WiFi connection details from NetworkManager
                std::string ssid, passphrase;
                int security;
                
                if (!getCurrentWiFiConnectionDetails(ssid, passphrase, security)) {
                    NMLOG_ERROR("Failed to retrieve current WiFi connection details for MfrMgr save");
                    return;
                }
                
                NMLOG_INFO("Retrieved current WiFi connection details for MfrMgr save - SSID: %s, Security: %d", ssid.c_str(), security);
                
                // Save the retrieved details synchronously (since we're already in a background thread)
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

#if 0
        bool NetworkManagerMfrManager::saveWiFiSettingsToMfr(const std::string& ssid, const std::string& passphrase, int security)
        {
            std::thread saveThread([this,ssid, passphrase, security]() {
                bool result = this->saveWiFiSettingsToMfrSync(ssid, passphrase, security);
                if (result) {
                    NMLOG_DEBUG("Background MfrMgr save completed successfully for SSID: %s", ssid.c_str());
                } else {
                    NMLOG_ERROR("Background MfrMgr save failed for SSID: %s", ssid.c_str());
                }
            });
            
            saveThread.detach();
            
            NMLOG_DEBUG("WiFi settings save operation queued for background execution - SSID: %s, Security: %d", ssid.c_str(), security);
            return true;
        }
#endif

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
            NMLOG_INFO(" Set Params param.requestType = %d, param.wifiCredentials.cSSID = %s, param.wifiCredentials.cPassword = %s, param.wifiCredentials.iSecurityMode = %d", setParam.requestType, setParam.wifiCredentials.cSSID, setParam.wifiCredentials.cPassword, setParam.wifiCredentials.iSecurityMode); 
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

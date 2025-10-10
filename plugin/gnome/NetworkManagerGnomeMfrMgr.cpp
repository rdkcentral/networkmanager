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

        NetworkManagerMfrManager* NetworkManagerMfrManager::getInstance()
        {
            static NetworkManagerMfrManager instance;
            return &instance;
        }

        NetworkManagerMfrManager::NetworkManagerMfrManager()
        {
            // Initialize pending credentials structure
            clearPendingCredentials();
            
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

        bool NetworkManagerMfrManager::saveWiFiSettingsToMfrSync(const std::string& ssid, const std::string& passphrase, int security)
        {
            int securityMode;
            if (!ensureIARMConnection()) {
                NMLOG_ERROR("IARM connection not available for saving WiFi settings");
                return false;
            }
            
            NMLOG_INFO("Saving WiFi settings to MfrMgr via IARM - SSID: %s, Security: %d", ssid.c_str(), security);
            
            IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t param{0};
            param.requestType = WIFI_GET_CREDENTIALS;
            IARM_Result_t ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_Credentials,
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

            memset(&param,0,sizeof(param));
            param.requestType = WIFI_SET_CREDENTIALS;

            // Copy SSID
            snprintf(param.wifiCredentials.cSSID, sizeof(param.wifiCredentials.cSSID), "%s", ssid.c_str());

            // Copy passphrase - only if not empty
            if (!passphrase.empty()) {
                snprintf(param.wifiCredentials.cPassword, sizeof(param.wifiCredentials.cPassword), "%s", passphrase.c_str());
                NMLOG_DEBUG("WiFi passphrase set for MfrMgr save");
            } else {
                param.wifiCredentials.cPassword[0] = '\0'; // Ensure empty string
                NMLOG_DEBUG("Empty passphrase - setting empty string in MfrMgr");
            }

            param.wifiCredentials.iSecurityMode = securityMode;
            
            // Make IARM Bus call to save credentials
            ret = IARM_Bus_Call(IARM_BUS_MFRLIB_NAME, IARM_BUS_MFRLIB_API_WIFI_Credentials,
                                              (void*)&param, sizeof(param));
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

        void NetworkManagerMfrManager::setWiFiCredentials(const std::string& ssid, const std::string& passphrase, int security)
        {
            // Cache credentials during connection process for later saving
            m_pendingCredentials.ssid = ssid;
            m_pendingCredentials.passphrase = passphrase;
            m_pendingCredentials.security = security;
            m_pendingCredentials.isValid = true;
            
            NMLOG_DEBUG("WiFi credentials cached for pending save - SSID: %s, Security: %d", ssid.c_str(), security);
        }

        void NetworkManagerMfrManager::handleWiFiConnectedWithCredentials()
        {
            if (m_pendingCredentials.isValid) {
                NMLOG_INFO("WiFi connected with cached credentials, saving to MfrMgr");
                saveWiFiSettingsToMfr(m_pendingCredentials.ssid, m_pendingCredentials.passphrase, m_pendingCredentials.security);
                
                // Clear cached credentials after saving
                clearPendingCredentials();
            }
        }

        void NetworkManagerMfrManager::clearPendingCredentials()
        {
            m_pendingCredentials.ssid.clear();
            m_pendingCredentials.passphrase.clear();
            m_pendingCredentials.security = 0;
            m_pendingCredentials.isValid = false;
        }

    } // namespace Plugin
} // namespace WPEFramework

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

        const char* nmUtils::wlanIface() {return m_wlanifname.c_str();}
        const char* nmUtils::ethIface() {return m_ethifname.c_str();}

        NMDeviceState nmUtils::ifaceState(NMClient *client, const char* interface)
        {
            NMDeviceState deviceState = NM_DEVICE_STATE_UNKNOWN;
            NMDevice *device = NULL;
            if(client == NULL)
                return deviceState;

            device = nm_client_get_device_by_iface(client, interface);
            if (device == NULL) {
                NMLOG_FATAL("libnm doesn't have device corresponding to %s", interface);
                return deviceState;
            }

            deviceState = nm_device_get_state(device);
            return deviceState;
        }

        uint8_t nmUtils::wifiSecurityModeFromAp(guint32 flags, guint32 wpaFlags, guint32 rsnFlags)
        {
                uint8_t security = Exchange::INetworkManager::WIFI_SECURITY_NONE;
                if ((flags == NM_802_11_AP_FLAGS_NONE) && (wpaFlags == NM_802_11_AP_SEC_NONE) && (rsnFlags == NM_802_11_AP_SEC_NONE))
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_NONE;
                else if( (flags & NM_802_11_AP_FLAGS_PRIVACY) && ((wpaFlags & NM_802_11_AP_SEC_PAIR_WEP40) || (rsnFlags & NM_802_11_AP_SEC_PAIR_WEP40)) )
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WEP_64;
                else if( (flags & NM_802_11_AP_FLAGS_PRIVACY) && ((wpaFlags & NM_802_11_AP_SEC_PAIR_WEP104) || (rsnFlags & NM_802_11_AP_SEC_PAIR_WEP104)) )
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WEP_128;
                else if((wpaFlags & NM_802_11_AP_SEC_PAIR_TKIP) || (rsnFlags & NM_802_11_AP_SEC_PAIR_TKIP))
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK_TKIP;
                else if((wpaFlags & NM_802_11_AP_SEC_PAIR_CCMP) || (rsnFlags & NM_802_11_AP_SEC_PAIR_CCMP))
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_PSK_AES;
                else if ((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK) && (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_802_1X))
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_WPA2_ENTERPRISE;
                else if(rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA_WPA2_PSK;
                else if((wpaFlags & NM_802_11_AP_SEC_GROUP_CCMP) || (rsnFlags & NM_802_11_AP_SEC_GROUP_CCMP))
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_PSK_AES;
                else if((wpaFlags & NM_802_11_AP_SEC_GROUP_TKIP) || (rsnFlags & NM_802_11_AP_SEC_GROUP_TKIP))
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA2_PSK_TKIP;
                else if((rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_OWE) || (rsnFlags & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM))
                    security = Exchange::INetworkManager::WIFISecurityMode::WIFI_SECURITY_WPA3_SAE;
                else
                    NMLOG_WARNING("security mode not defined (flag: %d, wpaFlags: %d, rsnFlags: %d)", flags, wpaFlags, rsnFlags);
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
            std:string freq;
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

       JsonObject nmUtils::apToJsonObject(NMAccessPoint *ap)
       {
            GError *error = NULL;
            GBytes *ssid = NULL;
            int strength = 0;
            std::string freq;
            int security;
            guint32 flags, wpaFlags, rsnFlags, apFreq;
            JsonObject ssidObj;
            if(ap == nullptr)
                return ssidObj;
            ssid = nm_access_point_get_ssid(ap);
            if (ssid)
            {
                char *ssidStr = nullptr;
                ssidStr = nm_utils_ssid_to_utf8((const guint8*)g_bytes_get_data(ssid, NULL), g_bytes_get_size(ssid));
                string ssidString(ssidStr);
                ssidObj["ssid"] = ssidString;
                strength = nm_access_point_get_strength(ap);
                apFreq   = nm_access_point_get_frequency(ap);
                flags    = nm_access_point_get_flags(ap);
                wpaFlags = nm_access_point_get_wpa_flags(ap);
                rsnFlags = nm_access_point_get_rsn_flags(ap);
                freq = nmUtils::wifiFrequencyFromAp(apFreq);
                security = nmUtils::wifiSecurityModeFromAp(flags, wpaFlags, rsnFlags);

                ssidObj["security"] = security;
                ssidObj["strength"] = nmUtils::convertPercentageToSignalStrengtStr(strength);
                ssidObj["frequency"] = freq;
            }
            else
                NMLOG_DEBUG("hidden ssid found, bssid: %s", nm_access_point_get_bssid(ap));

            return ssidObj;
       }

        void nmUtils::printActiveSSIDsOnly(NMDeviceWifi *wifiDevice)
        {
            if(!NM_IS_DEVICE_WIFI(wifiDevice))
            {
                NMLOG_ERROR("Not a wifi object ");
                return;
            }
            const GPtrArray *accessPointsArray = nm_device_wifi_get_access_points(wifiDevice);
            for (guint i = 0; i < accessPointsArray->len; i++)
            {
                NMAccessPoint *ap = NULL;
                GBytes *ssidGByte = NULL;
                std::string ssid;

                ap = (NMAccessPoint*)accessPointsArray->pdata[i];
                ssidGByte = nm_access_point_get_ssid(ap);
                if(ssidGByte)
                {
                    char* ssidStr = NULL;
                    gsize len;
                    const guint8 *ssidData = static_cast<const guint8 *>(g_bytes_get_data(ssidGByte, &len));
                    ssidStr = nm_utils_ssid_to_utf8(ssidData, len);
                    if(ssidStr != NULL) {
                        std::string ssidTmp(ssidStr, len);
                        ssid = ssidTmp;
                    }
                    else
                        ssid = "---";
                }
                else
                    ssid = "---";
            
                NMLOG_INFO("ssid: %s", ssid.c_str());
            }
        }

        bool nmUtils::caseInsensitiveCompare(const std::string& str1, const std::string& str2) {
            std::string upperStr1 = str1;
            std::string upperStr2 = str2;

            // Convert both strings to uppercase
            std::transform(upperStr1.begin(), upperStr1.end(), upperStr1.begin(), ::toupper);
            std::transform(upperStr2.begin(), upperStr2.end(), upperStr2.begin(), ::toupper);

            return upperStr1 == upperStr2;
        }

        bool nmUtils::getInterfacesName()
        {
            std::string line;
            std::string wifiIfname;
            std::string ethIfname; // cached interface name

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
                }

                if (line.find("WIFI_INTERFACE=") != std::string::npos) {
                    wifiIfname = line.substr(line.find('=') + 1);
                    wifiIfname.erase(wifiIfname.find_last_not_of("\r\n\t") + 1);
                    wifiIfname.erase(0, wifiIfname.find_first_not_of("\r\n\t"));
                }
            }
            file.close();
            if (ethIfname.empty() && wifiIfname.empty()) {
                NMLOG_FATAL("Could not find any interface name in /etc/device.properties");
                return false;
            }
            m_wlanifname = wifiIfname;
            m_ethifname = ethIfname;
            NMLOG_INFO("/etc/device.properties eth: %s, wlan: %s", m_ethifname.c_str(), m_wlanifname.c_str());
            return true;
        }
    }   // Plugin
}   // WPEFramework

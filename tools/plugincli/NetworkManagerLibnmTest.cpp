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
#include "NetworkManagerLogger.h"
#include "NetworkManagerGnomeEvents.h"
#include "INetworkManager.h"
#include "NetworkManagerGnomeUtils.h"
#include "NetworkManagerImplementation.h"
#include "NetworkManagerGnomeWIFI.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <list>
#include <string>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using namespace WPEFramework::Exchange;
using namespace std;

namespace WPEFramework
{
   namespace Plugin
    {

        void NetworkManagerImplementation::ReportInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface)
        {
            NMLOG_INFO("calling 'ReportInterfaceStateChange' cb");
        }
        void NetworkManagerImplementation::ReportActiveInterfaceChange(const string prevActiveInterface, const string currentActiveinterface)
        {
            NMLOG_INFO("calling 'ReportActiveInterfaceChange' cb");
        }
        void NetworkManagerImplementation::ReportIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status)
        {
            NMLOG_INFO("calling 'ReportIPAddressChange' cb");
        }
        void NetworkManagerImplementation::ReportInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState)
        {
            NMLOG_INFO("calling 'ReportInternetStatusChange' cb");
        }
        void NetworkManagerImplementation::ReportAvailableSSIDs(const JsonArray &arrayofWiFiScanResults)
        {
            NMLOG_INFO("calling 'ReportAvailableSSIDs' cb");
        }
        void NetworkManagerImplementation::ReportWiFiStateChange(const Exchange::INetworkManager::WiFiState state)
        {
            NMLOG_INFO("calling 'ReportWiFiStateChange' cb");
        }
        void NetworkManagerImplementation::ReportWiFiSignalQualityChange(const string ssid, const string strength, const string noise, const string snr, const Exchange::INetworkManager::WiFiSignalQuality quality)
        {
            NMLOG_INFO("calling 'ReportWiFiSignalQualityChange' cb");
        }

        NetworkManagerImplementation* _instance = nullptr;
    }
}

void displayMenu()
{
    std::cout << "\n--- Network Manager API Test Menu ---" << std::endl;
    std::cout << "1. Get Known SSIDs" << std::endl;
    std::cout << "2. Get Connected SSID" << std::endl;
    std::cout << "3. Add to Known SSIDs" << std::endl;
    std::cout << "4. Remove Known SSIDs" << std::endl;
    std::cout << "5. Start WiFi Scan" << std::endl;
    std::cout << "6. WiFi Connect" << std::endl;
    std::cout << "7. WiFi Disconnect" << std::endl;
    std::cout << "8. Get WiFi State" << std::endl;
    std::cout << "9. start WPS" << std::endl;
    std::cout << "10. stop WPS" << std::endl;
    std::cout << "11. Get WiFi Signal Strength" << std::endl;
    std::cout << "12. GetAvailableInterface" << std::endl;
    std::cout << "13. GetPrimaryInterface" << std::endl;
    std::cout << "14. SetPrimaryInterface" << std::endl;
    std::cout << "15. SetInterfaceState" << std::endl;
    std::cout << "16. GetInterfaceState" << std::endl;
    std::cout << "17. GetIPSettings" << std::endl;
    std::cout << "18. SetIPSettings" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "-------------------------------------" << std::endl;
}

WPEFramework::Exchange::INetworkManager::WIFISecurityMode getSecurityType()
{
    int securityChoice;
    std::cout << "Select Security Type:" << std::endl;
    std::cout << "1. WIFI_SECURITY_NONE" << std::endl;
    std::cout << "2. WIFI_SECURITY_WPA_PSK" << std::endl;
    std::cout << "3. WIFI_SECURITY_SAE" << std::endl;
    std::cout << "4. WIFI_SECURITY_EAP" << std::endl;
    std::cin >> securityChoice;

    switch (securityChoice) {
        case 4:
            return WPEFramework::Exchange::INetworkManager::WIFI_SECURITY_EAP;
        case 3:
            return WPEFramework::Exchange::INetworkManager::WIFI_SECURITY_SAE;
        case 2:
            return WPEFramework::Exchange::INetworkManager::WIFI_SECURITY_WPA_PSK;
        case 1:
            return WPEFramework::Exchange::INetworkManager::WIFI_SECURITY_NONE;
        default:
            std::cout << "Invalid choice. Defaulting to open." << std::endl;
            return WPEFramework::Exchange::INetworkManager::WIFI_SECURITY_NONE;
    }
}

void printSSIDs(const std::list<std::string>& ssids)
{
    if (ssids.empty()) {
        std::cout << "No SSIDs found" << std::endl;
    } else {
        std::cout << "SSIDs:" << std::endl;
        for (const auto& ssid : ssids) {
            std::cout << "- " << ssid << std::endl;
        }
    }
}

int main()
{
    int choice = -1;
    NMLOG_INFO("Starting NetworkManager libnm cli tool");
    NetworkManagerLogger::SetLevel(NetworkManagerLogger::DEBUG_LEVEL);
    GnomeNetworkManagerEvents* nmEvent = GnomeNetworkManagerEvents::getInstance();
    wifiManager *wifiMgr = wifiManager::getInstance();
    nmEvent->startNetworkMangerEventMonitor();
    if (wifiMgr == nullptr) {
        NMLOG_ERROR("Failed to create wifiManager instance");
        return -1;
    }
    NMLOG_INFO("wifiManager instance created successfully");
    while (choice != 0)
    {
        displayMenu();
        std::cout << "Enter your choice: \n";
        std::cin >> choice;
        switch (choice)
        {
            case 0:
                break;
            case 1: {
                std::list<std::string> ssids;
                if (wifiMgr->getKnownSSIDs(ssids)) {
                    printSSIDs(ssids);
                } else {
                    NMLOG_ERROR("Failed to get available SSIDs");
                }
                break;
            }
            case 2: {
                Exchange::INetworkManager::WiFiSSIDInfo ssidinfo;
                if (wifiMgr->wifiConnectedSSIDInfo(ssidinfo))
                    NMLOG_INFO("Connected SSID: %s", ssidinfo.ssid.c_str());
                else
                    NMLOG_ERROR("Failed to get connected SSID");
                break;
            }
            case 3: {
                std::string ssid, passphrase;
                std::cout << "Enter SSID to add : ";
                std::cin.ignore();
                std::getline(std::cin, ssid);
                std::cout << "Enter passphrase: ";
                std::getline(std::cin, passphrase);
                WPEFramework::Exchange::INetworkManager::WIFISecurityMode securityType = getSecurityType();
                Exchange::INetworkManager::WiFiConnectTo ssidinfo = {
                    .ssid = ssid,
                    .passphrase = passphrase,
                    .security = securityType,
                    .persist = true,
                };
                if (wifiMgr->addToKnownSSIDs(ssidinfo)) {
                    NMLOG_INFO("SSID added to known list successfully");
                } else {
                    NMLOG_ERROR("Failed to add SSID to known list");
                }
                break;
            }
            case 4: {
                std::string ssid;
                std::cout << "Enter know ssid to remove ? (leave blank for all): ";
                std::cin.ignore();
                std::getline(std::cin, ssid);
                if (wifiMgr->removeKnownSSID(ssid)) {
                    NMLOG_INFO("SSID removed successfully");
                } else {
                    NMLOG_ERROR("Failed to remove SSID");
                }
                break;
            }
            case 5:
            {
                std::string ssid;
                std::cout << "Enter SSID to scan (leave blank for all): ";

                if (std::cin.peek() == '\n') {
                    std::cin.ignore();
                }

                std::getline(std::cin, ssid);

                nmEvent->setwifiScanOptions(true);  // If this triggers scan customization

                NMLOG_INFO("Sending WiFi scan request%s", ssid.empty() ? " (all SSIDs)" : (" for SSID: " + ssid).c_str());

                if (wifiMgr->wifiScanRequest(ssid)) {
                    NMLOG_INFO("WiFi scan request sent successfully.");
                } else {
                    NMLOG_ERROR("Failed to send WiFi scan request.");
                }
                break;
            }
            case 6:
            {
                std::string ssid, passphrase;
                bool persist;

                std::cout << "Enter SSID to connect: ";
                std::cin.ignore();
                std::getline(std::cin, ssid);

                std::cout << "Enter passphrase: ";
                std::getline(std::cin, passphrase);

                WPEFramework::Exchange::INetworkManager::WIFISecurityMode securityType = getSecurityType();

                std::cout << "Persist SSID info? (1 for yes, 0 for no): ";
                std::cin >> persist;

                Exchange::INetworkManager::WiFiConnectTo ssidinfo = {
                    .ssid = ssid,
                    .passphrase = passphrase,
                    .security = securityType,
                    .persist = persist
                };

                if (wifiMgr->wifiConnect(ssidinfo)) {
                    NMLOG_INFO("Connected to WiFi successfully");
                } else {
                    NMLOG_ERROR("Failed to connect to WiFi");
                }
                break;
            }
            case 7: {
                if (wifiMgr->wifiDisconnect()) {
                    NMLOG_INFO("Disconnected from WiFi successfully");
                } else {
                    NMLOG_ERROR("Failed to disconnect from WiFi");
                }
                break;
            }
            case 8: {
                Exchange::INetworkManager::WiFiState state;
                if (wifiMgr->isWifiConnected()) {
                    NMLOG_INFO("WiFi State: connected");  // Assuming state is an enum or int
                } else {
                    NMLOG_ERROR("WiFi State: disconnected");
                }
                break;
            }
            case 9: {
                if (wifiMgr->startWPS()) {
                    NMLOG_INFO("WPS started successfully");
                } else {
                    NMLOG_ERROR("Failed to start WPS");
                }
                break;
            }
            case 10: {
                if (wifiMgr->stopWPS()) {
                    NMLOG_INFO("WPS started successfully");
                } else {
                    NMLOG_ERROR("Failed to start WPS");
                }
                break;
            }
            case 11: {
                std::string interface;
                std::string input;
                bool enable;
                std::cout << "Enter interface name to change the state: ";
                std::cin.ignore();
                std::getline(std::cin, interface);
                std::cout << "Enable Interface(input true/false): ";
                std::getline(std::cin, input);
                enable = (input == "true")? true: false;

                if(wifiMgr->setInterfaceState(interface, enable)){
                    NMLOG_INFO("setInterfaceState successful");
                }
                else
                {
                    NMLOG_ERROR("Failed to set Interface State");
                }
                break;
            }
            case 12: {
                std::string interface;
                std::string ipVersion;
                std::string input;
                bool autoConfig;
                std::string ipAddress;
                uint32_t prefix = 0;
                std::string gateway;
                std::string primarydns;
                std::string secondarydns;
                Exchange::INetworkManager::IPAddress address;
                std::cout << "Enter interface: ";
                std::cin.ignore();
                std::getline(std::cin, interface);

                std::cout << "Enter IP version: (IPv4/IPv6)";
                std::getline(std::cin, ipVersion);

                std::cout << "Enter auto configuration setting(input true/false): ";
                std::getline(std::cin, input);
                autoConfig = (input == "true")? true: false;

                if(!autoConfig)
                {
                    std::cout << "Enter IP address: ";
                    std::getline(std::cin, ipAddress);

                    // For prefix, use std::cin because it's a uint32_t
                    std::cout << "Enter prefix length: ";
                    std::cin >> prefix;
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore leftover newline

                    std::cout << "Enter gateway: ";
                    std::getline(std::cin, gateway);

                    std::cout << "Enter primary DNS: ";
                    std::getline(std::cin, primarydns);

                    std::cout << "Enter secondary DNS: ";
                    std::getline(std::cin, secondarydns);
                }
                address.ipversion = ipVersion;
                address.autoconfig = autoConfig;
                address.ipaddress = ipAddress;
                address.prefix = prefix;
                address.gateway = gateway;
                address.primarydns = primarydns;
                address.secondarydns = secondarydns;
                if(wifiMgr->setIpSettings(interface, address)){
                    NMLOG_INFO("setIPSettings successful");
                }
                else
                {
                    NMLOG_ERROR("Failed to set IP settings");
                }
                break;
            }
            case 13: {
                std::string interface;
                std::cout << "Enter interface name to set to: ";
                std::cin.ignore();
                std::getline(std::cin, interface);
                if (wifiMgr->setPrimaryInterface(interface)) {
                    NMLOG_INFO("setPrimaryInterface successful");
                } else {
                    NMLOG_ERROR("Failed to setPrimaryInterface");
                }
                break;
            }
            default:
                std::cout << "Invalid choice. Please try again." << std::endl;
                break;
        }
    }
    NMLOG_INFO("cliTool completed successfully");
    nmEvent->stopNetworkMangerEventMonitor();
    return 0;
}

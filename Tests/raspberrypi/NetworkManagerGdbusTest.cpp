#include "NetworkManagerGdbusClient.h"
#include "NetworkManagerGdbusMgr.h"
#include "NetworkManagerLogger.h"
#include "NetworkManagerGdbusEvent.h"
#include "INetworkManager.h"
#include "NetworkManagerGdbusUtils.h"
#include "NetworkManagerImplementation.h"
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
        void NetworkManagerImplementation::ReportAvailableSSIDs(JsonArray &arrayofWiFiScanResults)
        {
            NMLOG_INFO("calling 'ReportAvailableSSIDs' cb");
        }
        void NetworkManagerImplementation::ReportWiFiStateChange(const Exchange::INetworkManager::WiFiState state)
        {
            NMLOG_INFO("calling 'ReportWiFiStateChange' cb");
        }
        void NetworkManagerImplementation::ReportWiFiSignalStrengthChange(const string ssid, const string strength, const Exchange::INetworkManager::WiFiSignalQuality quality)
        {
            NMLOG_INFO("calling 'ReportWiFiSignalStrengthChange' cb");
        }

        NetworkManagerImplementation* _instance = nullptr;
    }
}

void displayMenu()
{
    std::cout << "\n--- Network Manager API Test Menu ---" << std::endl;
    std::cout << "1. Get Known SSIDs" << std::endl;
    std::cout << "2. Get Available SSIDs" << std::endl;
    std::cout << "3. Get Connected SSID" << std::endl;
    std::cout << "4. Add to Known SSIDs" << std::endl;
    std::cout << "5. Remove Known SSIDs" << std::endl;
    std::cout << "6. Start WiFi Scan" << std::endl;
    std::cout << "7. WiFi Connect" << std::endl;
    std::cout << "8. WiFi Disconnect" << std::endl;
    std::cout << "9. Get WiFi State" << std::endl;
    std::cout << "10. Get WiFi Signal Strength" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "-------------------------------------" << std::endl;
}

WPEFramework::Exchange::INetworkManager::WIFISecurityMode getSecurityType()
{
    int securityChoice;
    std::cout << "Select Security Type:" << std::endl;
    std::cout << "1. WPA/WPA2 PSK AES" << std::endl;
    std::cout << "2. WPA/WPA2 PSK TKIP" << std::endl;
    std::cout << "3. Open (No Security)" << std::endl;
    std::cin >> securityChoice;

    switch (securityChoice) {
        case 1:
            return WPEFramework::Exchange::INetworkManager::WIFI_SECURITY_WPA_PSK_AES;
        case 2:
            return WPEFramework::Exchange::INetworkManager::WIFI_SECURITY_WPA_PSK_TKIP;
        case 3:
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

    NetworkManagerClient* nmClient = NetworkManagerClient::getInstance();
    NetworkManagerEvents* nmEvents = NetworkManagerEvents::getInstance();
    int choice = -1;

    while (choice != 0) {
        displayMenu();
        std::cout << "Enter your choice: \n";
        std::cin >> choice;

        switch (choice) {
            case 1: {
                std::list<std::string> ssids;
                if (nmClient->getKnownSSIDs(ssids)) {
                    printSSIDs(ssids);
                } else {
                    NMLOG_ERROR("Failed to get known SSIDs");
                }
                break;
            }

            case 2: {
                std::list<std::string> ssids;
                if (nmClient->getAvailableSSIDs(ssids)) {
                    printSSIDs(ssids);
                } else {
                    NMLOG_ERROR("Failed to get available SSIDs");
                }
                break;
            }

            case 3: {
                Exchange::INetworkManager::WiFiSSIDInfo ssidinfo;
                if (nmClient->getConnectedSSID(ssidinfo)) {
                    NMLOG_INFO("Connected SSID: %s", ssidinfo.ssid.c_str());
                } else {
                    NMLOG_ERROR("Failed to get connected SSID");
                }
                break;
            }

            case 4: {
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
                if (nmClient->addToKnownSSIDs(ssidinfo)) {
                    NMLOG_INFO("SSID added to known list successfully");
                } else {
                    NMLOG_ERROR("Failed to add SSID to known list");
                }
                break;
            }

            case 5: {
                std::string ssid;
                std::cout << "Enter SSID to remove: ";
                std::cin >> ssid;
                if (nmClient->removeKnownSSIDs(ssid)) {
                    NMLOG_INFO("SSID removed successfully");
                } else {
                    NMLOG_ERROR("Failed to remove SSID");
                }
                break;
            }

            case 6: {
                std::string ssid;
                std::cout << "Enter SSID to scan (leave blank for all): ";
                std::cin.ignore();
                std::getline(std::cin, ssid);
                    nmEvents->setwifiScanOptions(true);
                if (nmClient->startWifiScan(ssid)) {
                    NMLOG_INFO("WiFi scan started successfully");
                } else {
                    NMLOG_ERROR("Failed to start WiFi scan");
                }
                break;
            }

            case 7: {
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

                if (nmClient->wifiConnect(ssidinfo)) {
                    NMLOG_INFO("Connected to WiFi successfully");
                } else {
                    NMLOG_ERROR("Failed to connect to WiFi");
                }
                break;
            }

            case 8: {
                if (nmClient->wifiDisconnect()) {
                    NMLOG_INFO("Disconnected from WiFi successfully");
                } else {
                    NMLOG_ERROR("Failed to disconnect from WiFi");
                }
                break;
            }

            case 9: {
                Exchange::INetworkManager::WiFiState state;
                if (nmClient->getWifiState(state)) {
                    NMLOG_INFO("WiFi State: %d", state);  // Assuming state is an enum or int
                } else {
                    NMLOG_ERROR("Failed to get WiFi state");
                }
                break;
            }

            case 10: {
                std::string ssid, signalStrength;
                Exchange::INetworkManager::WiFiSignalQuality quality;
                if (nmClient->getWiFiSignalStrength(ssid, signalStrength, quality)) {
                    NMLOG_INFO("SSID: %s, Signal Strength: %s, Quality: %d", ssid.c_str(), signalStrength.c_str(), quality);
                } else {
                    NMLOG_ERROR("Failed to get WiFi signal strength");
                }
                break;
            }

            case 0:
                std::cout << "Exiting program." << std::endl;
                break;

            default:
                std::cout << "Invalid choice, please try again." << std::endl;
                break;
            }
    }

    NMLOG_INFO("Program completed successfully");
    nmEvents->stopNetworkMangerEventMonitor();
    return 0;
}

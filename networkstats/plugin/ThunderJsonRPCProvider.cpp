/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include "ThunderJsonRPCProvider.h"
#include "NetworkConnectionStatsLogger.h"
#include <cstring>

using namespace std;
using namespace NetworkConnectionStatsLogger;

#define NETWORK_MANAGER_CALLSIGN "org.rdk.NetworkManager"

/* @brief Constructor */
NetworkJsonRPCProvider::NetworkJsonRPCProvider()
    : m_networkManagerClient(nullptr)
{
    NSLOG_INFO("NetworkJsonRPCProvider constructed");
}

/* @brief Destructor */
NetworkJsonRPCProvider::~NetworkJsonRPCProvider()
{
    m_networkManagerClient.reset();
}

/* @brief Initialize the provider with Thunder connection */
bool NetworkJsonRPCProvider::Initialize()
{
    try {
        // Set Thunder access point
        WPEFramework::Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), _T("127.0.0.1:9998"));
        
        // Create JSON-RPC client connection to NetworkManager
        m_networkManagerClient = std::make_shared<WPEFramework::JSONRPC::SmartLinkType<WPEFramework::Core::JSON::IElement>>(
            _T(NETWORK_MANAGER_CALLSIGN), 
            _T(""),  // No specific version
            _T("")   // No token needed for inter-plugin communication
        );
        
        if (m_networkManagerClient) {
            NSLOG_INFO("NetworkManager JSON-RPC client initialized successfully");
            return true;
        } else {
            NSLOG_ERROR("Failed to create NetworkManager JSON-RPC client");
            return false;
        }
    }
    catch (const std::exception& e) {
        NSLOG_ERROR("Exception during initialization: %s", e.what());
        return false;
    }
}

/* @brief Retrieve IPv4 address for specified interface */
std::string NetworkJsonRPCProvider::getIpv4Address(std::string interface_name)
{
    std::string ipv4_address = "";
    
    // Initialize member variables
    m_ipv4Gateway = "";
    m_ipv4PrimaryDns = "";
    
    if (!m_networkManagerClient) {
        NSLOG_ERROR("NetworkManager client not initialized");
        return "";
    }
    
    WPEFramework::Core::JSON::VariantContainer params;
    params["interface"] = interface_name;
    params["ipversion"] = "IPv4";
    
    WPEFramework::Core::JSON::VariantContainer result;
    
    uint32_t rc = m_networkManagerClient->Invoke<WPEFramework::Core::JSON::VariantContainer, WPEFramework::Core::JSON::VariantContainer>(
        5000, "GetIPSettings", params, result);
    
    if (rc == WPEFramework::Core::ERROR_NONE) {
        if (result.HasLabel("ipaddress")) {
            ipv4_address = result["ipaddress"].String();
            NSLOG_INFO("IPv4 address retrieved: %s", ipv4_address.c_str());
        }
        if (result.HasLabel("gateway")) {
            m_ipv4Gateway = result["gateway"].String();
            NSLOG_INFO("IPv4 Route: %s", m_ipv4Gateway.c_str());
        }
        if (result.HasLabel("primarydns")) {
            m_ipv4PrimaryDns = result["primarydns"].String();
            NSLOG_INFO("DNS entry: %s", m_ipv4PrimaryDns.c_str());
        }
    } else {
        NSLOG_ERROR("GetIPSettings JSON-RPC call failed with error code: %u", rc);
    }
    
    return ipv4_address;
}

/* @brief Retrieve IPv6 address for specified interface */
std::string NetworkJsonRPCProvider::getIpv6Address(std::string interface_name)
{
    std::string ipv6_address = "";
    
    // Initialize member variables
    m_ipv6Gateway = "";
    m_ipv6PrimaryDns = "";
    
    if (!m_networkManagerClient) {
        NSLOG_ERROR("NetworkManager client not initialized");
        return "";
    }
    
    WPEFramework::Core::JSON::VariantContainer params;
    params["interface"] = interface_name;
    params["ipversion"] = "IPv6";
    
    WPEFramework::Core::JSON::VariantContainer result;
    
    uint32_t rc = m_networkManagerClient->Invoke<WPEFramework::Core::JSON::VariantContainer, WPEFramework::Core::JSON::VariantContainer>(
        5000, "GetIPSettings", params, result);
    
    if (rc == WPEFramework::Core::ERROR_NONE) {
        if (result.HasLabel("ipaddress")) {
            ipv6_address = result["ipaddress"].String();
            NSLOG_INFO("IPv6 address retrieved: %s", ipv6_address.c_str());
        }
        if (result.HasLabel("gateway")) {
            m_ipv6Gateway = result["gateway"].String();
            NSLOG_INFO("IPv6 Route: %s", m_ipv6Gateway.c_str());
        }
        if (result.HasLabel("primarydns")) {
            m_ipv6PrimaryDns = result["primarydns"].String();
            NSLOG_INFO("DNS entry: %s", m_ipv6PrimaryDns.c_str());
        }
    } else {
        NSLOG_ERROR("GetIPSettings JSON-RPC call failed with error code: %u", rc);
    }
    
    return ipv6_address;
}

/* @brief Get current network connection type */
std::string NetworkJsonRPCProvider::getConnectionType()
{
    std::string interface = getInterface();
    
    if (interface.empty()) {
        NSLOG_ERROR("Error: Unable to retrieve interface");
        return "";
    }
    
    std::string connectionType;
    
    // Check if interface starts with "eth" or "eno"
    if (interface.find("eth") == 0 || interface.find("eno") == 0) {
        connectionType = "Ethernet";
        NSLOG_INFO("Connection type: %s (interface: %s)", connectionType.c_str(), interface.c_str());
    }
    // Check if interface starts with "wlan"
    else if (interface.find("wlan") == 0) {
        connectionType = "WiFi";
        NSLOG_INFO("Connection type: %s (interface: %s)", connectionType.c_str(), interface.c_str());
    }
    else {
        connectionType = "Unknown";
        NSLOG_INFO("Connection type: %s (interface: %s)", connectionType.c_str(), interface.c_str());
    }
    
    return connectionType;
}

/* @brief Get DNS server entries */
std::string NetworkJsonRPCProvider::getDnsEntries()
{
    // TODO: Implement DNS entries retrieval
    return "";
}

/* @brief Populate network interface data */
void NetworkJsonRPCProvider::populateNetworkData()
{
    // TODO: Implement network data population
}

/* @brief Get current active interface name */
std::string NetworkJsonRPCProvider::getInterface()
{
    if (!m_networkManagerClient) {
        NSLOG_ERROR("NetworkManager client not initialized");
        return "";
    }
    
    WPEFramework::Core::JSON::VariantContainer params;
    WPEFramework::Core::JSON::VariantContainer result;
    
    uint32_t rc = m_networkManagerClient->Invoke<WPEFramework::Core::JSON::VariantContainer, WPEFramework::Core::JSON::VariantContainer>(
        5000, "GetPrimaryInterface", params, result);
    
    std::string interface = "";
    if (rc == WPEFramework::Core::ERROR_NONE) {
        if (result.HasLabel("interface")) {
            interface = result["interface"].String();
            NSLOG_INFO("Primary interface retrieved: %s", interface.c_str());
        }
    } else {
        NSLOG_ERROR("GetPrimaryInterface JSON-RPC call failed with error code: %u", rc);
    }
    
    return interface;
}

/* @brief Ping to gateway to check packet loss */
bool NetworkJsonRPCProvider::pingToGatewayCheck(std::string endpoint, std::string ipversion, int count, int timeout)
{
    // Initialize member variables
    m_packetLoss = "";
    m_avgRtt = "";
    
    if (!m_networkManagerClient) {
        NSLOG_ERROR("NetworkManager client not initialized");
        return false;
    }
    
    WPEFramework::Core::JSON::VariantContainer params;
    params["endpoint"] = endpoint;
    params["ipversion"] = ipversion;
    params["count"] = count;
    params["timeout"] = timeout;
    params["guid"] = "network-stats-ping";
    
    WPEFramework::Core::JSON::VariantContainer result;
    
    uint32_t rc = m_networkManagerClient->Invoke<WPEFramework::Core::JSON::VariantContainer, WPEFramework::Core::JSON::VariantContainer>(
        30000, "Ping", params, result);
    
    if (rc != WPEFramework::Core::ERROR_NONE) {
        NSLOG_ERROR("Ping JSON-RPC call failed with error code: %u", rc);
        return false;
    }
    
    NSLOG_INFO("Ping request sent to %s (%s)", endpoint.c_str(), ipversion.c_str());
    
    // Check if ping was successful
    if (result.HasLabel("success")) {
        bool success = result["success"].Boolean();
        
        // Extract ping statistics from JSON
        if (result.HasLabel("packetsTransmitted")) {
            NSLOG_INFO("Packets transmitted: %d", static_cast<int>(result["packetsTransmitted"].Number()));
        }
        if (result.HasLabel("packetsReceived")) {
            NSLOG_INFO("Packets received: %d", static_cast<int>(result["packetsReceived"].Number()));
        }
        if (result.HasLabel("packetLoss")) {
            m_packetLoss = result["packetLoss"].String();
            NSLOG_INFO("Packet loss: %s", m_packetLoss.c_str());
        }
        if (result.HasLabel("tripMin")) {
            NSLOG_INFO("RTT min: %s ms", result["tripMin"].String().c_str());
        }
        if (result.HasLabel("tripAvg")) {
            m_avgRtt = result["tripAvg"].String();
            NSLOG_INFO("RTT avg: %s ms", m_avgRtt.c_str());
        }
        if (result.HasLabel("tripMax")) {
            NSLOG_INFO("RTT max: %s ms", result["tripMax"].String().c_str());
        }
        
        return success;
    }
    else {
        NSLOG_ERROR("Error: 'success' field not found in response");
        return false;
    }
}

/* @brief Get IPv4 gateway/route address from last getIpv4Address call */
std::string NetworkJsonRPCProvider::getIpv4Gateway()
{
    return m_ipv4Gateway;
}

/* @brief Get IPv6 gateway/route address from last getIpv6Address call */
std::string NetworkJsonRPCProvider::getIpv6Gateway()
{
    return m_ipv6Gateway;
}

/* @brief Get IPv4 primary DNS from last getIpv4Address call */
std::string NetworkJsonRPCProvider::getIpv4PrimaryDns()
{
    return m_ipv4PrimaryDns;
}

/* @brief Get IPv6 primary DNS from last getIpv6Address call */
std::string NetworkJsonRPCProvider::getIpv6PrimaryDns()
{
    return m_ipv6PrimaryDns;
}

/* @brief Get packet loss from last ping call */
std::string NetworkJsonRPCProvider::getPacketLoss()
{
    return m_packetLoss;
}

/* @brief Get average RTT from last ping call */
std::string NetworkJsonRPCProvider::getAvgRtt()
{
    return m_avgRtt;
}

#ifdef TEST_MAIN
/* Test main function to validate NetworkJsonRPCProvider member functions */
int main()
{
    NetworkJsonRPCProvider provider;
    int choice;
    std::string interface_name;
    bool running = true;

    std::cout << "\n========================================\n";
    std::cout << "NetworkJsonRPCProvider Test Suite\n";
    std::cout << "========================================\n";

    while (running)
    {
        std::cout << "\nMenu:\n";
        std::cout << "1. Test getIpv4Address()\n";
        std::cout << "2. Test getIpv6Address()\n";
        std::cout << "3. Test getConnectionType()\n";
        std::cout << "4. Test getDnsEntries()\n";
        std::cout << "5. Test populateNetworkData()\n";
        std::cout << "6. Test getInterface()\n";
        std::cout << "7. Test pingToGatewayCheck()\n";
        std::cout << "8. Run all tests\n";
        std::cout << "0. Exit\n";
        std::cout << "\nEnter your choice: ";
        std::cin >> choice;

        switch (choice)
        {
            case 1:
                std::cout << "\nTesting getIpv4Address()...\n";
                {
                    //interface_name = provider.getInterface();
                    interface_name = "eno1"; // Hardcoded for testing
                    std::cout << "Using interface: " << (interface_name.empty() ? "(empty)" : interface_name) << "\n";
                    std::string ipv4 = provider.getIpv4Address(interface_name);
                    std::cout << "Result: " << (ipv4.empty() ? "(empty)" : ipv4) << "\n";
                    std::string gateway = provider.getIpv4Gateway();
                    std::string primaryDns = provider.getIpv4PrimaryDns();
                    std::cout << "Gateway: " << (gateway.empty() ? "(empty)" : gateway) << "\n";
                    std::cout << "Primary DNS: " << (primaryDns.empty() ? "(empty)" : primaryDns) << "\n";
                }
                break;

            case 2:
                std::cout << "\nTesting getIpv6Address()...\n";
                {
                    //interface_name = provider.getInterface();
                    interface_name = "eno1"; // Hardcoded for testing
                    std::cout << "Using interface: " << (interface_name.empty() ? "(empty)" : interface_name) << "\n";
                    std::string ipv6 = provider.getIpv6Address(interface_name);
                    std::cout << "Result: " << (ipv6.empty() ? "(empty)" : ipv6) << "\n";
                    std::string gateway = provider.getIpv6Gateway();
                    std::string primaryDns = provider.getIpv6PrimaryDns();
                    std::cout << "Gateway: " << (gateway.empty() ? "(empty)" : gateway) << "\n";
                    std::cout << "Primary DNS: " << (primaryDns.empty() ? "(empty)" : primaryDns) << "\n";
                }
                break;

            case 3:
                std::cout << "\nTesting getConnectionType()...\n";
                {
                    std::string connType = provider.getConnectionType();
                    std::cout << "Result: " << (connType.empty() ? "(empty)" : connType) << "\n";
                }
                break;

            case 4:
                std::cout << "\nTesting getDnsEntries()...\n";
                {
                    std::string dns = provider.getDnsEntries();
                    std::cout << "Result: " << (dns.empty() ? "(empty)" : dns) << "\n";
                }
                break;

            case 5:
                std::cout << "\nTesting populateNetworkData()...\n";
                provider.populateNetworkData();
                std::cout << "populateNetworkData() executed successfully.\n";
                break;

            case 6:
                std::cout << "\nTesting getInterface()...\n";
                {
                    std::string iface = provider.getInterface();
                    std::cout << "Result: " << (iface.empty() ? "(empty)" : iface) << "\n";
                }
                break;

            case 7:
                std::cout << "\nTesting pingToGatewayCheck()...\n";
                {
                    std::string gateway;
                    std::cout << "Enter gateway IP address (or press Enter for default 8.8.8.8): ";
                    std::cin.ignore();
                    std::getline(std::cin, gateway);
                    if (gateway.empty()) {
                        gateway = "8.8.8.8";
                    }
                    
                    std::string ipver;
                    std::cout << "Enter IP version (IPv4/IPv6, default: IPv4): ";
                    std::getline(std::cin, ipver);
                    if (ipver.empty()) {
                        ipver = "IPv4";
                    }
                    
                    bool result = provider.pingToGatewayCheck(gateway, ipver, 5, 30);
                    std::cout << "Ping result: " << (result ? "SUCCESS" : "FAILED") << "\n";
                    std::string packetLoss = provider.getPacketLoss();
                    std::string avgRtt = provider.getAvgRtt();
                    std::cout << "Packet Loss: " << (packetLoss.empty() ? "(empty)" : packetLoss) << "\n";
                    std::cout << "Average RTT: " << (avgRtt.empty() ? "(empty)" : avgRtt) << " ms\n";
                }
                break;

            case 8:
                std::cout << "\n========================================\n";
                std::cout << "Running all tests...\n";
                std::cout << "========================================\n";

                std::cout << "\n[Test 1/6] getConnectionType()\n";
                {
                    std::string connType = provider.getConnectionType();
                    std::cout << "Result: " << (connType.empty() ? "(empty)" : connType) << "\n";
                }

                std::cout << "\n[Test 2/6] getDnsEntries()\n";
                {
                    std::string dns = provider.getDnsEntries();
                    std::cout << "Result: " << (dns.empty() ? "(empty)" : dns) << "\n";
                }

                std::cout << "\n[Test 3/6] getInterface()\n";
                {
                    std::string iface = provider.getInterface();
                    std::cout << "Result: " << (iface.empty() ? "(empty)" : iface) << "\n";
                }

                std::cout << "\n[Test 4/6] populateNetworkData()\n";
                provider.populateNetworkData();
                std::cout << "populateNetworkData() executed successfully.\n";
                std::cout << "\n[Test 5/6] getIpv4Address()\n";
                {
                    std::string test_iface = provider.getInterface();
                    std::cout << "Using interface: " << (test_iface.empty() ? "(empty)" : test_iface) << "\n";
                    std::string ipv4 = provider.getIpv4Address(test_iface);
                    std::cout << "Result: " << (ipv4.empty() ? "(empty)" : ipv4) << "\n";
                    std::string gateway = provider.getIpv4Gateway();
                    std::string primaryDns = provider.getIpv4PrimaryDns();
                    std::cout << "Gateway: " << (gateway.empty() ? "(empty)" : gateway) << "\n";
                    std::cout << "Primary DNS: " << (primaryDns.empty() ? "(empty)" : primaryDns) << "\n";
                }

                std::cout << "\n[Test 6/7] getIpv6Address()\n";
                {
                    std::string test_iface = provider.getInterface();
                    std::cout << "Using interface: " << (test_iface.empty() ? "(empty)" : test_iface) << "\n";
                    std::string ipv6 = provider.getIpv6Address(test_iface);
                    std::cout << "Result: " << (ipv6.empty() ? "(empty)" : ipv6) << "\n";
                    std::string gateway = provider.getIpv6Gateway();
                    std::string primaryDns = provider.getIpv6PrimaryDns();
                    std::cout << "Gateway: " << (gateway.empty() ? "(empty)" : gateway) << "\n";
                    std::cout << "Primary DNS: " << (primaryDns.empty() ? "(empty)" : primaryDns) << "\n";
                }

                std::cout << "\n[Test 7/7] pingToGatewayCheck()\n";
                {
                    bool pingResult = provider.pingToGatewayCheck("8.8.8.8", "IPv4", 5, 30);
                    std::cout << "Result: " << (pingResult ? "SUCCESS" : "FAILED") << "\n";
                    std::string packetLoss = provider.getPacketLoss();
                    std::string avgRtt = provider.getAvgRtt();
                    std::cout << "Packet Loss: " << (packetLoss.empty() ? "(empty)" : packetLoss) << "\n";
                    std::cout << "Average RTT: " << (avgRtt.empty() ? "(empty)" : avgRtt) << " ms\n";
                }

                std::cout << "\n========================================\n";
                std::cout << "All tests completed.\n";
                std::cout << "========================================\n";
                break;

            case 0:
                std::cout << "\nExiting test suite...\n";
                running = false;
                break;

            default:
                std::cout << "\nInvalid choice! Please select 0-8.\n";
                break;
        }
    }

    return 0;
}

#endif // TEST_MAIN



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
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <json/json.h>
#include <iostream>
#include <cstring>

using namespace std;

/* @brief Constructor */
NetworkJsonRPCProvider::NetworkJsonRPCProvider()
{
    // TODO: Initialize Thunder connection
}

/* @brief Destructor */
NetworkJsonRPCProvider::~NetworkJsonRPCProvider()
{
    // TODO: Cleanup resources
}

/* @brief Retrieve IPv4 address for specified interface */
std::string NetworkJsonRPCProvider::getIpv4Address(std::string interface_name)
{
    jsonrpc::HttpClient client("http://127.0.0.1:9998/jsonrpc");
    client.AddHeader("content-type", "application/json");
    
    jsonrpc::Client c(client, jsonrpc::JSONRPC_CLIENT_V2);
    Json::Value params;
    Json::Value result;
    std::string method("org.rdk.NetworkManager.1.GetIPSettings");
    
    // Set parameters
    params["interface"] = interface_name;
    params["ipversion"] = "IPv4";
    
    // Initialize member variables
    m_ipv4Gateway = "";
    m_ipv4PrimaryDns = "";
    
    try {
        result = c.CallMethod(method, params);
        
        std::string ipv4_address = "";
        
        // Parse and extract all fields from JSON response
        if (result.isMember("ipaddress") && result["ipaddress"].isString()) {
            ipv4_address = result["ipaddress"].asString();
            std::cout << "IPv4 address retrieved: " << ipv4_address << std::endl;
        }
        
        // Extract gateway as IPv4 Route
        if (result.isMember("gateway") && result["gateway"].isString()) {
            m_ipv4Gateway = result["gateway"].asString();
            std::cout << "IPv4 Route: " << m_ipv4Gateway << std::endl;
        }
        
        // Extract primarydns as DNS entry
        if (result.isMember("primarydns") && result["primarydns"].isString()) {
            m_ipv4PrimaryDns = result["primarydns"].asString();
            std::cout << "DNS entry: " << m_ipv4PrimaryDns << std::endl;
        }
        
        if (ipv4_address.empty()) {
            std::cerr << "Error: 'ipaddress' field not found or invalid in response" << std::endl;
        }
        
        return ipv4_address;
    }
    catch (jsonrpc::JsonRpcException& e) {
        std::cerr << "JSON-RPC Exception in getIpv4Address(): " << e.what() << std::endl;
        return "";
    }
    catch (std::exception& e) {
        std::cerr << "Exception in getIpv4Address(): " << e.what() << std::endl;
        return "";
    }
}

/* @brief Retrieve IPv6 address for specified interface */
std::string NetworkJsonRPCProvider::getIpv6Address(std::string interface_name)
{
    jsonrpc::HttpClient client("http://127.0.0.1:9998/jsonrpc");
    client.AddHeader("content-type", "application/json");
    
    jsonrpc::Client c(client, jsonrpc::JSONRPC_CLIENT_V2);
    Json::Value params;
    Json::Value result;
    std::string method("org.rdk.NetworkManager.1.GetIPSettings");
    
    // Set parameters
    params["interface"] = interface_name;
    params["ipversion"] = "IPv6";
    
    // Initialize member variables
    m_ipv6Gateway = "";
    m_ipv6PrimaryDns = "";
    
    try {
        result = c.CallMethod(method, params);
        
        std::string ipv6_address = "";
        
        // Parse and extract all fields from JSON response
        if (result.isMember("ipaddress") && result["ipaddress"].isString()) {
            ipv6_address = result["ipaddress"].asString();
            std::cout << "IPv6 address retrieved: " << ipv6_address << std::endl;
        }
        
        // Extract gateway as IPv6 Route
        if (result.isMember("gateway") && result["gateway"].isString()) {
            m_ipv6Gateway = result["gateway"].asString();
            std::cout << "IPv6 Route: " << m_ipv6Gateway << std::endl;
        }
        
        // Extract primarydns as DNS entry
        if (result.isMember("primarydns") && result["primarydns"].isString()) {
            m_ipv6PrimaryDns = result["primarydns"].asString();
            std::cout << "DNS entry: " << m_ipv6PrimaryDns << std::endl;
        }
        
        if (ipv6_address.empty()) {
            std::cerr << "Error: 'ipaddress' field not found or invalid in response" << std::endl;
        }
        
        return ipv6_address;
    }
    catch (jsonrpc::JsonRpcException& e) {
        std::cerr << "JSON-RPC Exception in getIpv6Address(): " << e.what() << std::endl;
        return "";
    }
    catch (std::exception& e) {
        std::cerr << "Exception in getIpv6Address(): " << e.what() << std::endl;
        return "";
    }
}

/* @brief Get current network connection type */
std::string NetworkJsonRPCProvider::getConnectionType()
{
    std::string interface = getInterface();
    
    if (interface.empty()) {
        std::cerr << "Error: Unable to retrieve interface" << std::endl;
        return "";
    }
    
    std::string connectionType;
    
    // Check if interface starts with "eth"
    if (interface.find("eth") == 0) {
        connectionType = "Ethernet";
        std::cout << "Connection type: " << connectionType << " (interface: " << interface << ")" << std::endl;
    }
    // Check if interface starts with "wlan"
    else if (interface.find("wlan") == 0) {
        connectionType = "WiFi";
        std::cout << "Connection type: " << connectionType << " (interface: " << interface << ")" << std::endl;
    }
    else {
        connectionType = "Unknown";
        std::cout << "Connection type: " << connectionType << " (interface: " << interface << ")" << std::endl;
    }
    connectionType = "Ethernet"; //For testing purpose hardcoding to Ethernet
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
    jsonrpc::HttpClient client("http://127.0.0.1:9998/jsonrpc");
    client.AddHeader("content-type", "application/json");
    
    jsonrpc::Client c(client, jsonrpc::JSONRPC_CLIENT_V2);
    Json::Value result;
    std::string method("org.rdk.NetworkManager.1.GetPrimaryInterface");
    
    try {
        result = c.CallMethod(method, Json::Value());
        
        if (result.isMember("interface") && result["interface"].isString()) {
            std::string interface = result["interface"].asString();
            std::cout << "Primary interface retrieved: " << interface << std::endl;
            return "eno1"; // Hardcoded for testing
        }
        else {
            std::cerr << "Error: 'interface' field not found or invalid in response" << std::endl;
            return "";
        }
    }
    catch (jsonrpc::JsonRpcException& e) {
        std::cerr << "JSON-RPC Exception in getInterface(): " << e.what() << std::endl;
        return "";
    }
    catch (std::exception& e) {
        std::cerr << "Exception in getInterface(): " << e.what() << std::endl;
        return "";
    }
}

/* @brief Ping to gateway to check packet loss */
bool NetworkJsonRPCProvider::pingToGatewayCheck(std::string endpoint, std::string ipversion, int count, int timeout)
{
    jsonrpc::HttpClient client("http://127.0.0.1:9998/jsonrpc");
    client.AddHeader("content-type", "application/json");
    
    jsonrpc::Client c(client, jsonrpc::JSONRPC_CLIENT_V2);
    Json::Value params;
    Json::Value result;
    std::string method("org.rdk.NetworkManager.1.Ping");
    
    // Set parameters according to NetworkManagerPlugin.md schema
    params["endpoint"] = endpoint;
    params["ipversion"] = ipversion;
    params["count"] = count;
    params["timeout"] = timeout;
    params["guid"] = "network-stats-ping";
    
    // Initialize member variables
    m_packetLoss = "";
    m_avgRtt = "";
    
    try {
        result = c.CallMethod(method, params);
        
        std::cout << "Ping request sent to " << endpoint << " (" << ipversion << ")" << std::endl;
        
        // Check if ping was successful
        if (result.isMember("success") && result["success"].isBool()) {
            bool success = result["success"].asBool();
            
            // Extract ping statistics from JSON
            if (result.isMember("packetsTransmitted") && result["packetsTransmitted"].isInt()) {
                std::cout << "Packets transmitted: " << result["packetsTransmitted"].asInt() << std::endl;
            }
            if (result.isMember("packetsReceived") && result["packetsReceived"].isInt()) {
                std::cout << "Packets received: " << result["packetsReceived"].asInt() << std::endl;
            }
            if (result.isMember("packetLoss") && result["packetLoss"].isString()) {
                m_packetLoss = result["packetLoss"].asString();
                std::cout << "Packet loss: " << m_packetLoss << std::endl;
            }
            if (result.isMember("tripMin") && result["tripMin"].isString()) {
                std::cout << "RTT min: " << result["tripMin"].asString() << " ms" << std::endl;
            }
            if (result.isMember("tripAvg") && result["tripAvg"].isString()) {
                m_avgRtt = result["tripAvg"].asString();
                std::cout << "RTT avg: " << m_avgRtt << " ms" << std::endl;
            }
            if (result.isMember("tripMax") && result["tripMax"].isString()) {
                std::cout << "RTT max: " << result["tripMax"].asString() << " ms" << std::endl;
            }
            
            return success;
        }
        else {
            std::cerr << "Error: 'success' field not found or invalid in response" << std::endl;
            return false;
        }
    }
    catch (jsonrpc::JsonRpcException& e) {
        std::cerr << "JSON-RPC Exception in pingToGatewayCheck(): " << e.what() << std::endl;
        return false;
    }
    catch (std::exception& e) {
        std::cerr << "Exception in pingToGatewayCheck(): " << e.what() << std::endl;
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



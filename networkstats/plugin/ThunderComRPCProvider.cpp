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

#include "ThunderComRPCProvider.h"
#include "NetworkConnectionStatsLogger.h"
// INetworkManager.h is included here (not in the header) to prevent the
// WPEFramework::Exchange::myIDs redefinition conflict with INetworkConnectionStats.h.
#include "INetworkManager.h"
#include <map>
#include <mutex>
#include <atomic>

using namespace std;
using namespace WPEFramework;
using namespace NetworkConnectionStatsLogger;


// ---------------------------------------------------------------------------
// NetworkManagerNotification — COM-RPC notification sink
//
// Receives typed COM-RPC callbacks from INetworkManager, converts them to
// VariantContainer parameters, and dispatches to stored std::function handlers.
//
// AddRef/Release are intentionally non-deleting (like Core::Sink<T>): lifetime
// is owned exclusively by NetworkComRPCProvider.
// ---------------------------------------------------------------------------
class NetworkManagerNotification
    : public WPEFramework::Exchange::INetworkManager::INotification {
public:
    NetworkManagerNotification() = default;
    ~NetworkManagerNotification() = default;

    BEGIN_INTERFACE_MAP(NetworkManagerNotification)
    INTERFACE_ENTRY(WPEFramework::Exchange::INetworkManager::INotification)
    END_INTERFACE_MAP

    void AddCallback(const std::string& eventName,
        std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> callback)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_callbacks[eventName] = callback;
    }

    void onInterfaceStateChange(
        const WPEFramework::Exchange::INetworkManager::InterfaceState state,
        const std::string interface) override
    {
        std::lock_guard<std::mutex> lock(m_lock);
        auto it = m_callbacks.find("onInterfaceStateChange");
        if (it == m_callbacks.end()) return;

        const char* s = "";
        switch (state) {
            case WPEFramework::Exchange::INetworkManager::INTERFACE_ADDED:        s = "INTERFACE_ADDED";        break;
            case WPEFramework::Exchange::INetworkManager::INTERFACE_LINK_UP:      s = "INTERFACE_LINK_UP";      break;
            case WPEFramework::Exchange::INetworkManager::INTERFACE_LINK_DOWN:    s = "INTERFACE_LINK_DOWN";    break;
            case WPEFramework::Exchange::INetworkManager::INTERFACE_ACQUIRING_IP: s = "INTERFACE_ACQUIRING_IP"; break;
            case WPEFramework::Exchange::INetworkManager::INTERFACE_REMOVED:      s = "INTERFACE_REMOVED";      break;
            case WPEFramework::Exchange::INetworkManager::INTERFACE_DISABLED:     s = "INTERFACE_DISABLED";     break;
            default: break;
        }
        WPEFramework::Core::JSON::VariantContainer params;
        params["interface"] = interface;
        params["status"]    = std::string(s);
        it->second(params);
    }

    void onActiveInterfaceChange(const std::string prevActiveInterface,
        const std::string currentActiveInterface) override
    {
        std::lock_guard<std::mutex> lock(m_lock);
        auto it = m_callbacks.find("onActiveInterfaceChange");
        if (it == m_callbacks.end()) return;

        WPEFramework::Core::JSON::VariantContainer params;
        params["prevActiveInterface"]    = prevActiveInterface;
        params["currentActiveInterface"] = currentActiveInterface;
        it->second(params);
    }

    void onIPAddressChange(const std::string interface, const std::string ipversion,
        const std::string ipaddress,
        const WPEFramework::Exchange::INetworkManager::IPStatus status) override
    {
        std::lock_guard<std::mutex> lock(m_lock);
        auto it = m_callbacks.find("onIPAddressChange");
        if (it == m_callbacks.end()) return;

        WPEFramework::Core::JSON::VariantContainer params;
        params["interface"] = interface;
        params["ipversion"] = ipversion;
        params["ipaddress"] = ipaddress;
        params["status"]    = std::string(
            status == WPEFramework::Exchange::INetworkManager::IP_ACQUIRED
                ? "ACQUIRED" : "LOST");
        it->second(params);
    }

    void onWiFiStateChange(
        const WPEFramework::Exchange::INetworkManager::WiFiState state) override
    {
        std::lock_guard<std::mutex> lock(m_lock);
        auto it = m_callbacks.find("onWiFiStateChange");
        if (it == m_callbacks.end()) return;

        WPEFramework::Core::JSON::VariantContainer params;
        params["status"] = std::string(
            state == WPEFramework::Exchange::INetworkManager::WIFI_STATE_CONNECTED
                ? "WIFI_STATE_CONNECTED" : "WIFI_STATE_OTHER");
        it->second(params);
    }

private:
    std::mutex m_lock;
    std::map<std::string,
        std::function<void(const WPEFramework::Core::JSON::VariantContainer&)>>
        m_callbacks;
};

/* @brief Constructor */
NetworkComRPCProvider::NetworkComRPCProvider()
    : m_service(nullptr)
    , m_notification(new Core::Sink<NetworkManagerNotification>())
    , m_notificationRegistered(false)
{
    NSLOG_INFO("NetworkComRPCProvider constructed");
}

/* @brief Destructor */
NetworkComRPCProvider::~NetworkComRPCProvider()
{
    if (m_notificationRegistered && m_service) {
        auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
        if (_nwmgr) {
            _nwmgr->Unregister(m_notification);
            _nwmgr->Release();
        }
        m_notificationRegistered = false;
    }
    delete m_notification;
    m_notification = nullptr;
    m_service = nullptr;
}

/* @brief Initialize the provider — store the IShell for later QueryInterfaceByCallsign calls */
bool NetworkComRPCProvider::Initialize(PluginHost::IShell* service)
{
    if (!service) {
        NSLOG_ERROR("Initialize: service is nullptr");
        return false;
    }
    m_service = service;
    NSLOG_INFO("NetworkComRPCProvider initialized with IShell service");
    return true;
}

/* @brief Retrieve IPv4 address for specified interface */
std::string NetworkComRPCProvider::getIpv4Address(std::string interface_name)
{
    std::string ipv4_address = "";
    m_ipv4Gateway    = "";
    m_ipv4PrimaryDns = "";

    if (!m_service) {
        NSLOG_ERROR("NetworkManager service not initialized");
        return "";
    }

    auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
    if (_nwmgr) {
        Exchange::INetworkManager::IPAddress address{};
        string iface = interface_name;
        uint32_t rc = _nwmgr->GetIPSettings(iface, string("IPv4"), address);
        _nwmgr->Release();

        if (rc == Core::ERROR_NONE) {
            ipv4_address     = address.ipaddress;
            m_ipv4Gateway    = address.gateway;
            m_ipv4PrimaryDns = address.primarydns;
            NSLOG_INFO("IPv4 address retrieved: %s", ipv4_address.c_str());
            NSLOG_INFO("IPv4 Route: %s", m_ipv4Gateway.c_str());
            NSLOG_INFO("DNS entry: %s", m_ipv4PrimaryDns.c_str());
        } else {
            NSLOG_ERROR("GetIPSettings (IPv4) failed with error code: %u", rc);
        }
    } else {
        NSLOG_ERROR("Failed to acquire INetworkManager interface");
    }

    return ipv4_address;
}

/* @brief Retrieve IPv6 address for specified interface */
std::string NetworkComRPCProvider::getIpv6Address(std::string interface_name)
{
    std::string ipv6_address = "";
    m_ipv6Gateway    = "";
    m_ipv6PrimaryDns = "";

    if (!m_service) {
        NSLOG_ERROR("NetworkManager service not initialized");
        return "";
    }

    auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
    if (_nwmgr) {
        Exchange::INetworkManager::IPAddress address{};
        string iface = interface_name;
        uint32_t rc = _nwmgr->GetIPSettings(iface, string("IPv6"), address);
        _nwmgr->Release();

        if (rc == Core::ERROR_NONE) {
            ipv6_address     = address.ipaddress;
            m_ipv6Gateway    = address.gateway;
            m_ipv6PrimaryDns = address.primarydns;
            NSLOG_INFO("IPv6 address retrieved: %s", ipv6_address.c_str());
            NSLOG_INFO("IPv6 Route: %s", m_ipv6Gateway.c_str());
            NSLOG_INFO("DNS entry: %s", m_ipv6PrimaryDns.c_str());
        } else {
            NSLOG_ERROR("GetIPSettings (IPv6) failed with error code: %u", rc);
        }
    } else {
        NSLOG_ERROR("Failed to acquire INetworkManager interface");
    }

    return ipv6_address;
}

/* @brief Get current network connection type */
std::string NetworkComRPCProvider::getConnectionType()
{
    std::string interface = getInterface();

    if (interface.empty()) {
        NSLOG_ERROR("Error: Unable to retrieve interface");
        return "";
    }

    std::string connectionType;

    if (interface.find("eth") == 0 || interface.find("eno") == 0) {
        connectionType = "Ethernet";
    } else if (interface.find("wlan") == 0) {
        connectionType = "WiFi";
    } else {
        connectionType = "Unknown";
    }
    NSLOG_INFO("Connection type: %s (interface: %s)", connectionType.c_str(), interface.c_str());

    return connectionType;
}

/* @brief Get DNS server entries */
std::string NetworkComRPCProvider::getDnsEntries()
{
    std::string dnsEntries = "";

    if (!m_ipv4PrimaryDns.empty()) {
        dnsEntries += m_ipv4PrimaryDns;
    }
    if (!m_ipv6PrimaryDns.empty()) {
        if (!dnsEntries.empty()) {
            dnsEntries += ",";
        }
        dnsEntries += m_ipv6PrimaryDns;
    }

    return dnsEntries;
}

/* @brief Populate network interface data */
void NetworkComRPCProvider::populateNetworkData()
{
    std::string interface = getInterface();

    if (!interface.empty()) {
        getIpv4Address(interface);
        getIpv6Address(interface);
        NSLOG_INFO("Network data populated for interface: %s", interface.c_str());
    }
}

/* @brief Get current active interface name */
std::string NetworkComRPCProvider::getInterface()
{
    if (!m_service) {
        NSLOG_ERROR("NetworkManager service not initialized");
        return "";
    }

    string interface = "";

    auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
    if (_nwmgr) {
        uint32_t rc = _nwmgr->GetPrimaryInterface(interface);
        _nwmgr->Release();

        if (rc == Core::ERROR_NONE) {
            NSLOG_INFO("Primary interface retrieved: %s", interface.c_str());
        } else {
            NSLOG_ERROR("GetPrimaryInterface failed with error code: %u", rc);
        }
    } else {
        NSLOG_ERROR("Failed to acquire INetworkManager interface");
    }

    return interface;
}

/* @brief Ping to gateway to check packet loss */
bool NetworkComRPCProvider::pingToGatewayCheck(std::string endpoint, std::string ipversion, int count, int timeout)
{
    m_packetLoss = "";
    m_avgRtt     = "";

    if (!m_service) {
        NSLOG_ERROR("NetworkManager service not initialized");
        return false;
    }

    string pingResult{};
    uint32_t rc = Core::ERROR_UNAVAILABLE;

    auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
    if (_nwmgr) {
        rc = _nwmgr->Ping(ipversion, endpoint,
                          static_cast<uint32_t>(count),
                          static_cast<uint16_t>(timeout),
                          string("network-stats-ping"),
                          pingResult);
        _nwmgr->Release();
    } else {
        NSLOG_ERROR("Failed to acquire INetworkManager interface");
        return false;
    }

    if (rc != Core::ERROR_NONE) {
        NSLOG_ERROR("Ping failed with error code: %u", rc);
        return false;
    }

    NSLOG_INFO("Ping request sent to %s (%s)", endpoint.c_str(), ipversion.c_str());

    // Parse the JSON response string returned by Ping
    Core::JSON::VariantContainer reply;
    reply.FromString(pingResult);

    bool success = false;
    if (reply.HasLabel("success")) {
        success = reply["success"].Boolean();
    }
    if (reply.HasLabel("packetsTransmitted")) {
        NSLOG_INFO("Packets transmitted: %d", static_cast<int>(reply["packetsTransmitted"].Number()));
    }
    if (reply.HasLabel("packetsReceived")) {
        NSLOG_INFO("Packets received: %d", static_cast<int>(reply["packetsReceived"].Number()));
    }
    if (reply.HasLabel("packetLoss")) {
        m_packetLoss = reply["packetLoss"].String();
        NSLOG_INFO("Packet loss: %s", m_packetLoss.c_str());
    }
    if (reply.HasLabel("tripMin")) {
        NSLOG_INFO("RTT min: %s ms", reply["tripMin"].String().c_str());
    }
    if (reply.HasLabel("tripAvg")) {
        m_avgRtt = reply["tripAvg"].String();
        NSLOG_INFO("RTT avg: %s ms", m_avgRtt.c_str());
    }
    if (reply.HasLabel("tripMax")) {
        NSLOG_INFO("RTT max: %s ms", reply["tripMax"].String().c_str());
    }

    return success;
}

/* @brief Get IPv4 gateway/route address from last getIpv4Address call */
std::string NetworkComRPCProvider::getIpv4Gateway()
{
    return m_ipv4Gateway;
}

/* @brief Get IPv6 gateway/route address from last getIpv6Address call */
std::string NetworkComRPCProvider::getIpv6Gateway()
{
    return m_ipv6Gateway;
}

/* @brief Get IPv4 primary DNS from last getIpv4Address call */
std::string NetworkComRPCProvider::getIpv4PrimaryDns()
{
    return m_ipv4PrimaryDns;
}

/* @brief Get IPv6 primary DNS from last getIpv6Address call */
std::string NetworkComRPCProvider::getIpv6PrimaryDns()
{
    return m_ipv6PrimaryDns;
}

/* @brief Get packet loss from last ping call */
std::string NetworkComRPCProvider::getPacketLoss()
{
    return m_packetLoss;
}

/* @brief Get average RTT from last ping call */
std::string NetworkComRPCProvider::getAvgRtt()
{
    return m_avgRtt;
}

/* @brief Subscribe to NetworkManager events via COM-RPC INotification */
uint32_t NetworkComRPCProvider::SubscribeToEvent(const std::string& eventName,
    std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> callback)
{
    if (!m_service) {
        NSLOG_ERROR("NetworkManager service not initialized");
        return Core::ERROR_UNAVAILABLE;
    }

    // Store callback in the notification sink (safe to call multiple times)
    m_notification->AddCallback(eventName, callback);

    // Register notification with NetworkManager only on the first subscription
    if (m_notificationRegistered) {
        NSLOG_INFO("Notification already registered; callback added for event: %s", eventName.c_str());
        return Core::ERROR_NONE;
    }

    uint32_t result = Core::ERROR_UNAVAILABLE;
    auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
    if (_nwmgr) {
        result = _nwmgr->Register(m_notification);
        _nwmgr->Release();

        if (result == Core::ERROR_NONE) {
            m_notificationRegistered = true;
            NSLOG_INFO("COM-RPC notification registered for event: %s", eventName.c_str());
        } else {
            NSLOG_ERROR("Failed to register COM-RPC notification, error code: %u", result);
        }
    } else {
        NSLOG_ERROR("Failed to acquire INetworkManager interface for notification registration");
    }

    return result;
}

/* @brief Invoke WiFiConnect API on NetworkManager to reconnect to last SSID */
uint32_t NetworkComRPCProvider::invokeWiFiConnect()
{
    if (!m_service) {
        NSLOG_ERROR("NetworkManager service not initialized");
        return Core::ERROR_UNAVAILABLE;
    }

    uint32_t rc = Core::ERROR_UNAVAILABLE;

    auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
    if (_nwmgr) {
        Exchange::INetworkManager::WiFiConnectTo ssid{};
        rc = _nwmgr->WiFiConnect(ssid);
        _nwmgr->Release();

        if (rc == Core::ERROR_NONE) {
            NSLOG_INFO("WiFiConnect invoked successfully on NetworkManager");
        } else {
            NSLOG_ERROR("WiFiConnect invocation failed with error code: %u", rc);
        }
    } else {
        NSLOG_ERROR("Failed to acquire INetworkManager interface");
    }

    return rc;
}

#ifdef TEST_MAIN
/* Test main function to validate NetworkComRPCProvider member functions */
int main()
{
    NetworkComRPCProvider provider;
    int choice;
    std::string interface_name;
    bool running = true;

    std::cout << "\n========================================\n";
    std::cout << "NetworkComRPCProvider Test Suite\n";
    std::cout << "========================================\n";
    std::cout << "Note: TEST_MAIN requires a valid PluginHost::IShell* to run.\n";
    std::cout << "      Pass nullptr to Initialize() for a dry-run check only.\n";

    return 0;
}
#endif // TEST_MAIN


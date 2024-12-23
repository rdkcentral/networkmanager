/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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
#include "LegacyPlugin_NetworkAPIs.h"
#include "NetworkManagerLogger.h"
#include "NetworkManagerJsonEnum.h"


using namespace std;
using namespace WPEFramework::Plugin;
#define API_VERSION_NUMBER_MAJOR 2
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0
#define NETWORK_MANAGER_CALLSIGN    "org.rdk.NetworkManager.1"
#define DEFAULT_PING_PACKETS 15

#define LOG_INPARAM() { string json; parameters.ToString(json); NMLOG_INFO("params=%s", json.c_str() ); }
#define LOG_OUTPARAM() { string json; response.ToString(json); NMLOG_INFO("response=%s", json.c_str() ); }

#define returnJson(rc) \
    { \
        if (Core::ERROR_NONE == rc)                 \
            response["success"] = true;             \
        else                                        \
            response["success"] = false;            \
        LOG_OUTPARAM();                             \
        return Core::ERROR_NONE;                    \
    }

namespace WPEFramework
{
    class Job : public Core::IDispatch {
    public:
        Job(function<void()> work)
        : _work(work)
        {
        }
        void Dispatch() override
        {
            _work();
        }

    private:
        function<void()> _work;
    };

    static Plugin::Metadata<Plugin::Network> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
    );

    namespace Plugin
    {
        SERVICE_REGISTRATION(Network, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);
        Network::Network()
        : PluginHost::JSONRPC()
        , m_service(nullptr)
        , _notification(this)
       {
           registerLegacyMethods();
       }

        Network::~Network()
        {
        }

        void Network::activatePrimaryPlugin()
        {
            uint32_t result = Core::ERROR_ASYNC_FAILED;
            string callsign(NETWORK_MANAGER_CALLSIGN);
            Core::Event event(false, true);
            Core::IWorkerPool::Instance().Submit(Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create([&]() {
                auto interface = m_service->QueryInterfaceByCallsign<PluginHost::IShell>(callsign);
                if (interface == nullptr) {
                    result = Core::ERROR_UNAVAILABLE;
                    NMLOG_WARNING("no IShell for %s", callsign.c_str());
                } else {
                    NMLOG_INFO("Activating %s", callsign.c_str());
                    result = interface->Activate(PluginHost::IShell::reason::REQUESTED);
                    interface->Release();
                }
                event.SetEvent();
            })));
            event.Lock();

            return;
        }

        const string Network::Initialize(PluginHost::IShell*  service )
        {
            m_service = service;
            m_service->AddRef();

            string callsign(NETWORK_MANAGER_CALLSIGN);

            string token = "";

            // TODO: use interfaces and remove token
            auto security = m_service->QueryInterfaceByCallsign<PluginHost::IAuthenticate>("SecurityAgent");
            if (security != nullptr) {
                string payload = "http://localhost";
                if (security->CreateToken(static_cast<uint16_t>(payload.length()), reinterpret_cast<const uint8_t*>(payload.c_str()), token) == Core::ERROR_NONE) {
                    NMLOG_DEBUG("Network plugin got security token");
                } else {
                    NMLOG_WARNING("Network plugin failed to get security token");
                }
                security->Release();
            } else {
                NMLOG_DEBUG("Network plugin: No security agent");
            }

            string query = "token=" + token;

            auto interface = m_service->QueryInterfaceByCallsign<PluginHost::IShell>(callsign);
            if (interface != nullptr)
            {
                PluginHost::IShell::state state = interface->State(); 
                if((PluginHost::IShell::state::ACTIVATED  == state) || (PluginHost::IShell::state::ACTIVATION == state))
                {
                    NMLOG_INFO("Dependency Plugin '%s' Ready", callsign.c_str());
                }
                else
                {
                    NMLOG_INFO("Lets attempt to activate the Plugin '%s'", callsign.c_str());
                    activatePrimaryPlugin();
                }
                interface->Release();
            }

            auto nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (nwmgr)
            {
                NMLOG_INFO("Lets subscribe to Events of '%s' Plugin", callsign.c_str());
                nwmgr->Register(&_notification);
                nwmgr->Release();
            }

            return string();
        }

        void Network::Deinitialize(PluginHost::IShell* /* service */)
        {
            unregisterLegacyMethods();
            m_service->Release();
            m_service = nullptr;
        }

        string Network::Information() const
        {
             return(string());
        }

        /**
         * Hook up all our JSON RPC methods
         *
         * Each method definition comprises of:
         *  * Input parameters
         *  * Output parameters
         *  * Method name
         *  * Function that implements that method
         */
        void Network::registerLegacyMethods(void)
        {
            CreateHandler({2});

            Register("getStbIp",                          &Network::getStbIp, this);
            Register("getSTBIPFamily",                    &Network::getSTBIPFamily, this);
            Register("getInterfaces",                     &Network::getInterfaces, this);
            Register("isInterfaceEnabled",                &Network::isInterfaceEnabled, this);
            Register("getPublicIP",                       &Network::getPublicIP, this);
            Register("setInterfaceEnabled",               &Network::setInterfaceEnabled, this);
            Register("getDefaultInterface",               &Network::getDefaultInterface, this);
            Register("setDefaultInterface",               &Network::setDefaultInterface, this);
            Register("setIPSettings",                     &Network::setIPSettings, this);
            Register("getIPSettings",                     &Network::getIPSettings, this);
            Register("getInternetConnectionState",        &Network::getInternetConnectionState, this);
            Register("ping",                              &Network::doPing, this);
            Register("isConnectedToInternet",             &Network::isConnectedToInternet, this);
            Register("setStunEndPoint",                   &Network::setStunEndPoint, this);
            Register("trace",                             &Network::doTrace, this);
            Register("setConnectivityTestEndpoints",      &Network::setConnectivityTestEndpoints, this);
            Register("startConnectivityMonitoring",       &Network::startConnectivityMonitoring, this);
            Register("getCaptivePortalURI",               &Network::getCaptivePortalURI, this);
            Register("stopConnectivityMonitoring",        &Network::stopConnectivityMonitoring, this);
            GetHandler(2)->Register<JsonObject, JsonObject>("setIPSettings", &Network::setIPSettings, this);
            GetHandler(2)->Register<JsonObject, JsonObject>("getIPSettings", &Network::getIPSettings2, this);
        }

        /**
         * Unregister all our JSON-RPC methods
         */
        void Network::unregisterLegacyMethods(void)
        {
            Unregister("getInterfaces");
            Unregister("isInterfaceEnabled");
            Unregister("getPublicIP");
            Unregister("setInterfaceEnabled");
            Unregister("getDefaultInterface");
            Unregister("setDefaultInterface");
            Unregister("setIPSettings");
            Unregister("getIPSettings");
            Unregister("getInternetConnectionState");
            Unregister("ping");
            Unregister("isConnectedToInternet");
            Unregister("setConnectivityTestEndpoints");
            Unregister("startConnectivityMonitoring");
            Unregister("getCaptivePortalURI");
            Unregister("stopConnectivityMonitoring");
        }

#define CIDR_NETMASK_IP_LEN 32
const string CIDR_PREFIXES[CIDR_NETMASK_IP_LEN+1] = {
                                                     "0.0.0.0",
                                                     "128.0.0.0",
                                                     "192.0.0.0",
                                                     "224.0.0.0",
                                                     "240.0.0.0",
                                                     "248.0.0.0",
                                                     "252.0.0.0",
                                                     "254.0.0.0",
                                                     "255.0.0.0",
                                                     "255.128.0.0",
                                                     "255.192.0.0",
                                                     "255.224.0.0",
                                                     "255.240.0.0",
                                                     "255.248.0.0",
                                                     "255.252.0.0",
                                                     "255.254.0.0",
                                                     "255.255.0.0",
                                                     "255.255.128.0",
                                                     "255.255.192.0",
                                                     "255.255.224.0",
                                                     "255.255.240.0",
                                                     "255.255.248.0",
                                                     "255.255.252.0",
                                                     "255.255.254.0",
                                                     "255.255.255.0",
                                                     "255.255.255.128",
                                                     "255.255.255.192",
                                                     "255.255.255.224",
                                                     "255.255.255.240",
                                                     "255.255.255.248",
                                                     "255.255.255.252",
                                                     "255.255.255.254",
                                                     "255.255.255.255",
                                                   };

        static bool caseInsensitiveCompare(const std::string& str1, const char* str2)
        {
            std::string upperStr1 = str1;
            std::transform(upperStr1.begin(), upperStr1.end(), upperStr1.begin(), ::toupper);
            return upperStr1 == str2;
        }

        uint32_t Network::getInterfaces (const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            LOG_INPARAM();

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                Exchange::INetworkManager::IInterfaceDetailsIterator* _interfaces{};

                rc = _nwmgr->GetAvailableInterfaces(_interfaces);
                if (Core::ERROR_NONE == rc)
                {
                    if (_interfaces != nullptr)
                    {
                        JsonArray array;
                        Exchange::INetworkManager::InterfaceDetails entry{};
                        while (_interfaces->Next(entry) == true)
                        {
                            JsonObject each;
                            Core::JSON::EnumType<Exchange::INetworkManager::InterfaceType> type{entry.type};
                            each["interface"] = type.Data();
                            each["macAddress"] = entry.mac;
                            each["enabled"] = entry.enabled;
                            each["connected"] = entry.connected;

                            array.Add(JsonValue(each));
                        }

                        _interfaces->Release();
                        response["interfaces"] = array;
                    }
                }
                _nwmgr->Release();
            }

            returnJson(rc);
        }

        uint32_t Network::setStunEndPoint(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string endPoint = parameters["server"].String();
            uint32_t port = parameters["port"].Number();
            uint32_t bindTimeout = parameters["timeout"].Number();
            uint32_t cacheTimeout = parameters["cache_timeout"].Number();

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->SetStunEndpoint(endPoint, port, bindTimeout, cacheTimeout);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t Network::setInterfaceEnabled (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            if (parameters.HasLabel("interface") && parameters.HasLabel("enabled"))
            {
                const string givenInterface = parameters["interface"].String();
                const bool enabled = parameters["enabled"].Boolean();
                const string interface = getInterfaceTypeToName(givenInterface);

                if ("wlan0" != interface && "eth0" != interface)
                    rc = Core::ERROR_BAD_REQUEST;
                else
                {
                    auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
                    if (_nwmgr)
                    {
                        rc = _nwmgr->SetInterfaceState(interface, enabled);
                        _nwmgr->Release();
                    }
                    else
                        rc = Core::ERROR_UNAVAILABLE;
                }
            }
            else
                rc = Core::ERROR_BAD_REQUEST;

            returnJson(rc);
        }

        uint32_t Network::getDefaultInterface (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string interface;

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->GetPrimaryInterface(interface);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                string mappedInterface = getInterfaceNameToType(interface);
                response["interface"] = mappedInterface;
            }
            returnJson(rc);
        }

        uint32_t Network::setDefaultInterface(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string givenInterface = parameters["interface"].String();
            const string interface = getInterfaceTypeToName(givenInterface);

            if ("wlan0" != interface && "eth0" != interface)
            {
                rc = Core::ERROR_BAD_REQUEST;
                return rc;
            }

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->SetPrimaryInterface(interface);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t Network::setIPSettings(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::IPAddress address{};

            string givenInterface = "";
            string interface = "";
            string ipversion = "";

            if (parameters.HasLabel("interface"))
            {
                givenInterface = parameters["interface"].String();
                interface = getInterfaceTypeToName(givenInterface);

                if ("wlan0" != interface && "eth0" != interface)
                {
                    return Core::ERROR_BAD_REQUEST;
                }
            }
            else
            {
                return Core::ERROR_BAD_REQUEST;
            }

            address.autoconfig = parameters["autoconfig"].Boolean();
            if (!address.autoconfig)
            {
                address.ipaddress      = parameters["ipaddr"].String();
                address.ipversion      = parameters["ipversion"].String();
                address.gateway        = parameters["gateway"].String();
                address.primarydns     = parameters["primarydns"].String();
                address.secondarydns   = parameters["secondarydns"].String();

                auto it = find(begin(CIDR_PREFIXES), end(CIDR_PREFIXES), parameters["netmask"].String());
                if (it == end(CIDR_PREFIXES))
                    return Core::ERROR_BAD_REQUEST;
                else
                    address.prefix         = distance(begin(CIDR_PREFIXES), it);

            }

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->SetIPSettings(interface, address);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t Network::internalGetIPSettings(const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            string interface{};
            string givenInterface{};
            string ipversion{};

            if (parameters.HasLabel("interface"))
            {
                givenInterface = parameters["interface"].String();
                interface = getInterfaceTypeToName(givenInterface);

                if (!interface.empty() && "wlan0" != interface && "eth0" != interface)
                {
                    return Core::ERROR_BAD_REQUEST;
                }
            }

            if (parameters.HasLabel("ipversion"))
                ipversion = parameters["ipversion"].String();

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                Exchange::INetworkManager::IPAddress address{};
                rc = _nwmgr->GetIPSettings(interface, ipversion, address);
                _nwmgr->Release();

                if (Core::ERROR_NONE == rc)
                {
                    givenInterface = getInterfaceNameToType(interface);

                    response["interface"]  = givenInterface;
                    response["ipversion"]  = address.ipversion;
                    response["autoconfig"] = address.autoconfig;
                    if (!address.ipaddress.empty())
                    {
                        response["ipaddr"]    = address.ipaddress;
                        if ("IPv4" == address.ipversion)
                        {
                            if(address.prefix > CIDR_NETMASK_IP_LEN)
                            {
                                rc = Core::ERROR_GENERAL;
                                return rc;
                            }
                            response["netmask"]  = CIDR_PREFIXES[address.prefix];
                        }
                        else
                            response["netmask"]       = address.prefix;

                        response["dhcpserver"]   = address.dhcpserver;
                        response["gateway"]      = address.gateway;
                        response["primarydns"]   = address.primarydns;
                        response["secondarydns"] = address.secondarydns;
                    }
                }
            }
            return rc;
        }

        uint32_t Network::getIPSettings (const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            JsonObject tmpResponse;

            LOG_INPARAM();
            rc = internalGetIPSettings(parameters, tmpResponse);

            if (tmpResponse.HasLabel("ipaddr") && (!tmpResponse["ipaddr"].String().empty()))
                response = tmpResponse;
            else
            {
                NMLOG_INFO("IP Address not assigned to the given interface yet");
                rc = Core::ERROR_GENERAL;
            }
            returnJson(rc);
        }

        uint32_t Network::getIPSettings2 (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            rc = internalGetIPSettings(parameters, response);

            returnJson(rc);
        }

        uint32_t Network::isConnectedToInternet(const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            LOG_INPARAM();
            string ipversion{};
            Exchange::INetworkManager::InternetStatus status{};

            if (parameters.HasLabel("ipversion"))
                ipversion = parameters["ipversion"].String();

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr != nullptr)
            {
                rc = _nwmgr->IsConnectedToInternet(ipversion, status);
                _nwmgr->Release();

                if (Core::ERROR_NONE == rc)
                {
                    response["ipversion"] = ipversion;
                    response["connectedToInternet"] = (Exchange::INetworkManager::InternetStatus::INTERNET_FULLY_CONNECTED == status);
                }
            }

            returnJson(rc);
        }

        uint32_t Network::getInternetConnectionState(const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            LOG_INPARAM();
            string ipversion{};
            Exchange::INetworkManager::InternetStatus status{};

            if (parameters.HasLabel("ipversion"))
                ipversion = parameters["ipversion"].String();

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr != nullptr)
            {
                rc = _nwmgr->IsConnectedToInternet(ipversion, status);

                if (Core::ERROR_NONE == rc)
                {
                    response["ipversion"] = ipversion;
                    response["state"] = JsonValue(status);
                    if (Exchange::INetworkManager::InternetStatus::INTERNET_CAPTIVE_PORTAL == status)
                    {
                        string uri{};
                        _nwmgr->GetCaptivePortalURI(uri);
                        response["URI"] =  uri;
                    }
                }
                _nwmgr->Release();
            }

            returnJson(rc);
        }

        uint32_t Network::doPing(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            string result{};
            string endpoint{};
            uint32_t rc = Core::ERROR_GENERAL;
            LOG_INPARAM();
            if (parameters.HasLabel("endpoint"))
            {
                string guid{};
                string ipversion{"IPv4"};
                uint32_t noOfRequest = 3;
                uint16_t timeOutInSeconds = 5;

                endpoint = parameters["endpoint"].String();

                if (parameters.HasLabel("ipversion"))
                    ipversion = parameters["ipversion"].String();

                if (parameters.HasLabel("packets"))
                    noOfRequest  = parameters["packets"].Number();

                if (parameters.HasLabel("guid"))
                    guid = parameters["guid"].String();

                auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
                if (_nwmgr)
                {
                    rc = _nwmgr->Ping(ipversion, endpoint, noOfRequest, timeOutInSeconds, guid, result);
                    _nwmgr->Release();
                }
                else
                    rc = Core::ERROR_UNAVAILABLE;
            }

            if (Core::ERROR_NONE == rc)
            {
                JsonObject reply;
                reply.FromString(result);
                response = reply;
                response["target"] = endpoint;
            }
            returnJson(rc);
        }

        uint32_t Network::doTrace(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            if (parameters.HasLabel("endpoint"))
            {
                string result{};
                string ipversion{"IPv4"};
                const string endpoint = parameters["endpoint"].String();
                const uint32_t noOfRequest  = parameters["packets"].Number();
                const string guid           = parameters["guid"].String();

                if (parameters.HasLabel("ipversion"))
                    ipversion = parameters["ipversion"].String();

                auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
                if (_nwmgr)
                {
                    rc = _nwmgr->Trace(ipversion, endpoint, noOfRequest, guid, result);
                    _nwmgr->Release();
                }
                else
                    rc = Core::ERROR_UNAVAILABLE;

                if (Core::ERROR_NONE == rc)
                {
                    JsonObject reply;
                    reply.FromString(result);
                    response = reply;
                    response["target"] = endpoint;
                }
            }
            returnJson(rc);
        }

        uint32_t Network::getPublicIP(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string ipAddress{};
            string ipversion = "IPv4";
            if (parameters.HasLabel("ipversion"))
                ipversion = parameters["ipversion"].String();

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->GetPublicIP(ipversion, ipAddress);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                response["public_ip"] = ipAddress;
            }
            returnJson(rc);
        }

        uint32_t Network::isInterfaceEnabled (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            if (parameters.HasLabel("interface"))
            {
                bool enabled;
                const string givenInterface = parameters["interface"].String();
                const string interface = getInterfaceTypeToName(givenInterface);

                if ("wlan0" != interface && "eth0" != interface)
                    return Core::ERROR_BAD_REQUEST;
            
                auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
                if (_nwmgr)
                {
                    rc = _nwmgr->GetInterfaceState(interface, enabled);
                    _nwmgr->Release();
                }
                else
                    rc = Core::ERROR_UNAVAILABLE;

                if (Core::ERROR_NONE == rc)
                    response["enabled"] = enabled;
            }
            else
                rc = Core::ERROR_BAD_REQUEST;

            returnJson(rc);
        }

        uint32_t Network::setConnectivityTestEndpoints(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            ::WPEFramework::RPC::IIteratorType<string, RPC::ID_STRINGITERATOR>* endpointsIter{};
            JsonArray array = parameters["endpoints"].Array();

            if (0 == array.Length() || 5 < array.Length())
            {
                NMLOG_DEBUG("minimum of 1 to maximum of 5 Urls are allowed");
                returnJson(rc);
            }

            std::vector<std::string> endpoints;
            JsonArray::Iterator index(array.Elements());
            while (index.Next() == true)
            {
                if (Core::JSON::Variant::type::STRING == index.Current().Content())
                {
                    endpoints.push_back(index.Current().String().c_str());
                }
                else
                {
                    NMLOG_DEBUG("Unexpected variant type");
                    returnJson(rc);
                }
            }
            endpointsIter = (Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(endpoints));

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
                rc = _nwmgr->SetConnectivityTestEndpoints(endpointsIter);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if(_nwmgr)
                _nwmgr->Release();

            if (endpointsIter)
                endpointsIter->Release();

            returnJson(rc);
        }

        uint32_t Network::startConnectivityMonitoring(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            uint32_t interval = parameters["interval"].Number();

            NMLOG_DEBUG("connectivity interval = %d", interval);
            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
                rc = _nwmgr->StartConnectivityMonitoring(interval);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if(_nwmgr)
                _nwmgr->Release();

            returnJson(rc);
        }

        uint32_t Network::getCaptivePortalURI(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string uri;

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->GetCaptivePortalURI(uri);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
                response["uri"] = uri;

            returnJson(rc);
        }

        uint32_t Network::stopConnectivityMonitoring(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->StopConnectivityMonitoring();
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t Network::getStbIp(const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            LOG_INPARAM();
            JsonObject tmpResponse;

            rc = internalGetIPSettings(parameters, tmpResponse);

            if ((Core::ERROR_NONE == rc) && tmpResponse.HasLabel("ipaddr"))
            {
                response["ip"]         = tmpResponse["ipaddr"];
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t Network::getSTBIPFamily(const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            LOG_INPARAM();
            JsonObject tmpResponse;
            JsonObject tmpParameters;

            tmpParameters["ipversion"] = parameters["family"];

            rc = internalGetIPSettings(tmpParameters, tmpResponse);

            if ((Core::ERROR_NONE == rc) && tmpResponse.HasLabel("ipaddr"))
            {
                response["ip"]         = tmpResponse["ipaddr"];
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        /** Private */
        string Network::getInterfaceNameToType(const string & interface)
        {
            if(interface == "wlan0")
                return string("WIFI");
            else if(interface == "eth0")
                return string("ETHERNET");
            return string("");
        }

        string Network::getInterfaceTypeToName(const string & interface)
        {
            if(interface == "WIFI")
                return string("wlan0");
            else if(interface == "ETHERNET")
                return string("eth0");
            return string("");
        }

        /** Event Handling and Publishing */
        void Network::onInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface)
        {
            JsonObject parameters;
            parameters["interface"] = interface;
            if (Exchange::INetworkManager::INTERFACE_ADDED == state)
                parameters["enabled"] = true;
            else if (Exchange::INetworkManager::INTERFACE_REMOVED == state)
                parameters["enabled"] = false;
            else if (Exchange::INetworkManager::INTERFACE_LINK_UP == state)
                parameters["status"] = "CONNECTED";
            else if (Exchange::INetworkManager::INTERFACE_LINK_DOWN == state)
                parameters["status"] = "DISCONNECTED";


            string json;
            parameters.ToString(json);
            if ((Exchange::INetworkManager::INTERFACE_ADDED == state) || (Exchange::INetworkManager::INTERFACE_REMOVED == state))
            {
                NMLOG_INFO("Posting onInterfaceStatusChanged as %s", json.c_str());
                Notify("onInterfaceStatusChanged", parameters);
            }
            else if ((Exchange::INetworkManager::INTERFACE_LINK_UP == state) || (Exchange::INetworkManager::INTERFACE_LINK_DOWN == state))
            {
                NMLOG_INFO("Posting onConnectionStatusChanged as %s", json.c_str());
                Notify("onConnectionStatusChanged", parameters);
            }

            return;
        }

        void Network::onActiveInterfaceChange(const string prevActiveInterface, const string currentActiveinterface)
        {
            JsonObject legacyParams;
            
            legacyParams["oldInterfaceName"] = getInterfaceNameToType(prevActiveInterface);
            legacyParams["newInterfaceName"] = getInterfaceNameToType(currentActiveinterface);


            string json;
            legacyParams.ToString(json);

            NMLOG_INFO("Posting onDefaultInterfaceChanged as %s", json.c_str());
            Notify("onDefaultInterfaceChanged", legacyParams);
            return;
        }

        void Network::onIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status)
        {
            JsonObject legacyParams;
            Core::JSON::EnumType<Exchange::INetworkManager::IPStatus> iStatus{status};

            legacyParams["interface"] = getInterfaceNameToType(interface);
            if (ipversion == "IPv6")
            {
                legacyParams["ip6Address"] = ipaddress;
            }
            else
            {
                legacyParams["ip4Address"] = ipaddress;
            }

            legacyParams["status"] = iStatus.Data();

            string json;
            legacyParams.ToString(json);
            NMLOG_INFO("Posting onIPAddressStatusChanged as %s", json.c_str());

            Notify("onIPAddressStatusChanged", legacyParams);

            return;
        }

        void Network::onInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState)
        {
            JsonObject parameters;
            Core::JSON::EnumType<Exchange::INetworkManager::InternetStatus> currStatus(currState);
            parameters["state"] = JsonValue(currState);
            parameters["status"] = currStatus.Data();

            string json;
            parameters.ToString(json);

            NMLOG_INFO("Posting onInternetStatusChange as, %s", json.c_str());
            Notify("onInternetStatusChange", parameters);
            return;
        }
    }
}

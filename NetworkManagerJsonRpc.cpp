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

#include "NetworkManager.h"
#include "INetworkManager.h"
#include "NetworkManagerJsonEnum.h"

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


using namespace NetworkManagerLogger;

namespace WPEFramework
{
    namespace Plugin
    {
        /**
         * Hook up all our JSON RPC methods
         *
         * Each method definition comprises of:
         *  * Input parameters
         *  * Output parameters
         *  * Method name
         *  * Function that implements that method
         */
        void NetworkManager::RegisterAllMethods()
        {
            Register("GetLogLevel",                       &NetworkManager::GetLogLevel, this);
            Register("SetLogLevel",                       &NetworkManager::SetLogLevel, this);
            Register("GetAvailableInterfaces",            &NetworkManager::GetAvailableInterfaces, this);
            Register("GetPrimaryInterface",               &NetworkManager::GetPrimaryInterface, this);
            Register("SetPrimaryInterface",               &NetworkManager::SetPrimaryInterface, this);
            Register("SetInterfaceState",                 &NetworkManager::SetInterfaceState, this);
            Register("GetInterfaceState",                 &NetworkManager::GetInterfaceState, this);
            Register("GetIPSettings",                     &NetworkManager::GetIPSettings, this);
            Register("SetIPSettings",                     &NetworkManager::SetIPSettings, this);
            Register("GetStunEndpoint",                   &NetworkManager::GetStunEndpoint, this);
            Register("SetStunEndpoint",                   &NetworkManager::SetStunEndpoint, this);
            Register("GetConnectivityTestEndpoints",      &NetworkManager::GetConnectivityTestEndpoints, this);
            Register("SetConnectivityTestEndpoints",      &NetworkManager::SetConnectivityTestEndpoints, this);
            Register("IsConnectedToInternet",             &NetworkManager::IsConnectedToInternet, this);
            Register("GetCaptivePortalURI",               &NetworkManager::GetCaptivePortalURI, this);
            Register("StartConnectivityMonitoring",       &NetworkManager::StartConnectivityMonitoring, this);
            Register("StopConnectivityMonitoring",        &NetworkManager::StopConnectivityMonitoring, this);
            Register("GetPublicIP",                       &NetworkManager::GetPublicIP, this);
            Register("Ping",                              &NetworkManager::Ping, this);
            Register("Trace",                             &NetworkManager::Trace, this);
            Register("StartWiFiScan",                     &NetworkManager::StartWiFiScan, this);
            Register("StopWiFiScan",                      &NetworkManager::StopWiFiScan, this);
            Register("GetKnownSSIDs",                     &NetworkManager::GetKnownSSIDs, this);
            Register("AddToKnownSSIDs",                   &NetworkManager::AddToKnownSSIDs, this);
            Register("RemoveKnownSSID",                   &NetworkManager::RemoveKnownSSID, this);
            Register("WiFiConnect",                       &NetworkManager::WiFiConnect, this);
            Register("WiFiDisconnect",                    &NetworkManager::WiFiDisconnect, this);
            Register("GetConnectedSSID",                  &NetworkManager::GetConnectedSSID, this);
            Register("StartWPS",                          &NetworkManager::StartWPS, this);
            Register("StopWPS",                           &NetworkManager::StopWPS, this);
            Register("GetWifiState",                      &NetworkManager::GetWifiState, this);
            Register("GetWiFiSignalStrength",             &NetworkManager::GetWiFiSignalStrength, this);
            Register("GetSupportedSecurityModes",         &NetworkManager::GetSupportedSecurityModes, this);
        }

        /**
         * Unregister all our JSON-RPC methods
         */
        void NetworkManager::UnregisterAllMethods()
        {
            Unregister("SetLogLevel");
            Unregister("GetAvailableInterfaces");
            Unregister("GetPrimaryInterface");
            Unregister("SetPrimaryInterface");
            Unregister("SetInterfaceState");
            Unregister("GetInterfaceState");
            Unregister("GetIPSettings");
            Unregister("SetIPSettings");
            Unregister("GetStunEndpoint");
            Unregister("SetStunEndpoint");
            Unregister("GetConnectivityTestEndpoints");
            Unregister("SetConnectivityTestEndpoints");
            Unregister("IsConnectedToInternet");
            Unregister("GetCaptivePortalURI");
            Unregister("StartConnectivityMonitoring");
            Unregister("StopConnectivityMonitoring");
            Unregister("GetPublicIP");
            Unregister("Ping");
            Unregister("Trace");
            Unregister("StartWiFiScan");
            Unregister("StopWiFiScan");
            Unregister("GetKnownSSIDs");
            Unregister("AddToKnownSSIDs");
            Unregister("RemoveKnownSSID");
            Unregister("WiFiConnect");
            Unregister("WiFiDisconnect");
            Unregister("GetConnectedSSID");
            Unregister("StartWPS");
            Unregister("StopWPS");
            Unregister("GetWifiState");
            Unregister("GetWiFiSignalStrength");
            Unregister("GetSupportedSecurityModes");
        }

        uint32_t NetworkManager::SetLogLevel (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();

            uint32_t rc = Core::ERROR_GENERAL;
            LogLevel level = INFO_LEVEL;
            if (parameters.HasLabel("level"))
            {
                level = static_cast <LogLevel> (parameters["level"].Number());

                NetworkManagerLogger::SetLevel(level);

                const Exchange::INetworkManager::Logging log = static_cast <Exchange::INetworkManager::Logging> (level);
                if (_networkManager)
                    rc = _networkManager->SetLogLevel(log);
                else
                    rc = Core::ERROR_UNAVAILABLE;
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::GetLogLevel (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();

            uint32_t rc = Core::ERROR_NONE;
            LogLevel level = INFO_LEVEL;
            NetworkManagerLogger::GetLevel(level);
            response["level"] = static_cast <uint8_t>(level);

            returnJson(rc);
        }

        uint32_t NetworkManager::GetAvailableInterfaces (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            Exchange::INetworkManager::IInterfaceDetailsIterator* _interfaces{};

            if (_networkManager)
                rc = _networkManager->GetAvailableInterfaces(_interfaces);
            else
                rc = Core::ERROR_UNAVAILABLE;

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
                        each["type"] = type.Data();
                        each["name"] = entry.name;
                        each["mac"] = entry.mac;
                        each["enabled"] = entry.enabled;
                        each["connected"] = entry.connected;

                        array.Add(JsonValue(each));
                    }

                    _interfaces->Release();
                    response["interfaces"] = array;
                }
            }

            returnJson(rc);
        }

        uint32_t NetworkManager::GetPrimaryInterface (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string interface;

            if (_networkManager)
                rc = _networkManager->GetPrimaryInterface(interface);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                response["interface"] = interface;      
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::SetPrimaryInterface (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string interface = parameters["interface"].String();

            if ("wlan0" != interface && "eth0" != interface)
            {
                rc = Core::ERROR_BAD_REQUEST;
                return rc;
            }

            if (_networkManager)
                rc = _networkManager->SetPrimaryInterface(interface);
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::SetInterfaceState(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            if (parameters.HasLabel("interface") && parameters.HasLabel("enabled"))
            {
                const string interface = parameters["interface"].String();
                const bool enabled = parameters["enabled"].Boolean();

                if ("wlan0" != interface && "eth0" != interface)
                    rc = Core::ERROR_BAD_REQUEST;
                else if (_networkManager)
                    rc = _networkManager->SetInterfaceState(interface, enabled);
                else
                    rc = Core::ERROR_UNAVAILABLE;
            }
            else
                rc = Core::ERROR_BAD_REQUEST;

            returnJson(rc);
        }

        uint32_t NetworkManager::GetInterfaceState(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            if (parameters.HasLabel("interface"))
            {
                const string interface = parameters["interface"].String();
                bool enabled;

                if ("wlan0" != interface && "eth0" != interface)
                    rc = Core::ERROR_BAD_REQUEST;
                else if (_networkManager)
                    rc = _networkManager->SetInterfaceState(interface, enabled);
                else
                    rc = Core::ERROR_UNAVAILABLE;

                if (Core::ERROR_NONE == rc)
                    response["enabled"] = enabled;
            }
            else
                rc = Core::ERROR_BAD_REQUEST;

            returnJson(rc);
        }

        uint32_t NetworkManager::GetIPSettings (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::IPAddress address{};

            string interface = parameters["interface"].String();
            string ipversion = parameters["ipversion"].String();

            if (_networkManager)
                rc = _networkManager->GetIPSettings(interface, ipversion, address);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                response["interface"]    = interface;
                response["ipversion"]    = address.ipversion;
                response["autoconfig"]   = address.autoconfig;
                if (!address.ipaddress.empty())
                {
                    response["ipaddress"]    = address.ipaddress;
                    response["prefix"]       = address.prefix;
                    response["ula"]          = address.ula;
                    response["dhcpserver"]   = address.dhcpserver;
                    response["gateway"]      = address.gateway;
                    response["primarydns"]   = address.primarydns;
                    response["secondarydns"] = address.secondarydns;
                }
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::SetIPSettings(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::IPAddress address{};

            string interface = "";
            string ipversion = "";

            if (parameters.HasLabel("interface"))
                interface = parameters["interface"].String();
            else
            {
                rc = Core::ERROR_BAD_REQUEST;
                return rc;
            }

            address.autoconfig = parameters["autoconfig"].Boolean();
            if (!address.autoconfig)
            {
                address.ipaddress      = parameters["ipaddress"].String();
                address.ipversion      = parameters["ipversion"].String();
                address.prefix         = parameters["prefix"].Number();
                address.gateway        = parameters["gateway"].String();
                address.primarydns     = parameters["primarydns"].String();
                address.secondarydns   = parameters["secondarydns"].String();
            }

            if (_networkManager)
                rc = _networkManager->SetIPSettings(interface, address);
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::GetStunEndpoint(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string endPoint;
            uint32_t port;
            uint32_t timeout;
            uint32_t cacheLifetime;

            if (_networkManager)
                rc = _networkManager->GetStunEndpoint(endPoint, port, timeout, cacheLifetime);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                response["endpoint"] = endPoint;
                response["port"] = port;
                response["timeout"] = timeout;
                response["cacheLifetime"] = cacheLifetime;
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::SetStunEndpoint(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string endPoint = parameters["endpoint"].String();
            uint32_t port = parameters["port"].Number();
            uint32_t bindTimeout = parameters["timeout"].Number();
            uint32_t cacheTimeout = parameters["cacheLifetime"].Number();

            if (_networkManager)
                rc = _networkManager->SetStunEndpoint(endPoint, port, bindTimeout, cacheTimeout);
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::GetConnectivityTestEndpoints(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::IStringIterator* endpoints = NULL;

            if (_networkManager)
                rc = _networkManager->GetConnectivityTestEndpoints(endpoints);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                if (endpoints)
                {
                    JsonArray array;
                    string entry{};
                    while (endpoints->Next(entry) == true) { array.Add(entry); }

                    endpoints->Release();
                    response["endpoints"] = array;
                }
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::SetConnectivityTestEndpoints(const JsonObject& parameters, JsonObject& response)
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

            if (_networkManager)
                rc = _networkManager->SetConnectivityTestEndpoints(endpointsIter);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (endpointsIter)
                endpointsIter->Release();

            returnJson(rc);
        }

        uint32_t NetworkManager::IsConnectedToInternet(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string ipversion = parameters["ipversion"].String();
            Exchange::INetworkManager::InternetStatus result;
            

            if (_networkManager)
                rc = _networkManager->IsConnectedToInternet(ipversion, result);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                Core::JSON::EnumType<Exchange::INetworkManager::InternetStatus> status(result);
                response["ipversion"] = ipversion;
                response["connected"] = (Exchange::INetworkManager::InternetStatus::INTERNET_FULLY_CONNECTED == result);
                response["state"] = JsonValue(status);
                response["status"] = status.Data();
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::GetCaptivePortalURI(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string uri;
            if (_networkManager)
                rc = _networkManager->GetCaptivePortalURI(uri);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
                response["uri"] = uri;

            returnJson(rc);
        }

        uint32_t NetworkManager::StartConnectivityMonitoring(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            uint32_t interval = parameters["interval"].Number();

            NMLOG_DEBUG("connectivity interval = %d", interval);
            if (_networkManager)
                rc = _networkManager->StartConnectivityMonitoring(interval);
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::StopConnectivityMonitoring(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            if (_networkManager)
                rc = _networkManager->StopConnectivityMonitoring();
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::GetPublicIP(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string ipAddress{};
            string ipversion = "IPv4";
            if (parameters.HasLabel("ipversion"))
                ipversion = parameters["ipversion"].String();

            if ((!m_publicIPAddress.empty()) && (m_publicIPAddressType == ipversion))
            {
                rc = Core::ERROR_NONE;
                ipAddress = m_publicIPAddress;
            }
            else
            {
                if (_networkManager)
                    rc = _networkManager->GetPublicIP(ipversion, ipAddress);
                else
                    rc = Core::ERROR_UNAVAILABLE;
            }

            if (Core::ERROR_NONE == rc)
            {
                response["ipaddress"] = ipAddress;
                response["ipversion"] = ipversion;

                m_publicIPAddress = ipAddress;
                m_publicIPAddressType = ipversion;
                PublishToThunderAboutInternet();
            }
            returnJson(rc);
        }

        void NetworkManager::PublishToThunderAboutInternet()
        {
            NMLOG_DEBUG("No public IP persisted yet; Update the data");
            if (m_publicIPAddress.empty())
            {
                JsonObject input, output;
                GetPublicIP(input, output);
            }

            if (!m_publicIPAddress.empty())
            {
                PluginHost::ISubSystem* subSystem = _service->SubSystems();

                if (subSystem != nullptr)
                {
                    const PluginHost::ISubSystem::IInternet* internet(subSystem->Get<PluginHost::ISubSystem::IInternet>());
                    if (nullptr == internet)
                    {
                        subSystem->Set(PluginHost::ISubSystem::INTERNET, this);
                        NMLOG_INFO("Set INTERNET ISubSystem");
                    }

                    subSystem->Release();
                }
            }
        }

        uint32_t NetworkManager::Ping(const JsonObject& parameters, JsonObject& response)
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

                if (parameters.HasLabel("count"))
                    noOfRequest  = parameters["count"].Number();

                if (parameters.HasLabel("timeout"))
                    timeOutInSeconds  = parameters["timeout"].Number();

                if (parameters.HasLabel("guid"))
                    guid = parameters["guid"].String();

                if (_networkManager)
                    rc = _networkManager->Ping(ipversion, endpoint, noOfRequest, timeOutInSeconds, guid, result);
                else
                    rc = Core::ERROR_UNAVAILABLE;
            }

            if (Core::ERROR_NONE == rc)
            {
                JsonObject reply;
                reply.FromString(result);
                response = reply;
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::Trace(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string result{};
        
            if (parameters.HasLabel("endpoint"))
            {
                const string ipversion      = parameters["ipversion"].String();
                const string endpoint       = parameters["endpoint"].String();
                const uint32_t noOfRequest  = parameters["packets"].Number();
                const string guid           = parameters["guid"].String();

                if (_networkManager)
                    rc = _networkManager->Trace(ipversion, endpoint, noOfRequest, guid, result);
                else
                    rc = Core::ERROR_UNAVAILABLE;

                if (Core::ERROR_NONE == rc)
                {
                    JsonObject reply;
                    reply.FromString(result);
                    response = reply;
                }
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::StartWiFiScan(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string frequency = parameters["frequency"].String();
            Exchange::INetworkManager::IStringIterator* ssids = NULL;

            if (_networkManager)
                rc = _networkManager->StartWiFiScan(frequency, ssids);
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::StopWiFiScan(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            if (_networkManager)
                rc = _networkManager->StopWiFiScan();
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::GetKnownSSIDs(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            ::WPEFramework::RPC::IIteratorType<string, RPC::ID_STRINGITERATOR>* _ssids{};

            if (_networkManager)
                rc = _networkManager->GetKnownSSIDs(_ssids);

            if (Core::ERROR_NONE == rc)
            {
                if (_ssids != nullptr)
                {
                    JsonArray ssids;
                    string _resultItem_{};
                    while (_ssids->Next(_resultItem_) == true) { ssids.Add() = _resultItem_; }
                    _ssids->Release();

                    response["ssids"] = ssids;
                }
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::AddToKnownSSIDs(const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::WiFiConnectTo ssid{};
            NMLOG_INFO("Entry to %s\n", __FUNCTION__);

            if (parameters.HasLabel("ssid") && parameters.HasLabel("passphrase"))
            {
                ssid.ssid            = parameters["ssid"].String();
                ssid.passphrase      = parameters["passphrase"].String();
                ssid.security        = static_cast <Exchange::INetworkManager::WIFISecurityMode> (parameters["security"].Number());

                if (_networkManager)
                    rc = _networkManager->AddToKnownSSIDs(ssid);
                else
                    rc = Core::ERROR_UNAVAILABLE;
            }

            returnJson(rc);
        }

        uint32_t NetworkManager::RemoveKnownSSID(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string ssid{};

            if (parameters.HasLabel("ssid"))
            {
                ssid = parameters["ssid"].String();
                if (_networkManager)
                    rc = _networkManager->RemoveKnownSSID(ssid);
                else
                    rc = Core::ERROR_UNAVAILABLE;
            }
            else
                rc = Core::ERROR_BAD_REQUEST;

            returnJson(rc);
        }

        uint32_t NetworkManager::WiFiConnect(const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::WiFiConnectTo ssid{};
            NMLOG_INFO("Entry to %s\n", __FUNCTION__);

            if (parameters.HasLabel("ssid"))
                ssid.ssid = parameters["ssid"].String();
            else
                returnJson(rc);

            if (parameters.HasLabel("passphrase"))
                ssid.passphrase = parameters["passphrase"].String();

            if (parameters.HasLabel("security"))
                ssid.security= static_cast <Exchange::INetworkManager::WIFISecurityMode> (parameters["security"].Number());

            // Check Security modes
            if (parameters.HasLabel("eap"))
                ssid.eap = parameters["eap"].String();
            if (parameters.HasLabel("eap_identity"))
                ssid.eap_identity = parameters["eap_identity"].String();
            if (parameters.HasLabel("ca_cert"))
                ssid.ca_cert = parameters["ca_cert"].String();
            if (parameters.HasLabel("client_cert"))
                ssid.client_cert = parameters["client_cert"].String();
            if (parameters.HasLabel("private_key"))
                ssid.private_key = parameters["private_key"].String();
            if (parameters.HasLabel("private_key_passwd"))
                ssid.private_key_passwd = parameters["private_key_passwd"].String();
            if (parameters.HasLabel("persist"))
                ssid.persist = parameters["persist"].Boolean();
            else
                ssid.persist = true;

            if (_networkManager)
                rc = _networkManager->WiFiConnect(ssid);
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::WiFiDisconnect(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            if (_networkManager)
                rc = _networkManager->WiFiDisconnect();
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::GetConnectedSSID(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::WiFiSSIDInfo ssidInfo{};

            if (_networkManager)
                rc = _networkManager->GetConnectedSSID(ssidInfo);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                response["ssid"] = ssidInfo.ssid;
                response["bssid"] = ssidInfo.bssid;
                response["security"] = JsonValue(ssidInfo.security);
                response["strength"] = ssidInfo.strength;
                response["frequency"] = ssidInfo.frequency;
                response["rate"] = ssidInfo.rate;
                response["noise"] = ssidInfo.noise;
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::StartWPS(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string wps_pin{};
            Core::JSON::EnumType<Exchange::INetworkManager::WiFiWPS> method;

            if (parameters.HasLabel("method"))
            {
                if (parameters["method"].Content() == WPEFramework::Core::JSON::Variant::type::STRING)
                    method.FromString(parameters["method"].String());
                else if (parameters["method"].Content() == WPEFramework::Core::JSON::Variant::type::NUMBER)
                    method = static_cast <Exchange::INetworkManager::WiFiWPS> (parameters["method"].Number());

                if ((Exchange::INetworkManager::WIFI_WPS_PIN == method) && parameters.HasLabel("pin"))
                {
                    wps_pin = parameters["pin"].String();
                }
            }

            if (_networkManager)
                rc = _networkManager->StartWPS(method, wps_pin);
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::StopWPS(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            if (_networkManager)
                rc = _networkManager->StopWPS();
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t NetworkManager::GetWifiState(const JsonObject& parameters, JsonObject& response)
        {
            Exchange::INetworkManager::WiFiState state;
            uint32_t rc = Core::ERROR_GENERAL;

            LOG_INPARAM();
            if (_networkManager)
                rc = _networkManager->GetWifiState(state);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                Core::JSON::EnumType<Exchange::INetworkManager::WiFiState> iState{state};
                response["state"] = JsonValue(state);
                response["status"] = iState.Data();
            }

            returnJson(rc);
        }

        uint32_t NetworkManager::GetWiFiSignalStrength(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string ssid{};
            string signalStrength{};
            Exchange::INetworkManager::WiFiSignalQuality quality{};

            if (_networkManager)
                rc = _networkManager->GetWiFiSignalStrength(ssid, signalStrength, quality);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                Core::JSON::EnumType<Exchange::INetworkManager::WiFiSignalQuality> iquality(quality);
                response["ssid"] = ssid;
                response["strength"] = signalStrength;
                response["quality"] = iquality.Data();
            }
            returnJson(rc);
        }

        uint32_t NetworkManager::GetSupportedSecurityModes(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::ISecurityModeIterator* securityModes{};

            if (_networkManager)
                rc = _networkManager->GetSupportedSecurityModes(securityModes);
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                if (securityModes != nullptr)
                {
                    JsonObject modes{};
                    Exchange::INetworkManager::WIFISecurityModeInfo _resultItem_{};
                    while (securityModes->Next(_resultItem_) == true)
                    {
                        response.Set(_resultItem_.securityName.c_str(), JsonValue(_resultItem_.security));
                    }
                    securityModes->Release();
                }
            }
            returnJson(rc);
        }

        void NetworkManager::onInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface)
        {
            Core::JSON::EnumType<Exchange::INetworkManager::InterfaceState> iState{state};
            JsonObject parameters;
            parameters["state"] = JsonValue(state);
            parameters["status"] = iState.Data();
            parameters["interface"] = interface;

            LOG_INPARAM();
            Notify(_T("onInterfaceStateChange"), parameters);
        }

        void NetworkManager::onActiveInterfaceChange(const string prevActiveInterface, const string currentActiveinterface)
        {
            JsonObject parameters;
            parameters["prevActiveInterface"] = prevActiveInterface;
            parameters["currentActiveInterface"] = currentActiveinterface;

            LOG_INPARAM();
            Notify(_T("onActiveInterfaceChange"), parameters);
        }

        void NetworkManager::onIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status)
        {
            Core::JSON::EnumType<Exchange::INetworkManager::IPStatus> iStatus{status};
            JsonObject parameters;
            parameters["interface"] = interface;
            parameters["ipversion"] = ipversion;
            parameters["ipaddress"] = ipaddress;
            parameters["status"] = iStatus.Data();

            LOG_INPARAM();
            Notify(_T("onIPAddressChange"), parameters);
        }

        void NetworkManager::onInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState)
        {
            JsonObject parameters;
            Core::JSON::EnumType<Exchange::INetworkManager::InternetStatus> prevStatus(prevState);
            Core::JSON::EnumType<Exchange::INetworkManager::InternetStatus> currStatus(currState);
            parameters["prevState"] = JsonValue(prevState);
            parameters["prevStatus"] = prevStatus.Data();
            parameters["state"] = JsonValue(currState);
            parameters["status"] = currStatus.Data();

            LOG_INPARAM();
            Notify(_T("onInternetStatusChange"), parameters);
        }

        void NetworkManager::onAvailableSSIDs(const string jsonOfScanResults)
        {
            JsonObject parameters;
            JsonArray scanResults;
            scanResults.FromString(jsonOfScanResults);
            parameters["ssids"] = scanResults;

            LOG_INPARAM();
            Notify(_T("onAvailableSSIDs"), parameters);
        }

        void NetworkManager::onWiFiStateChange(const Exchange::INetworkManager::WiFiState state)
        {
            JsonObject parameters;
            Core::JSON::EnumType<Exchange::INetworkManager::WiFiState> iState{state};
            parameters["state"] = JsonValue(state);
            parameters["status"] = iState.Data();

            LOG_INPARAM();
            Notify(_T("onWiFiStateChange"), parameters);
        }

        void NetworkManager::onWiFiSignalStrengthChange(const string ssid, const string strength, const Exchange::INetworkManager::WiFiSignalQuality quality)
        {
            Core::JSON::EnumType<Exchange::INetworkManager::WiFiSignalQuality> iquality(quality);
            JsonObject parameters;
            parameters["ssid"] = ssid;
            parameters["strength"] = strength;
            parameters["quality"] = iquality.Data();

            LOG_INPARAM();
            Notify(_T("onWiFiSignalStrengthChange"), parameters);
        }
    }
}

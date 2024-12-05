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
#include "LegacyPlugin_WiFiManagerAPIs.h"
#include "NetworkManagerLogger.h"
#include "NetworkManagerJsonEnum.h"

using namespace std;
using namespace WPEFramework::Plugin;
#define API_VERSION_NUMBER_MAJOR 2
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0
#define NETWORK_MANAGER_CALLSIGN    "org.rdk.NetworkManager.1"
#define SUBSCRIPTION_TIMEOUT_IN_MILLISECONDS 500

#define LOG_INPARAM() { string json; parameters.ToString(json); NMLOG_INFO("params=%s", json.c_str() ); }
#define LOG_OUTPARAM() { string json; response.ToString(json); NMLOG_INFO("response=%s", json.c_str() ); }

#define returnJson(rc) \
    { \
        if (Core::ERROR_NONE == rc)                 \
            response["success"] = true;             \
        else                                        \
            response["success"] = false;            \
        LOG_OUTPARAM();                             \
        return rc;                                  \
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

    static Plugin::Metadata<Plugin::WiFiManager> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
    );

    WiFiManager* _gWiFiInstance = nullptr;
    namespace Plugin
    {
        SERVICE_REGISTRATION(WiFiManager, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        WiFiManager::WiFiManager()
        : PluginHost::JSONRPC()
        , m_service(nullptr)
        , m_subsWiFiStateChange(false)
        , m_subsAvailableSSIDs(false)
        , m_subsWiFiStrengthChange(false)
       {
           _gWiFiInstance = this;
           m_timer.connect(std::bind(&WiFiManager::subscribeToEvents, this));
           registerLegacyMethods();
       }

        WiFiManager::~WiFiManager()
        {
            _gWiFiInstance = nullptr;
        }

        void WiFiManager::activatePrimaryPlugin()
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

        const string WiFiManager::Initialize(PluginHost::IShell*  service )
        {
            m_service = service;
            m_service->AddRef();

            string callsign(NETWORK_MANAGER_CALLSIGN);
            string token = "";

            // TODO: use interfaces and remove token
            auto security = m_service->QueryInterfaceByCallsign<PluginHost::IAuthenticate>("SecurityAgent");
            if (security != nullptr) {
                string payload = "http://localhost";
                if (security->CreateToken(
                            static_cast<uint16_t>(payload.length()),
                            reinterpret_cast<const uint8_t*>(payload.c_str()),
                            token)
                        == Core::ERROR_NONE) {
                    NMLOG_DEBUG("WiFi manager plugin got security token");
                } else {
                    NMLOG_WARNING("WiFi manager plugin failed to get security token");
                }
                security->Release();
            } else {
                NMLOG_INFO("WiFi manager plugin: No security agent");
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
        
            Core::SystemInfo::SetEnvironment(_T("THUNDER_ACCESS"), (_T("127.0.0.1:9998")));
            m_networkmanager = make_shared<WPEFramework::JSONRPC::SmartLinkType<WPEFramework::Core::JSON::IElement> >(_T(NETWORK_MANAGER_CALLSIGN), _T("org.rdk.Wifi"), query);

            subscribeToEvents();
            return string();
        }

        void WiFiManager::Deinitialize(PluginHost::IShell* /* service */)
        {
            unregisterLegacyMethods();
            m_service->Release();
            m_service = nullptr;
            _gWiFiInstance = nullptr;
        }

        string WiFiManager::Information() const
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
        void WiFiManager::registerLegacyMethods(void)
        {
            CreateHandler({2});
            Register("cancelWPSPairing",                  &WiFiManager::cancelWPSPairing, this);
            Register("clearSSID",                         &WiFiManager::clearSSID, this);
            Register("connect",                           &WiFiManager::connect, this);
            Register("disconnect",                        &WiFiManager::disconnect, this);
            Register("getConnectedSSID",                  &WiFiManager::getConnectedSSID, this);
            Register("getCurrentState",                   &WiFiManager::getCurrentState, this);
            Register("getPairedSSID",                     &WiFiManager::getPairedSSID, this);
            Register("getPairedSSIDInfo",                 &WiFiManager::getPairedSSIDInfo, this);
            Register("getSupportedSecurityModes",         &WiFiManager::getSupportedSecurityModes, this);
            Register("initiateWPSPairing",                &WiFiManager::initiateWPSPairing, this);
            GetHandler(2)->Register<JsonObject, JsonObject>("initiateWPSPairing", &WiFiManager::initiateWPSPairing, this);
            Register("isPaired",                          &WiFiManager::isPaired, this);
            Register("saveSSID",                          &WiFiManager::saveSSID, this);
            Register("startScan",                         &WiFiManager::startScan, this);
            Register("stopScan",                          &WiFiManager::stopScan, this);
        }

        /**
         * Unregister all our JSON-RPC methods
         */
        void WiFiManager::unregisterLegacyMethods(void)
        {
            Unregister("cancelWPSPairing");
            Unregister("clearSSID");
            Unregister("connect");
            Unregister("disconnect");
            Unregister("getConnectedSSID");
            Unregister("getCurrentState");
            Unregister("getPairedSSID");
            Unregister("getPairedSSIDInfo");
            Unregister("getSupportedSecurityModes");
            Unregister("initiateWPSPairing");
            Unregister("isPaired");
            Unregister("saveSSID");
            Unregister("startScan");
            Unregister("stopScan");
        }

        uint32_t WiFiManager::cancelWPSPairing (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->StopWPS();
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
                response["result"] = string();

            returnJson(rc);
        }

        uint32_t WiFiManager::clearSSID (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string ssid{};

            ssid = parameters["ssid"].String();
            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
               rc = _nwmgr->RemoveKnownSSID(ssid);
               _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
                response["result"] = 0;

            returnJson(rc);
        }
 
        uint32_t WiFiManager::connect(const JsonObject& parameters, JsonObject& response)
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

            if (parameters.HasLabel("securityMode"))
                ssid.security= static_cast <Exchange::INetworkManager::WIFISecurityMode> (parameters["securityMode"].Number());

            ssid.persist = true;

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->WiFiConnect(ssid);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        uint32_t WiFiManager::getConnectedSSID (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::WiFiSSIDInfo ssidInfo{};

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->GetConnectedSSID(ssidInfo);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                response["ssid"] = ssidInfo.ssid;
                response["bssid"] = ssidInfo.bssid;
                response["rate"] = ssidInfo.rate;
                response["noise"] = ssidInfo.noise;
                response["security"] = JsonValue(ssidInfo.security);
                response["signalStrength"] = ssidInfo.strength;
                response["frequency"] = ssidInfo.frequency;
            }
            returnJson(rc);
        }

        uint32_t WiFiManager::getCurrentState(const JsonObject& parameters, JsonObject& response)
        {
            Exchange::INetworkManager::WiFiState state;
            uint32_t rc = Core::ERROR_GENERAL;

            LOG_INPARAM();
            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->GetWifiState(state);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                /* Legacy Enums Mapping */
                if (state >= Exchange::INetworkManager::WiFiState::WIFI_STATE_ERROR)
                    response["state"] = 6; // 6: FAILED - The device has encountered an unrecoverable error with the Wifi adapter.
                else if (state > Exchange::INetworkManager::WiFiState::WIFI_STATE_CONNECTED)
                    response["state"] = 2; // 2: DISCONNECTED - The device is installed and enabled, but not yet connected to a network
                else
                    response["state"] = JsonValue(state);
            }

            returnJson(rc);
        }

        uint32_t WiFiManager::getPairedSSID(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            ::WPEFramework::RPC::IIteratorType<string, RPC::ID_STRINGITERATOR>* _ssids{};

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->GetKnownSSIDs(_ssids);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                if (_ssids != nullptr)
                {
                    string _resultItem_{};
                    while (_ssids->Next(_resultItem_) == true) {
                        response["ssid"] = _resultItem_;
                        /* Just take one Entry : 1st Entry */
                        break;
                    }
                    _ssids->Release();
                }
            }
            returnJson(rc);
        }

        uint32_t WiFiManager::getPairedSSIDInfo(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::WiFiSSIDInfo ssidInfo{};

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->GetConnectedSSID(ssidInfo);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
            {
                response["ssid"] = ssidInfo.ssid;
                response["bssid"] = ssidInfo.bssid;
            }
            returnJson(rc);
        }

        uint32_t WiFiManager::getSupportedSecurityModes(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::ISecurityModeIterator* securityModes{};

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->GetSupportedSecurityModes(securityModes);
                _nwmgr->Release();
            }
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

        uint32_t WiFiManager::isPaired (const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            LOG_INPARAM();
            JsonObject tmpResponse;

            rc = getPairedSSID(parameters, tmpResponse);

            if (Core::ERROR_NONE == rc)
            {
                if (tmpResponse.HasLabel("ssid"))
                    response["result"] = 1;
                else
                    response["result"] = 0;
            }
            returnJson(rc);
        }

        uint32_t WiFiManager::saveSSID (const JsonObject& parameters, JsonObject& response)
        {
            uint32_t rc = Core::ERROR_GENERAL;
            Exchange::INetworkManager::WiFiConnectTo ssid{};
            NMLOG_INFO("Entry to %s\n", __FUNCTION__);

            if (parameters.HasLabel("ssid") && parameters.HasLabel("passphrase"))
            {
                ssid.ssid            = parameters["ssid"].String();
                ssid.passphrase      = parameters["passphrase"].String();
                ssid.security        = static_cast <Exchange::INetworkManager::WIFISecurityMode> (parameters["security"].Number());

                auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
                if (_nwmgr)
                {
                    rc = _nwmgr->AddToKnownSSIDs(ssid);
                    _nwmgr->Release();
                }
                else
                    rc = Core::ERROR_UNAVAILABLE;
            }

            if (Core::ERROR_NONE == rc)
                response["result"] = 0;

            returnJson(rc);
        }

        uint32_t WiFiManager::disconnect (const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->WiFiDisconnect();
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
                response["result"] = 0;

            returnJson(rc);
        }

        uint32_t WiFiManager::initiateWPSPairing (const JsonObject& parameters, JsonObject& response)
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

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->StartWPS(method, wps_pin);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (Core::ERROR_NONE == rc)
                response["result"] = 0;
            else
                response["result"] = 1;
            returnJson(rc);
        }

	uint32_t WiFiManager::startScan(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;
            string frequency{};
            Exchange::INetworkManager::IStringIterator* ssids = NULL;


            if (parameters.HasLabel("frequency"))
                frequency = parameters["frequency"].String();

            if (parameters.HasLabel("ssid"))
            {
                string inputSSID = parameters["ssid"].String();
                string ssid{};
                vector<string> inputSSIDlist;
                stringstream ssidStream(inputSSID);
                while (getline(ssidStream, ssid, '|'))
                {
                    inputSSIDlist.push_back(ssid);
                }

                ssids = (Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(inputSSIDlist));
            }

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->StartWiFiScan(frequency, ssids);
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            if (ssids)
                ssids->Release();

            returnJson(rc);
        }

        uint32_t WiFiManager::stopScan(const JsonObject& parameters, JsonObject& response)
        {
            LOG_INPARAM();
            uint32_t rc = Core::ERROR_GENERAL;

            auto _nwmgr = m_service->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORK_MANAGER_CALLSIGN);
            if (_nwmgr)
            {
                rc = _nwmgr->StopWiFiScan();
                _nwmgr->Release();
            }
            else
                rc = Core::ERROR_UNAVAILABLE;

            returnJson(rc);
        }

        /** Private */
        void WiFiManager::subscribeToEvents(void)
        {
            uint32_t errCode = Core::ERROR_GENERAL;
            if (m_networkmanager)
            {
                if (!m_subsWiFiStateChange)
                {
                    errCode = m_networkmanager->Subscribe<JsonObject>(5000, _T("onWiFiStateChange"), &WiFiManager::onWiFiStateChange);
                    if (Core::ERROR_NONE == errCode)
                        m_subsWiFiStateChange = true;
                    else
                        NMLOG_ERROR ("Subscribe to onWiFiStateChange failed, errCode: %u", errCode);
                }

                if (!m_subsAvailableSSIDs)
                {
                    errCode = m_networkmanager->Subscribe<JsonObject>(5000, _T("onAvailableSSIDs"), &WiFiManager::onAvailableSSIDs);
                    if (Core::ERROR_NONE == errCode)
                        m_subsAvailableSSIDs = true;
                    else
                        NMLOG_ERROR("Subscribe to onAvailableSSIDs failed, errCode: %u", errCode);
                }

                if (!m_subsWiFiStrengthChange)
                {
                    errCode = m_networkmanager->Subscribe<JsonObject>(5000, _T("onWiFiSignalStrengthChange"), &WiFiManager::onWiFiSignalStrengthChange);
                    if (Core::ERROR_NONE == errCode)
                        m_subsWiFiStrengthChange = true;
                    else
                        NMLOG_ERROR("Subscribe to onWiFiSignalStrengthChange failed, errCode: %u", errCode);
                }
            }
            else
                NMLOG_ERROR("m_networkmanager is null");

            if (m_subsWiFiStateChange && m_subsAvailableSSIDs && m_subsWiFiStrengthChange)
            {
                m_timer.stop();
                NMLOG_INFO("All the required events are subscribed; Retry timer stoped");
            }
            else
            {
                m_timer.start(SUBSCRIPTION_TIMEOUT_IN_MILLISECONDS);
                NMLOG_INFO("Few required events are yet to be subscribed; Retry timer started");
            }
        }

        bool WiFiManager::ErrorCodeMapping(const uint32_t ipvalue, uint32_t &opvalue)
        {
            bool ret = true;

            switch (ipvalue)
            {
                case Exchange::INetworkManager::WIFI_STATE_SSID_CHANGED:
                    opvalue = WIFI_SSID_CHANGED;
                    break;
                case Exchange::INetworkManager::WIFI_STATE_CONNECTION_LOST:
                    opvalue = WIFI_CONNECTION_LOST;
                    break;
                case Exchange::INetworkManager::WIFI_STATE_CONNECTION_FAILED:
                    opvalue = WIFI_CONNECTION_FAILED;
                    break;
                case Exchange::INetworkManager::WIFI_STATE_CONNECTION_INTERRUPTED:
                    opvalue = WIFI_CONNECTION_INTERRUPTED;
                    break;
                case Exchange::INetworkManager::WIFI_STATE_INVALID_CREDENTIALS:
                    opvalue = WIFI_INVALID_CREDENTIALS;
                    break;
                case Exchange::INetworkManager::WIFI_STATE_SSID_NOT_FOUND:
                    opvalue = WIFI_NO_SSID;
                    break;
                case Exchange::INetworkManager::WIFI_STATE_ERROR:
                    opvalue = WIFI_UNKNOWN;
                    break;
                case Exchange::INetworkManager::WIFI_STATE_AUTHENTICATION_FAILED:
                    opvalue = WIFI_AUTH_FAILED;
                    break;
                default:
                    ret = false;
                    break;
            }
            return ret;
        }

        /** Event Handling and Publishing */
        void WiFiManager::onWiFiStateChange(const JsonObject& parameters)
        {
            JsonObject legacyResult;
            JsonObject legacyErrorResult;
            string json;
            uint32_t errorCode;
            uint32_t state = parameters["state"].Number();

            legacyResult["state"] = parameters["state"];
            legacyResult["isLNF"] = false;

            if(_gWiFiInstance)
            {
                if(ErrorCodeMapping(state, errorCode))
                {
                    legacyErrorResult["code"] = errorCode;
                    NMLOG_INFO("onError with errorcode as, %u",  errorCode);

                    legacyErrorResult.ToString(json);
                    NMLOG_INFO("Posting onError as %s", json.c_str());

                    _gWiFiInstance->Notify("onError", legacyErrorResult);
                }
                else
                {
                    NMLOG_INFO("onWiFiStateChange with state as: %u", state);

                    legacyResult.ToString(json);
                    NMLOG_INFO("Posting onWIFIStateChanged as %s", json.c_str());
                    _gWiFiInstance->Notify("onWIFIStateChanged", legacyResult);
                }
            }
            else
                NMLOG_WARNING("Ignoring %s", __FUNCTION__);

            return;
        }

        void WiFiManager::onAvailableSSIDs(const JsonObject& parameters)
        {
            string json;
            parameters.ToString(json);
            NMLOG_INFO("Posting onAvailableSSIDs Event as %s", json.c_str());
            if(_gWiFiInstance)
                _gWiFiInstance->Notify("onAvailableSSIDs", parameters);
            else
                NMLOG_WARNING("Ignoring %s", __FUNCTION__);

            return;
        }

        void WiFiManager::onWiFiSignalStrengthChange(const JsonObject& parameters)
        {
            JsonObject legacyParams;
            legacyParams["signalStrength"] = parameters["strength"];
            legacyParams["strength"] = parameters["quality"];

            string json;
            legacyParams.ToString(json);
            NMLOG_INFO("Posting onWifiSignalThresholdChanged as %s", json.c_str());
            if (_gWiFiInstance)
                _gWiFiInstance->Notify("onWifiSignalThresholdChanged", legacyParams);
            else
                NMLOG_WARNING("Ignoring %s", __FUNCTION__);

            return;
        }
    }
}

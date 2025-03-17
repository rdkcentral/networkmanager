#ifndef SERVICEMOCK_H
#define SERVICEMOCK_H

#include <gmock/gmock.h>

#include "Module.h"

class ServiceMock : public WPEFramework::PluginHost::IShell {
public:
    virtual ~ServiceMock() = default;

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(string, Versions, (), (const, override));
    MOCK_METHOD(string, Locator, (), (const, override));
    MOCK_METHOD(string, ClassName, (), (const, override));
    MOCK_METHOD(string, Callsign, (), (const, override));
    MOCK_METHOD(string, WebPrefix, (), (const, override));
    MOCK_METHOD(string, ConfigLine, (), (const, override));
    MOCK_METHOD(string, PersistentPath, (), (const, override));
    MOCK_METHOD(string, VolatilePath, (), (const, override));
    MOCK_METHOD(string, DataPath, (), (const, override));
    MOCK_METHOD(state, State, (), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, Activate, (const reason), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Deactivate, (const reason), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Unavailable, (const reason), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, ConfigLine, (const string& config), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SystemRootPath, (const string& systemRootPath), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Hibernate, (const uint32_t timeout), (override));
    MOCK_METHOD(string, SystemPath, (), (const, override));
    MOCK_METHOD(string, PluginPath, (), (const, override));
    MOCK_METHOD(WPEFramework::PluginHost::IShell::startup, Startup, (), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, Startup, (const startup value), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Resumed, (const bool value), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Metadata, (string& info), (const, override));

    MOCK_METHOD(bool, Resumed, (), (const, override));
    MOCK_METHOD(bool, IsSupported, (const uint8_t), (const, override));
    MOCK_METHOD(void, EnableWebServer, (const string&, const string&), (override));
    MOCK_METHOD(void, DisableWebServer, (), (override));
    MOCK_METHOD(WPEFramework::PluginHost::ISubSystem*, SubSystems, (), (override));
    MOCK_METHOD(uint32_t, Submit, (const uint32_t, const WPEFramework::Core::ProxyType<WPEFramework::Core::JSON::IElement>&), (override));
    MOCK_METHOD(void, Notify, (const string&), (override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t), (override));
    MOCK_METHOD(void*, QueryInterfaceByCallsign, (const uint32_t, const string&), (override));
    MOCK_METHOD(void, Register, (WPEFramework::PluginHost::IPlugin::INotification*), (override));
    MOCK_METHOD(void, Unregister, (WPEFramework::PluginHost::IPlugin::INotification*), (override));
    MOCK_METHOD(string, Model, (), (const, override));
    MOCK_METHOD(bool, Background, (), (const, override));
    MOCK_METHOD(string, Accessor, (), (const, override));
    MOCK_METHOD(string, ProxyStubPath, (), (const, override));
    MOCK_METHOD(string, HashKey, (), (const, override));
    MOCK_METHOD(string, Substitute, (const string&), (const, override));
    MOCK_METHOD(WPEFramework::PluginHost::IShell::ICOMLink*, COMLink, (), (override));
    MOCK_METHOD(reason, Reason, (), (const, override));
    MOCK_METHOD(string, SystemRootPath, (), (const, override));
};

#endif //SERVICEMOCK_H


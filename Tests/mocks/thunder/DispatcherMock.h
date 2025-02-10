#ifndef DISPATCHERMOCK_H
#define DISPATCHERMOCK_H

#include <gmock/gmock.h>

#include "Module.h"

 class DispatcherMock: public WPEFramework::PluginHost::IDispatcher{
 public:
         virtual ~DispatcherMock() = default;
         MOCK_METHOD(void, AddRef, (), (const, override));
         MOCK_METHOD(uint32_t, Release, (), (const, override));
         MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNummer), (override));
         MOCK_METHOD(void, Activate, (WPEFramework::PluginHost::IShell* service));
         MOCK_METHOD(void, Deactivate, ());
         MOCK_METHOD(WPEFramework::Core::ProxyType<WPEFramework::Core::JSONRPC::Message>, Invoke, (const string&, uint32_t, const WPEFramework::Core::JSONRPC::Message&), (override));
};

#endif //DISPATCHERMOCK_H


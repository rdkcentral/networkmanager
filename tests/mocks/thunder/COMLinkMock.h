#ifndef RDKSERVICES_TESTS_MOCKS_COMLINKMOCK_H_
#define RDKSERVICES_TESTS_MOCKS_COMLINKMOCK_H_

#include <gmock/gmock.h>
#include "Module.h"

class COMLinkMock : public WPEFramework::PluginHost::IShell::ICOMLink {
public:
    virtual ~COMLinkMock() = default;

    MOCK_METHOD(void, Register, (WPEFramework::RPC::IRemoteConnection::INotification*), (override));
    MOCK_METHOD(void, Unregister, (WPEFramework::RPC::IRemoteConnection::INotification*), (override));
    MOCK_METHOD(WPEFramework::RPC::IRemoteConnection*, RemoteConnection, (const uint32_t), (override));
    MOCK_METHOD(void*, Instantiate, (const WPEFramework::RPC::Object&, const uint32_t, uint32_t&, const string&, const string&), (override));
};

#endif //RDKSERVICES_TESTS_MOCKS_COMLINKMOCK_H_


/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#include "FactoriesImplementation.h"
#include "ThunderPortability.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <condition_variable>
#include "NetworkManagerImplementation.h"
#include "NetworkManager.h"
#include "IarmBusMock.h"
#include "ServiceMock.h"
#include "Module.h"

#include <fstream>
#include "ThunderPortability.h"
#include "INetworkManager.h"

using namespace std;
using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using IStringIterator = RPC::IIteratorType<string,RPC::ID_STRINGITERATOR>;
using ::testing::NiceMock;

namespace {
class InternetStatusNotificationProbe : public Exchange::INetworkManager::INotification {
public:
    void onInternetStatusChangeWithReason(const Exchange::INetworkManager::InternetStatus prevState,
                                const Exchange::INetworkManager::InternetStatus currState,
                                const string interface,
                                const string reason) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++count;
        lastPrev = prevState;
        lastCurr = currState;
        lastInterface = interface;
        lastReason = reason;
        cv.notify_all();
    }

    bool WaitForCount(const size_t expected, const std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, timeout, [&]() { return count >= expected; });
    }

    size_t Count() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return count;
    }

    Exchange::INetworkManager::InternetStatus LastPrev() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return lastPrev;
    }

    Exchange::INetworkManager::InternetStatus LastCurr() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return lastCurr;
    }

    string LastInterface() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return lastInterface;
    }

    string LastReason() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return lastReason;
    }

    uint32_t AddRef() const override
    {
        return Core::ERROR_NONE;
    }

    uint32_t Release() const override
    {
        return Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t id) override
    {
        if ((id == Exchange::INetworkManager::INotification::ID) || (id == Core::IUnknown::ID)) {
            return static_cast<Exchange::INetworkManager::INotification*>(this);
        }
        return nullptr;
    }

private:
    mutable std::mutex mutex;
    std::condition_variable cv;
    size_t count {0};
    Exchange::INetworkManager::InternetStatus lastPrev {Exchange::INetworkManager::INTERNET_UNKNOWN};
    Exchange::INetworkManager::InternetStatus lastCurr {Exchange::INetworkManager::INTERNET_UNKNOWN};
    string lastInterface;
    string lastReason;
};
} // namespace

class NetworkManagerImplTest : public ::testing::Test {
protected:
    static Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImplementation;
    static Exchange::INetworkManager* interface;
    static IarmBusImplMock* p_iarmBusImplMock;

    static void SetUpTestSuite() {
        // Create the mock implementation for IARM Bus
        p_iarmBusImplMock = new NiceMock<IarmBusImplMock>;
        IarmBus::setImpl(p_iarmBusImplMock);

        // Create the NetworkManagerImplementation instance
        NetworkManagerImplementation = Core::ProxyType<Plugin::NetworkManagerImplementation>::Create();
        interface = static_cast<Exchange::INetworkManager*>(NetworkManagerImplementation->QueryInterface(Exchange::INetworkManager::ID));
        ASSERT_TRUE(interface != nullptr);
    }

    static void TearDownTestSuite() {
        // Release the interface and clean up resources
        interface->Release();
        IarmBus::setImpl(nullptr);
        if (p_iarmBusImplMock != nullptr) {
            delete p_iarmBusImplMock;
            p_iarmBusImplMock = nullptr;
        }
    }

    virtual void SetUp() override {
        ASSERT_TRUE(interface != nullptr);
    }

    virtual void TearDown() override {
        ASSERT_TRUE(interface != nullptr);
    }
};

// Static member initialization
Core::ProxyType<Plugin::NetworkManagerImplementation> NetworkManagerImplTest::NetworkManagerImplementation;
Exchange::INetworkManager* NetworkManagerImplTest::interface = nullptr;
IarmBusImplMock* NetworkManagerImplTest::p_iarmBusImplMock = nullptr;

TEST_F(NetworkManagerImplTest, SetLogLevel) {
    // Test setting log level to FATAL
    Exchange::INetworkManager::Logging logLevel = Exchange::INetworkManager::LOG_LEVEL_FATAL;
    uint32_t result = interface->SetLogLevel(logLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Test setting log level to ERROR
    logLevel = Exchange::INetworkManager::LOG_LEVEL_ERROR;
    result = interface->SetLogLevel(logLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Test setting log level to WARNING
    logLevel = Exchange::INetworkManager::LOG_LEVEL_WARNING;
    result = interface->SetLogLevel(logLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Test setting log level to INFO
    logLevel = Exchange::INetworkManager::LOG_LEVEL_INFO;
    result = interface->SetLogLevel(logLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Test setting log level to DEBUG
    logLevel = Exchange::INetworkManager::LOG_LEVEL_DEBUG;
    result = interface->SetLogLevel(logLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);
}

TEST_F(NetworkManagerImplTest, GetLogLevel) {
    // Test getting log level after setting it to FATAL
    Exchange::INetworkManager::Logging logLevel = Exchange::INetworkManager::LOG_LEVEL_FATAL;
    interface->SetLogLevel(logLevel);
    Exchange::INetworkManager::Logging retrievedLogLevel;
    uint32_t result = interface->GetLogLevel(retrievedLogLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(retrievedLogLevel, Exchange::INetworkManager::LOG_LEVEL_FATAL);

    // Test getting log level after setting it to ERROR
    logLevel = Exchange::INetworkManager::LOG_LEVEL_ERROR;
    interface->SetLogLevel(logLevel);
    result = interface->GetLogLevel(retrievedLogLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(retrievedLogLevel, Exchange::INetworkManager::LOG_LEVEL_ERROR);

    // Test getting log level after setting it to WARNING
    logLevel = Exchange::INetworkManager::LOG_LEVEL_WARNING;
    interface->SetLogLevel(logLevel);
    result = interface->GetLogLevel(retrievedLogLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(retrievedLogLevel, Exchange::INetworkManager::LOG_LEVEL_WARNING);

    // Test getting log level after setting it to INFO
    logLevel = Exchange::INetworkManager::LOG_LEVEL_INFO;
    interface->SetLogLevel(logLevel);
    result = interface->GetLogLevel(retrievedLogLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(retrievedLogLevel, Exchange::INetworkManager::LOG_LEVEL_INFO);

    // Test getting log level after setting it to DEBUG
    logLevel = Exchange::INetworkManager::LOG_LEVEL_DEBUG;
    interface->SetLogLevel(logLevel);
    result = interface->GetLogLevel(retrievedLogLevel);
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_EQ(retrievedLogLevel, Exchange::INetworkManager::LOG_LEVEL_DEBUG);
}

TEST_F(NetworkManagerImplTest, LogMessageTruncation) {
    // Generate a log message longer than the maximum allowed size
    std::string longMessage(1025, 'A'); // Create a string with 1025 'A' characters
    NetworkManagerLogger::logPrint(NetworkManagerLogger::DEBUG_LEVEL, __FILE__, __func__, __LINE__, "%s", longMessage.c_str());

    // Since the log message is truncated, the last three characters should be "..."
    // This test ensures that the truncation logic is executed
    EXPECT_TRUE(true); // Placeholder assertion to ensure the test runs
}

TEST_F(NetworkManagerImplTest, GetConnectivityTestEndpoints)
{
    // Set up mock endpoints in the ConnectivityMonitor
    std::vector<std::string> mockEndpoints = {"http://clients3.google.com/generate_204", "http://example.com"};
    NetworkManagerImplementation->connectivityMonitor.setConnectivityMonitorEndpoints(mockEndpoints);

    // Call GetConnectivityTestEndpoints
    RPC::IIteratorType<string,RPC::ID_STRINGITERATOR>* endpoints = nullptr;
    uint32_t result = interface->GetConnectivityTestEndpoints(endpoints);

    // Verify the result
    EXPECT_EQ(result, Core::ERROR_NONE);
    ASSERT_NE(endpoints, nullptr);

    // Verify the endpoints returned
    std::vector<std::string> retrievedEndpoints;
    std::string endpoint;
    while (endpoints->Next(endpoint)) {
        retrievedEndpoints.push_back(endpoint);
    }
    EXPECT_EQ(retrievedEndpoints, mockEndpoints);

    // Clean up
    endpoints->Release();
        // remove any previous connectivity endpoints file
}

TEST_F(NetworkManagerImplTest, SetConnectivityTestEndpoints_EmptyEndpoints) {
    // Prepare empty endpoints
    std::vector<std::string> emptyEndpoints;
    RPC::IIteratorType<string,RPC::ID_STRINGITERATOR>* endpoints = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(emptyEndpoints);

    // Call SetConnectivityTestEndpoints
    uint32_t result = interface->SetConnectivityTestEndpoints(endpoints);

    // Verify the result
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Verify the endpoints were not set
    std::vector<std::string> retrievedEndpoints = NetworkManagerImplementation->connectivityMonitor.getConnectivityMonitorEndpoints();
    printf("Retrieved Endpoints Size: %zu %s\n", retrievedEndpoints.size(), retrievedEndpoints[0].c_str());
	EXPECT_TRUE(retrievedEndpoints.empty() != true && retrievedEndpoints[0] == "http://clients3.google.com/generate_204"); // default endpoint should remain

    // Clean up
    endpoints->Release();
}

TEST_F(NetworkManagerImplTest, SetConnectivityTestEndpoints_ValidEndpoints) {
    // Prepare valid endpoints
    std::vector<std::string> validEndpoints = {"http://clients3.google.com/generate_204", "http://example.com"};
    RPC::IIteratorType<string,RPC::ID_STRINGITERATOR>* endpoints = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(validEndpoints);

    // Call SetConnectivityTestEndpoints
    uint32_t result = interface->SetConnectivityTestEndpoints(endpoints);

    // Verify the result
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Verify the endpoints were set correctly
    std::vector<std::string> retrievedEndpoints = NetworkManagerImplementation->connectivityMonitor.getConnectivityMonitorEndpoints();
    EXPECT_EQ(retrievedEndpoints, validEndpoints);

    // Clean up
    endpoints->Release();
        // remove any previous connectivity endpoints file
}

TEST_F(NetworkManagerImplTest, SetConnectivityTestEndpoints_InvalidEndpoints) {
    // Prepare invalid endpoints (too short to be valid URLs)
    std::vector<std::string> invalidEndpoints = {"1234567", "1234567890"}; // "1234567" is not a valid URL less than 7, "12345678" is too short
    RPC::IIteratorType<string,RPC::ID_STRINGITERATOR>* endpoints = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(invalidEndpoints);

    // Call SetConnectivityTestEndpoints
    uint32_t result = interface->SetConnectivityTestEndpoints(endpoints);

    // Verify the result
    EXPECT_EQ(result, Core::ERROR_NONE);

    // Verify the endpoints were not set
    std::vector<std::string> retrievedEndpoints = NetworkManagerImplementation->connectivityMonitor.getConnectivityMonitorEndpoints();
    EXPECT_TRUE(retrievedEndpoints.size() == 1 && retrievedEndpoints[0] == "1234567890");

    // Clean up
    endpoints->Release();
}

TEST_F(NetworkManagerImplTest, SetConnectivityTestEndpoints_TooManyEndpoints) {

	std::vector<std::string> tooManyEndpoints;
	for (int i = 0; i < 12; ++i) {
		tooManyEndpoints.push_back("http://example.com/endpoint" + std::to_string(i));
	}
	RPC::IIteratorType<string,RPC::ID_STRINGITERATOR>* endpoints = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(tooManyEndpoints);

	// Call SetConnectivityTestEndpoints
	uint32_t result = interface->SetConnectivityTestEndpoints(endpoints);

	// Verify the result
	EXPECT_EQ(result, Core::ERROR_NONE);

	std::vector<std::string> retrievedEndpoints = NetworkManagerImplementation->connectivityMonitor.getConnectivityMonitorEndpoints();
	EXPECT_EQ((int)retrievedEndpoints.size(), 12);
	
	// Clean up
	endpoints->Release();
}

#ifdef USE_CONNECTIVITYCHECKMGR
TEST_F(NetworkManagerImplTest, DelegatedInternetStatusBridgePublishesAndDeduplicates)
{
    InternetStatusNotificationProbe notification;
    ASSERT_EQ(interface->Register(&notification), Core::ERROR_NONE);

    const string configLine = R"({"loglevel":3,"useConnectivityCheckMgr":true})";
    ASSERT_EQ(interface->Configure(configLine), Core::ERROR_NONE);

    NetworkManagerImplementation->setDefaultInterface("eth0");
    NetworkManagerImplementation->OnDelegatedInternetStatusChange(Exchange::INetworkManager::INTERNET_LIMITED);

    EXPECT_TRUE(notification.WaitForCount(1, std::chrono::milliseconds(1000)));
    EXPECT_EQ(notification.LastCurr(), Exchange::INetworkManager::INTERNET_LIMITED);
    EXPECT_EQ(notification.LastInterface(), "eth0");

    // Same state is deduplicated.
    NetworkManagerImplementation->OnDelegatedInternetStatusChange(Exchange::INetworkManager::INTERNET_LIMITED);
    EXPECT_EQ(notification.Count(), static_cast<size_t>(1));

    // New state transition is published.
    NetworkManagerImplementation->OnDelegatedInternetStatusChange(Exchange::INetworkManager::INTERNET_FULLY_CONNECTED);
    EXPECT_TRUE(notification.WaitForCount(2, std::chrono::milliseconds(1000)));
    EXPECT_EQ(notification.LastPrev(), Exchange::INetworkManager::INTERNET_LIMITED);
    EXPECT_EQ(notification.LastCurr(), Exchange::INetworkManager::INTERNET_FULLY_CONNECTED);

    EXPECT_EQ(interface->Unregister(&notification), Core::ERROR_NONE);
}

TEST_F(NetworkManagerImplTest, DelegatedNoInternetReasonIsPublishedOnlyForNoInternet)
{
    InternetStatusNotificationProbe notification;
    ASSERT_EQ(interface->Register(&notification), Core::ERROR_NONE);

    const string configLine = R"({"loglevel":3,"useConnectivityCheckMgr":true})";
    ASSERT_EQ(interface->Configure(configLine), Core::ERROR_NONE);

    NetworkManagerImplementation->setDefaultInterface("eth0");
    NetworkManagerImplementation->OnDelegatedInternetStatusChange(
        Exchange::INetworkManager::INTERNET_NOT_AVAILABLE, "PROBE_FAILED");

    EXPECT_TRUE(notification.WaitForCount(1, std::chrono::milliseconds(1000)));
    EXPECT_EQ(notification.LastCurr(), Exchange::INetworkManager::INTERNET_NOT_AVAILABLE);
    EXPECT_EQ(notification.LastReason(), "PROBE_FAILED");

    NetworkManagerImplementation->OnDelegatedInternetStatusChange(
        Exchange::INetworkManager::INTERNET_FULLY_CONNECTED, "STALE_REASON");

    EXPECT_TRUE(notification.WaitForCount(2, std::chrono::milliseconds(1000)));
    EXPECT_EQ(notification.LastCurr(), Exchange::INetworkManager::INTERNET_FULLY_CONNECTED);
    EXPECT_TRUE(notification.LastReason().empty());

    EXPECT_EQ(interface->Unregister(&notification), Core::ERROR_NONE);
}

TEST_F(NetworkManagerImplTest, BuiltInMonitorInternetStatusEventOmitsReason)
{
    InternetStatusNotificationProbe notification;
    ASSERT_EQ(interface->Register(&notification), Core::ERROR_NONE);

    NetworkManagerImplementation->ReportInternetStatusChange(
        Exchange::INetworkManager::INTERNET_UNKNOWN,
        Exchange::INetworkManager::INTERNET_NOT_AVAILABLE, "eth0");

    EXPECT_TRUE(notification.WaitForCount(1, std::chrono::milliseconds(1000)));
    EXPECT_TRUE(notification.LastReason().empty());

    EXPECT_EQ(interface->Unregister(&notification), Core::ERROR_NONE);
}

TEST_F(NetworkManagerImplTest, DelegatedInternetStatusBridgeIgnoredWhenDelegationDisabled)
{
    InternetStatusNotificationProbe notification;
    ASSERT_EQ(interface->Register(&notification), Core::ERROR_NONE);

    const string configLine = R"({"loglevel":3,"useConnectivityCheckMgr":false})";
    ASSERT_EQ(interface->Configure(configLine), Core::ERROR_NONE);

    NetworkManagerImplementation->OnDelegatedInternetStatusChange(Exchange::INetworkManager::INTERNET_LIMITED);
    EXPECT_FALSE(notification.WaitForCount(1, std::chrono::milliseconds(250)));

    EXPECT_EQ(interface->Unregister(&notification), Core::ERROR_NONE);
}
#endif


/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#pragma once

#include <gmock/gmock.h>
#include "libIBus.h"
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

class IarmBusImplMock : public IarmBusImpl {
public:
    IarmBusImplMock()
        : IarmBusImpl()
    {
        // Defaults:
        ON_CALL(*this, IARM_Bus_IsConnected(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [](const char*, int* isRegistered) {
                    *isRegistered = 1;
                    TEST_LOG("Mock IARM_Bus_IsConnected called");
                    return IARM_RESULT_SUCCESS;
                }));

        ON_CALL(*this, IARM_Bus_Init(::testing::_))
            .WillByDefault(::testing::Invoke(
                [](const char* name) {
                    TEST_LOG("Mock IARM_Bus_Init called");
                    return IARM_RESULT_SUCCESS;
                }));
        ON_CALL(*this, IARM_Bus_Connect())
            .WillByDefault(::testing::Invoke(
                []() {
                    TEST_LOG("Mock IARM_Bus_Connect called");
                    return IARM_RESULT_SUCCESS;
                }));
        ON_CALL(*this, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
        ON_CALL(*this, IARM_Bus_UnRegisterEventHandler(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
        ON_CALL(*this, IARM_Bus_Call(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
		ON_CALL(*this, IARM_Bus_BroadcastEvent(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
        ON_CALL(*this, IARM_Bus_RegisterCall(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
        ON_CALL(*this, IARM_Bus_Call_with_IPCTimeout(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
    }
    virtual ~IarmBusImplMock() = default;

    MOCK_METHOD(IARM_Result_t, IARM_Bus_Init, (const char* name), (override));
    MOCK_METHOD(IARM_Result_t, IARM_Bus_Connect, (), (override));
    MOCK_METHOD(IARM_Result_t, IARM_Bus_IsConnected, (const char* memberName, int* isRegistered), (override));
    MOCK_METHOD(IARM_Result_t, IARM_Bus_RegisterEventHandler, (const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler), (override));
    MOCK_METHOD(IARM_Result_t, IARM_Bus_UnRegisterEventHandler, (const char* ownerName, IARM_EventId_t eventId), (override));
    MOCK_METHOD(IARM_Result_t, IARM_Bus_RemoveEventHandler, (const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler), (override));
    MOCK_METHOD(IARM_Result_t, IARM_Bus_Call, (const char* ownerName, const char* methodName, void* arg, size_t argLen), (override));
    MOCK_METHOD(IARM_Result_t, IARM_Bus_RegisterCall, (const char* methodName, IARM_BusCall_t handler), (override));
	MOCK_METHOD(IARM_Result_t, IARM_Bus_BroadcastEvent, (const char *ownerName, IARM_EventId_t eventId, void *arg, size_t argLen), (override));
    MOCK_METHOD(IARM_Result_t, IARM_Bus_Call_with_IPCTimeout, (const char *ownerName,  const char *methodName, void *arg, size_t argLen, int timeout), (override));
};


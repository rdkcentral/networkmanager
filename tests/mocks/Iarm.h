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

#include <string>

typedef int IARM_EventId_t;

typedef enum _IARM_Result_t {
    IARM_RESULT_SUCCESS,
    IARM_RESULT_INVALID_PARAM,
    IARM_RESULT_INVALID_STATE,
    IARM_RESULT_IPCCORE_FAIL,
    IARM_RESULT_OOM,
} IARM_Result_t;

#define IARM_BUS_DAEMON_NAME "Daemon"

typedef IARM_Result_t (*IARM_BusCall_t)(void* arg);
typedef void (*IARM_EventHandler_t)(const char* owner, IARM_EventId_t eventId, void* data, size_t len);

class IarmBusImpl {
public:
    virtual ~IarmBusImpl() = default;

    virtual IARM_Result_t IARM_Bus_Init(const char* name) = 0;
    virtual IARM_Result_t IARM_Bus_Connect() = 0;
    virtual IARM_Result_t IARM_Bus_IsConnected(const char* memberName, int* isRegistered) = 0;
    virtual IARM_Result_t IARM_Bus_RegisterEventHandler(const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) = 0;
    virtual IARM_Result_t IARM_Bus_UnRegisterEventHandler(const char* ownerName, IARM_EventId_t eventId) = 0;
    virtual IARM_Result_t IARM_Bus_RemoveEventHandler(const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) = 0;
    virtual IARM_Result_t IARM_Bus_Call(const char* ownerName, const char* methodName, void* arg, size_t argLen) = 0;
    virtual IARM_Result_t IARM_Bus_BroadcastEvent(const char *ownerName, IARM_EventId_t eventId, void *arg, size_t argLen) = 0;
    virtual IARM_Result_t IARM_Bus_RegisterCall(const char* methodName, IARM_BusCall_t handler) = 0;
    virtual IARM_Result_t IARM_Bus_Call_with_IPCTimeout(const char *ownerName,  const char *methodName, void *arg, size_t argLen, int timeout) = 0;
};

class IarmBus {
protected:
    static IarmBusImpl* impl;
public:
    IarmBus();
    IarmBus(const IarmBus &obj) = delete; // deleted copy constructor so that copy of the instance cannot be created.
    static void setImpl(IarmBusImpl* newImpl);
    static IARM_Result_t IARM_Bus_Init(const char* name);
    static IARM_Result_t IARM_Bus_Connect();
    static IARM_Result_t IARM_Bus_IsConnected(const char* memberName, int* isRegistered);
    static IARM_Result_t IARM_Bus_RegisterEventHandler(const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler);
    static IARM_Result_t IARM_Bus_UnRegisterEventHandler(const char* ownerName, IARM_EventId_t eventId);
    static IARM_Result_t IARM_Bus_RemoveEventHandler(const char* ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler);
    static IARM_Result_t IARM_Bus_Call(const char* ownerName, const char* methodName, void* arg, size_t argLen);
    static IARM_Result_t IARM_Bus_BroadcastEvent(const char *ownerName, IARM_EventId_t eventId, void *arg, size_t argLen) ;

    static IARM_Result_t IARM_Bus_RegisterCall(const char* methodName, IARM_BusCall_t handler);
    static IARM_Result_t IARM_Bus_Call_with_IPCTimeout(const char *ownerName,  const char *methodName, void *arg, size_t argLen, int timeout);
};


extern IARM_Result_t (*IARM_Bus_Init)(const char*);
extern IARM_Result_t (*IARM_Bus_Connect)();
extern IARM_Result_t (*IARM_Bus_IsConnected)(const char*,int*);
extern IARM_Result_t (*IARM_Bus_RegisterEventHandler)(const char*,IARM_EventId_t,IARM_EventHandler_t);
extern IARM_Result_t (*IARM_Bus_UnRegisterEventHandler)(const char*,IARM_EventId_t);
extern IARM_Result_t (*IARM_Bus_RemoveEventHandler)(const char*,IARM_EventId_t,IARM_EventHandler_t);
extern IARM_Result_t (*IARM_Bus_Call)(const char*,const char*,void*,size_t);
extern IARM_Result_t (*IARM_Bus_BroadcastEvent)(const char *,IARM_EventId_t,void *,size_t);
extern IARM_Result_t (*IARM_Bus_RegisterCall)(const char*,IARM_BusCall_t);
extern IARM_Result_t (*IARM_Bus_Call_with_IPCTimeout)(const char*,const char*,void*,size_t,int);

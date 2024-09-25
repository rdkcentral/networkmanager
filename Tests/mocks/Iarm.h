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


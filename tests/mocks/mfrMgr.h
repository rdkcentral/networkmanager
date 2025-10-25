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

#define SSID_SIZE                   32
#define WIFI_MAX_PASSWORD_LEN       64

#define IARM_BUS_MFRLIB_NAME                            "MFRLib"
#define IARM_BUS_MFRLIB_API_WIFI_Credentials            "mfrWifiCredentials"
#define IARM_BUS_MFRLIB_API_WIFI_EraseAllData           "mfrWifiEraseAllData"

/**
 * @brief WIFI API return status
 *
 */
typedef enum _WIFI_API_RESULT {
    WIFI_API_RESULT_SUCCESS = 0,                  ///< operation is successful
    WIFI_API_RESULT_FAILED,                       ///< Operation general error. This enum is deprecated
    WIFI_API_RESULT_NULL_PARAM,                   ///< NULL argument is passed to the module
    WIFI_API_RESULT_INVALID_PARAM,                ///< Invalid argument is passed to the module
    WIFI_API_RESULT_NOT_INITIALIZED,              ///< module not initialized
    WIFI_API_RESULT_OPERATION_NOT_SUPPORTED,      ///< operation not supported in the specific platform
    WIFI_API_RESULT_READ_WRITE_FAILED,            ///< flash read/write failed or crc check failed
    WIFI_API_RESULT_MAX                           ///< Out of range - required to be the last item of the enum
} WIFI_API_RESULT;
/**
 * @brief WIFI credentials data struct
 *
 */
typedef struct {
    char cSSID[SSID_SIZE+1];                      ///< SSID field.
    char cPassword[WIFI_MAX_PASSWORD_LEN+1];      ///< password field
    int  iSecurityMode;                           ///< security mode. Platform dependent and caller is responsible to validate it
} WIFI_DATA;

typedef enum _WifiRequestType {
    WIFI_GET_CREDENTIALS = 0,
    WIFI_SET_CREDENTIALS = 1
} WifiRequestType_t;

typedef struct _IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t {
    WIFI_DATA wifiCredentials;
    WifiRequestType_t requestType;
    WIFI_API_RESULT returnVal;
} IARM_BUS_MFRLIB_API_WIFI_Credentials_Param_t;

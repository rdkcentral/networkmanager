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

#pragma once

/* Below declared functions are copied from wpa_supplicant(lib32-wpa-supplicant/2.10-r0/wpa_supplicant-2.10/src/common/wpa_ctrl.h)
 * for resolving the unit test compilation issue */

#ifdef  __cplusplus
extern "C" {
#endif

struct wpa_ctrl {
    int s;
    char *ctrl_path;
};

struct wpa_ctrl * wpa_ctrl_open(const char *ctrl_path);
void wpa_ctrl_close(struct wpa_ctrl *ctrl);
int wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,char *reply, size_t *reply_len,void (*msg_cb)(char *msg, size_t len));
int wpa_ctrl_pending(struct wpa_ctrl* ctrl);
int wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len);
int wpa_ctrl_attach(struct wpa_ctrl* ctrl_handle);

#ifdef  __cplusplus
}
#endif

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

#include <gmock/gmock.h>
#include <mntent.h>

#include "Wraps.h"

extern "C" FILE* __real_setmntent(const char* command, const char* type);

class WrapsImplMock : public WrapsImpl {
public:
    WrapsImplMock():WrapsImpl()
    {
	   /*Setting up Default behavior for setmntent: 
        * We are mocking setmntent in this file below with __wrap_setmntent,
        * and the actual setmntent will be called via this interface */
        ON_CALL(*this, setmntent(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const char* command, const char* type) -> FILE* {
                return __real_setmntent(command, type);
            }));
    }
    virtual ~WrapsImplMock() = default;

    MOCK_METHOD(int, system, (const char* command), (override));
    MOCK_METHOD(FILE*, popen, (const char* command, const char* type), (override));
    MOCK_METHOD(int, pclose, (FILE* pipe), (override));
    MOCK_METHOD(void, syslog, (int pri, const char* fmt, va_list args), (override));
    MOCK_METHOD(FILE*, setmntent, (const char* command, const char* type), (override));
    MOCK_METHOD(struct mntent*, getmntent, (FILE* pipe), (override));
    MOCK_METHOD(struct wpa_ctrl*, wpa_ctrl_open, (const char *ctrl_path), (override));
    MOCK_METHOD(int, wpa_ctrl_request, (struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,char *reply, size_t *reply_len,void (*msg_cb)(char *msg, size_t len)), (override));
    MOCK_METHOD(void, wpa_ctrl_close, (struct wpa_ctrl *ctrl), (override));
    MOCK_METHOD(int, wpa_ctrl_pending, (struct wpa_ctrl *ctrl), (override));
    MOCK_METHOD(int, wpa_ctrl_recv , (struct wpa_ctrl *ctrl, char *reply, size_t *reply_len), (override));
    MOCK_METHOD(int, wpa_ctrl_attach, (struct wpa_ctrl *ctrl), (override));
    MOCK_METHOD(FILE*, v_secure_popen, (const char *direction, const char *command, va_list args), (override));
    MOCK_METHOD(int, v_secure_pclose, (FILE *file), (override));
    MOCK_METHOD(int, unlink, (const char* filePath), (override));
    MOCK_METHOD(int, v_secure_system, (const char *command, va_list args), (override));
    MOCK_METHOD(ssize_t, readlink, (const char *pathname, char *buf, size_t bufsiz), (override));
    MOCK_METHOD(int, ioctl, (int fd,unsigned long request,void* arg), (override));
    MOCK_METHOD(int, statvfs, (const char* path,struct statvfs* buf), (override));
    MOCK_METHOD(int, statfs, (const char* path, struct statfs* buf), (override));
    MOCK_METHOD(std::istream&, getline, (std::istream& is, std::string& line), (override));
    MOCK_METHOD(int, mkdir, (const char* path, mode_t mode), (override));
    MOCK_METHOD(int, mount, (const char* source, const char* target, const char* filesystemtype, unsigned long mountflags, const void* data), ());
    MOCK_METHOD(int, stat, (const char* path, struct stat* info), (override));
    MOCK_METHOD(int, open, (const char* pathname, int flags, mode_t mode),(override));
    MOCK_METHOD(int, umount, (const char* path), (override));
    MOCK_METHOD(int, rmdir, (const char* pathname), (override));
    MOCK_METHOD(time_t, time, (time_t* arg), (override));
    MOCK_METHOD(int, access, (const char* pathname, int mode), (override));
    MOCK_METHOD(int, chown, (const char *path, uid_t owner, gid_t group), (override));
    MOCK_METHOD(int, nftw, (const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags), (override));
    MOCK_METHOD(DIR* , opendir, (const char* pathname), (override));
    MOCK_METHOD(struct dirent*, readdir, (DIR*), (override));
    MOCK_METHOD(int, closedir, (DIR* dirp), (override));
};

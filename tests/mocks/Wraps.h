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

#include <stdio.h>
#include <mntent.h>
#include <istream>
#include "wpa_ctrl_mock.h"
#include "secure_wrappermock.h"
#include <unistd.h>
#include <dirent.h>


class WrapsImpl {
public:
    virtual ~WrapsImpl() = default;

    virtual int system(const char* command) = 0;
    virtual FILE* popen(const char* command, const char* type) = 0;
    virtual int pclose(FILE *pipe) = 0;
    virtual void syslog(int pri, const char* fmt, va_list args) = 0;
    virtual FILE* setmntent(const char* command, const char* type) = 0;
    virtual struct mntent* getmntent(FILE *pipe) = 0;
    virtual struct wpa_ctrl * wpa_ctrl_open(const char *ctrl_path) = 0;
    virtual int wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,char *reply, size_t *reply_len,void (*msg_cb)(char *msg, size_t len)) = 0;
    virtual void wpa_ctrl_close(struct wpa_ctrl *ctrl) = 0;
    virtual int wpa_ctrl_pending(struct wpa_ctrl* ctrl) = 0;
    virtual int wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len) = 0;
    virtual int wpa_ctrl_attach(struct wpa_ctrl* ctrl) = 0;
    virtual FILE *v_secure_popen(const char *direction, const char *command, va_list args) = 0;
    virtual int v_secure_pclose(FILE *) = 0;
    virtual int v_secure_system(const char *command, va_list args) = 0;
    virtual int unlink(const char* filePath) = 0;
    virtual ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) = 0;
    virtual int ioctl(int fd,unsigned long request,void* arg) = 0;
    virtual int statvfs(const char* path,struct statvfs* buf) = 0;
    virtual int statfs(const char* path, struct statfs* buf)= 0;
    virtual std::istream& getline(std::istream& is, std::string& line) = 0;
    virtual int mkdir(const char* path, mode_t mode) =0;
    virtual int mount(const char* source, const char* target, const char* filesystemtype, unsigned long mountflags, const void* data) = 0;
    virtual int stat(const char* path, struct stat* info) =0;
    virtual int open(const char* pathname, int flags, mode_t mode) = 0;
    virtual int umount(const char* path) = 0;
    virtual int rmdir(const char* pathname) = 0;
    virtual time_t time(time_t* arg) = 0;
    virtual int access(const char* pathname, int mode) = 0;
    virtual int chown(const char *path, uid_t owner, gid_t group) = 0;
    virtual int nftw(const char* dirpath,int (*fn)(const char*, const struct stat*, int, struct FTW*),int nopenfd, int flags) = 0;
    virtual DIR* opendir(const char* pathname) = 0;
    virtual struct dirent* readdir(DIR* dirp) = 0;
    virtual int closedir(DIR* dirp) = 0;
};

class Wraps {
protected:
   static WrapsImpl* impl;

public:
    Wraps();
    Wraps(const Wraps &obj) = delete;
    static void setImpl(WrapsImpl* newImpl);
    static Wraps& getInstance();

    static int system(const char* command);
    static FILE* popen(const char* command, const char* type);
    static void syslog(int pri, const char* fmt, va_list args);
    static struct wpa_ctrl * wpa_ctrl_open(const char *ctrl_path);
    static int wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,char *reply, size_t *reply_len,void (*msg_cb)(char *msg, size_t len));
    static void wpa_ctrl_close(struct wpa_ctrl *ctrl);
    static int wpa_ctrl_pending(struct wpa_ctrl* ctrl);
    static int wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len);
    static int wpa_ctrl_attach(struct wpa_ctrl* ctrl);

    static FILE *v_secure_popen(const char *direction, const char *command, va_list args);

    static int v_secure_pclose(FILE *file);
    static int unlink(const char* filePath);
    static int v_secure_system(const char *command, va_list args);

    static int pclose(FILE* pipe);

    static FILE* setmntent(const char* command, const char* type);

    static struct mntent* getmntent(FILE* pipe);
    ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
    static int ioctl(int fd,unsigned long request,void* arg);
    static int statvfs(const char* path,struct statvfs* buf);
    static int statfs(const char* path, struct statfs* buf);
    static std::istream& getline(std::istream& is, std::string& line);
    static int mkdir(const char* path, mode_t mode);
    static int mount(const char* source, const char* target, const char* filesystemtype, unsigned long mountflags, const void* data);
    static int stat(const char* path, struct stat* info);
    static int open(const char* pathname, int flags, mode_t mode);
    static int umount(const char* path);
    static int rmdir(const char* pathname);
    static time_t time(time_t* arg);
    static int access(const char* pathname, int mode);
    static int chown(const char *path, uid_t owner, gid_t group);
    static int nftw(const char* dirpath,int (*fn)(const char*, const struct stat*, int, struct FTW*),int nopenfd, int flags);
    static DIR* opendir(const char* pathname);
    static struct dirent* readdir(DIR* dirp);
    static int closedir(DIR* dirp);
};

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

#include <stdarg.h>
#include <syslog.h>
#include "Wraps.h"
#include <mntent.h>
#include <istream>
#include <istream>
#include <assert.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <gmock/gmock.h>
#include <dirent.h>

extern "C" int __real_open(const char* pathname, int flags,int mode=0);

extern "C" int __wrap_system(const char* command)
{
    return Wraps::getInstance().system(command);
}

extern "C" FILE* __wrap_popen(const char* command, const char* type)
{
    return Wraps::getInstance().popen(command, type);
}

extern "C" int __wrap_pclose(FILE* pipe)
{
    return Wraps::getInstance().pclose(pipe);
}

//This function will be called for syslog() in the code (added -Wl,-wrap,syslog)
extern  "C" void __wrap_syslog(int pri, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Wraps::getInstance().syslog(pri, fmt, args);
    vsyslog(pri, fmt, args);
    va_end(args);
}

extern "C" FILE* __wrap_setmntent(const char* command, const char* type)
{
    return Wraps::getInstance().setmntent(command, type);
}

extern "C" struct mntent* __wrap_getmntent(FILE* pipe)
{
    return Wraps::getInstance().getmntent(pipe);
}

extern "C" struct wpa_ctrl * __wrap_wpa_ctrl_open(const char *ctrl_path)
{
    return Wraps::getInstance().wpa_ctrl_open(ctrl_path);
}

extern "C" int __wrap_wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,char *reply, size_t *reply_len,void (*msg_cb)(char *msg, size_t len))
{
    return Wraps::getInstance().wpa_ctrl_request(ctrl, cmd, cmd_len, reply, reply_len, msg_cb);
}

extern "C" void __wrap_wpa_ctrl_close(struct wpa_ctrl *ctrl)
{
    Wraps::getInstance().wpa_ctrl_close(ctrl);
}

extern "C" int __wrap_wpa_ctrl_pending(struct wpa_ctrl *ctrl)
{
    return Wraps::getInstance().wpa_ctrl_pending(ctrl);
}

extern "C" int __wrap_wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len)
{
    return Wraps::getInstance().wpa_ctrl_recv(ctrl, reply, reply_len);
}

extern "C" int __wrap_wpa_ctrl_attach(struct wpa_ctrl *ctrl)
{
    return Wraps::getInstance().wpa_ctrl_attach(ctrl);
}

extern "C" FILE * __wrap_v_secure_popen(const char *direction, const char *command, ...)
{
    va_list args;
    FILE *retFp = nullptr;
    va_start(args, command);
    retFp = Wraps::getInstance().v_secure_popen(direction, command, args);
    va_end(args);
    return retFp;
}

extern "C" int __wrap_v_secure_pclose(FILE *file)
{
    return Wraps::getInstance().v_secure_pclose(file);
}

extern "C" int __wrap_unlink(const char* filePath)
{
    return Wraps::getInstance().unlink(filePath);
}

extern "C" int __wrap_v_secure_system(const char *command, ...)
{
    va_list args;
    int ret;
    va_start(args, command);
    ret = Wraps::getInstance().v_secure_system(command, args);
    va_end(args);
    return ret;
}

extern "C" ssize_t __wrap_readlink(const char *pathname, char *buf, size_t bufsiz)
{
    return Wraps::getInstance().readlink(pathname, buf, bufsiz);
}

extern "C" int __wrap_ioctl(int fd,unsigned long request,void* arg)
{
    return Wraps::getInstance().ioctl(fd, request, arg);
}
extern "C" int __wrap_statvfs(const char* path,struct statvfs* buf)
{
    return Wraps::getInstance().statvfs(path, buf);
}
extern "C" int __wrap_statfs(const char* path, struct statfs* buf)
{
    return Wraps::getInstance().statfs(  path,buf);
}
extern "C" std::istream& __wrap_getline(std::istream& is, std::string& line)
{
    return std::getline(is, line);
}
extern "C" int __wrap_mkdir(const char* path, mode_t mode)
{
    return Wraps::getInstance().mkdir(path,mode);
}
extern "C" int __wrap_mount(const char* source, const char* target, const char* filesystemtype, unsigned long mountflags, const void* data)
{
    return Wraps::getInstance().mount(source,target,filesystemtype,mountflags,data);
}
extern "C" int __wrap_stat(const char* path, struct stat* info)
{
    return Wraps::getInstance().stat( path,info);
}
extern "C" int __wrap_open(const char* pathname, int flags, mode_t mode)
{
    int ret = Wraps::getInstance().open(pathname, flags, mode);
     return ret;
}
extern "C" int __wrap_umount(const char* path)
{
    return Wraps::getInstance().umount(path);
}
extern "C" int __wrap_rmdir(const char* pathname)
{
    return Wraps::getInstance().rmdir(pathname);
}

extern "C" time_t __wrap_time(time_t *arg)
{
    return Wraps::getInstance().time(arg);
}

extern "C" int __wrap_access(const char* pathname, int mode)
{
    return Wraps::getInstance().access(pathname, mode);
}

extern "C" int __wrap_chown(const char *path, uid_t owner, gid_t group)
{
    return Wraps::getInstance().chown(path, owner, group);
}

extern "C" int __wrap_nftw(const char* dirpath,int (*fn)(const char*, const struct stat*, int, struct FTW*),int nopenfd, int flags)
{
    return Wraps::getInstance().nftw(dirpath, fn, nopenfd, flags);
}

extern "C" DIR*  __wrap_opendir(const char* pathname)
{
    return Wraps::getInstance().opendir(pathname);
}

extern "C" struct dirent* __wrap_readdir(DIR* dirp)
{
    return Wraps::getInstance().readdir(dirp);
}

extern "C" int __wrap_closedir(DIR* dirp) {
    return -1;
}

WrapsImpl* Wraps::impl = nullptr;

Wraps::Wraps() {}

void Wraps::setImpl(WrapsImpl* newImpl)
{
        // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

Wraps& Wraps::getInstance()
{
   static Wraps instance;
   return instance;
}

int Wraps::system(const char* command)
{
    EXPECT_NE(impl, nullptr);
    return impl->system(command);
}

FILE* Wraps::popen(const char* command, const char* type)
{
    EXPECT_NE(impl, nullptr);
    return impl->popen(command, type);
}

int Wraps::pclose(FILE* pipe)
{
    EXPECT_NE(impl, nullptr);
    return impl->pclose(pipe);
}

void Wraps::syslog(int pri, const char* fmt, va_list args)
{
    EXPECT_NE(impl, nullptr);
    impl->syslog(pri, fmt, args);
}

FILE* Wraps::setmntent(const char* command, const char* type)
{
    EXPECT_NE(impl, nullptr);
    return impl->setmntent(command,type);
}

struct mntent* Wraps::getmntent(FILE* pipe)
{
    EXPECT_NE(impl, nullptr);
    return impl->getmntent(pipe);
}

struct wpa_ctrl* Wraps::wpa_ctrl_open(const char *ctrl_path)
{
    EXPECT_NE(impl, nullptr);
    return impl->wpa_ctrl_open(ctrl_path);
}

int Wraps::wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,char *reply, size_t *reply_len,void (*msg_cb)(char *msg, size_t len))
{
    EXPECT_NE(impl, nullptr);
    return impl->wpa_ctrl_request(ctrl, cmd, cmd_len, reply, reply_len, msg_cb);
}

void Wraps::wpa_ctrl_close(struct wpa_ctrl *ctrl)
{
    EXPECT_NE(impl, nullptr);
    return impl->wpa_ctrl_close(ctrl);
}

int Wraps::wpa_ctrl_pending(struct wpa_ctrl* ctrl)
{
    EXPECT_NE(impl, nullptr);
    return impl->wpa_ctrl_pending(ctrl);
}

int Wraps::wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len)
{
    EXPECT_NE(impl, nullptr);
    return impl->wpa_ctrl_recv(ctrl, reply, reply_len);
}

int Wraps::wpa_ctrl_attach(struct wpa_ctrl *ctrl)
{
    EXPECT_NE(impl, nullptr);
    return impl->wpa_ctrl_attach(ctrl);
}

FILE * Wraps::v_secure_popen(const char *direction, const char *command, va_list args)
{
    EXPECT_NE(impl, nullptr);
    return impl->v_secure_popen(direction, command, args);
}

int Wraps::v_secure_pclose(FILE *file)
{
    EXPECT_NE(impl, nullptr);
    return impl->v_secure_pclose(file);
}

int Wraps::v_secure_system(const char *command, va_list args)
{
    EXPECT_NE(impl, nullptr);
    return impl->v_secure_system(command,args);
}

int Wraps::unlink(const char* filePath)
{
    EXPECT_NE(impl, nullptr);
    return impl->unlink(filePath);
}

ssize_t Wraps::readlink(const char *pathname, char *buf, size_t bufsiz)
{
    EXPECT_NE(impl, nullptr);
    return impl->readlink(pathname,buf,bufsiz);
}
int Wraps::ioctl(int fd,unsigned long request,void* arg)
{
    EXPECT_NE(impl, nullptr);
    return impl->ioctl(fd,request, arg);
}
int Wraps::statvfs(const char* path,struct statvfs* buf)
{
    EXPECT_NE(impl, nullptr);
    return impl->statvfs(path,buf);
}
int Wraps::statfs(const char* path, struct statfs* buf)
{
    EXPECT_NE(impl, nullptr);
    return impl->statfs(  path,buf);
}
std::istream& Wraps::getline(std::istream& is, std::string& line) 
{
    EXPECT_NE(impl, nullptr);
    return impl->getline(is, line);
}
int Wraps::mkdir(const char* path, mode_t mode)
{
    EXPECT_NE(impl, nullptr);
    return impl->mkdir(path,mode);
}
int Wraps::mount(const char* source, const char* target, const char* filesystemtype, unsigned long mountflags, const void* data)
{
    EXPECT_NE(impl, nullptr);
    return impl->mount(source,target,filesystemtype,mountflags,data);
}
int Wraps::stat(const char* path, struct stat* info)
{
    //EXPECT_NE(impl, nullptr);
    int result =-1;
    if(impl != nullptr)
    {
        result = impl->stat(path, info);
    }
    else
    {
        result = ::stat(path, info);
    }
    //return impl->stat(path,info);
    return result;
}
int Wraps::open(const char* pathname, int flags, mode_t mode)
{
    int result = -1; 
    if (impl != nullptr)
    {
        result = impl->open(pathname, flags, mode);
    }
    else
    {
        result =::open(pathname, flags, mode);
    }
    return result; 
}
int Wraps::umount(const char* path)
{
    EXPECT_NE(impl, nullptr);
    return impl->umount(path);
}
int Wraps::rmdir(const char* pathname)
{
    EXPECT_NE(impl, nullptr);
    return impl->rmdir(pathname);
}

time_t Wraps::time(time_t* arg)
{
    EXPECT_NE(impl, nullptr);
    return impl->time(arg);
}

int Wraps::access(const char* pathname, int mode)
{
    EXPECT_NE(impl, nullptr);
    return impl->access(pathname, mode);
}

int Wraps::chown(const char *path, uid_t owner, gid_t group)
{
    EXPECT_NE(impl, nullptr);
    return impl->chown(path, owner, group);
}

int Wraps::nftw(const char* dirpath,int (*fn)(const char*, const struct stat*, int, struct FTW*),int nopenfd, int flags)
{
    EXPECT_NE(impl, nullptr);
    return impl->nftw(dirpath, fn, nopenfd, flags);
}

DIR* Wraps::opendir(const char* pathname)
{
    DIR* result = nullptr;
    if (impl != nullptr)
    {
        result = impl->opendir(pathname);
    }
    else
    {
        result =::opendir(pathname);
    }
    return result;
}

struct dirent* Wraps::readdir(DIR* dirp)
{
    EXPECT_NE(impl, nullptr);
    return impl->readdir(dirp);
}

int Wraps::closedir(DIR* dirp)
{
    EXPECT_NE(impl, nullptr);
    return impl->closedir(dirp);
}


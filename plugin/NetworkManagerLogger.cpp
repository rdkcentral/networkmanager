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

#include "NetworkManagerLogger.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <map>
#include <string>
#include <algorithm>
#include <cctype>
#include <sys/time.h>

#ifdef USE_RDK_LOGGER
#include "rdk_debug.h"
#endif

namespace NetworkManagerLogger {
    static LogLevel gDefaultLogLevel = INFO_LEVEL;


#ifdef USE_RDK_LOGGER
    rdk_LogLevel mapTordkLogLevel(LogLevel level)
    {
      rdk_LogLevel rdklevel = RDK_LOG_INFO;
      switch (level)
      {
        case FATAL_LEVEL:
          rdklevel = RDK_LOG_FATAL;
          break;
        case ERROR_LEVEL:
          rdklevel = RDK_LOG_ERROR;
          break;
        case WARNING_LEVEL:
          rdklevel = RDK_LOG_WARN;
          break;
        case INFO_LEVEL:
          rdklevel = RDK_LOG_INFO;
          break;
        case DEBUG_LEVEL:
          rdklevel = RDK_LOG_DEBUG;
          break;
      }
      return rdklevel;
    }
#endif

    const char* trimPath(const char* s)
    {
      if (!s)
        return s;

      const char* t = strrchr(s, (int) '/');
      if (t) t++;
      if (!t) t = s;

      return t;
    }

    void Init()
    {
#ifdef USE_RDK_LOGGER
        rdk_logger_init(0 == access("/opt/debug.ini", R_OK) ? "/opt/debug.ini" : "/etc/debug.ini");
#endif
    }

    void logPrint(LogLevel level, const char* file, const char* func, int line, const char* format, ...)
    {
        size_t n = 0;
        const short kFormatMessageSize = 1024;
        char formattedLog[kFormatMessageSize] = {0};

        va_list args;

        va_start(args, format);
        n = vsnprintf(formattedLog, (kFormatMessageSize - 1), format, args);
        va_end(args);

        if (n > (kFormatMessageSize - 1))
        {
            formattedLog[kFormatMessageSize - 4] = '.';
            formattedLog[kFormatMessageSize - 3] = '.';
            formattedLog[kFormatMessageSize - 2] = '.';
        }
        formattedLog[kFormatMessageSize - 1] = '\0';
#ifdef USE_RDK_LOGGER
        RDK_LOG(mapTordkLogLevel(level), "LOG.RDK.NWMGR", "%s\n", formattedLog);
#else
        const char* levelMap[] = {"Fatal", "Error", "Warn", "Info", "Debug"};
        struct timeval tv;
        struct tm* lt;
        const char* fileName = trimPath(file);

        if (gDefaultLogLevel < level)
            return;

        gettimeofday(&tv, NULL);
        lt = localtime(&tv.tv_sec);

        printf("%.2d:%.2d:%.2d.%.6lld [%-5s] [PID=%d] [TID=%d] [%s +%d] %s : %s\n", lt->tm_hour, lt->tm_min, lt->tm_sec, (long long int)tv.tv_usec, levelMap[level], getpid(), gettid(), fileName, line, func, formattedLog);
        fflush(stdout);
#endif
    }

    void SetLevel(LogLevel level)
    {
        gDefaultLogLevel = level;
        NMLOG_INFO("NetworkManager logLevel:%d", level);
#ifdef USE_RDK_LOGGER
        // TODO : Inform RDKLogger to change the log level
#endif
    }

    void GetLevel(LogLevel& level)
    {
        level = gDefaultLogLevel;
#ifdef USE_RDK_LOGGER
        // TODO : Inform RDKLogger to change the log level
#endif
    }
} // namespace NetworkManagerLogger

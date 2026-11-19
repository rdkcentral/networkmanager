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

#ifndef NM_LOGGER_H
#define NM_LOGGER_H

namespace NetworkManagerLogger {

class Debug {
public:
    Debug() = delete;
    Debug(const Debug& a_Copy) = delete;
    Debug& operator=(const Debug& a_RHS) = delete;
    ~Debug() = default;

    explicit Debug(const string& text)
        : _text(WPEFramework::Core::ToString(text))
    {
    }
    Debug(const TCHAR formatter[], ...)
    {
        va_list ap;
        va_start(ap, formatter);
        WPEFramework::Trace::Format(_text, formatter, ap);
        va_end(ap);
    }
    inline const char* Data() const
    {
        return (_text.c_str());
    }
    inline uint16_t Length() const
    {
        return (static_cast<uint16_t>(_text.length()));
    }

private:
    std::string _text;
};

#define NMLOG_DEBUG(FMT, ...) TRACE_GLOBAL(NetworkManagerLogger::Debug, (_T(FMT), ##__VA_ARGS__))
#define NMLOG_INFO(FMT, ...) TRACE_GLOBAL(WPEFramework::Trace::Information, (_T(FMT), ##__VA_ARGS__))
#define NMLOG_WARNING(FMT, ...) TRACE_GLOBAL(WPEFramework::Trace::Warning, (_T(FMT), ##__VA_ARGS__))
#define NMLOG_ERROR(FMT, ...) TRACE_GLOBAL(WPEFramework::Trace::Error, (_T(FMT), ##__VA_ARGS__))
#define NMLOG_FATAL(FMT, ...) TRACE_GLOBAL(WPEFramework::Trace::Fatal, (_T(FMT), ##__VA_ARGS__))

#define LOG_ENTRY_FUNCTION()    { NMLOG_INFO("Entering %s", __func__); }

} // namespace NetworkManagerLogger

#endif  // NM_LOGGER_H

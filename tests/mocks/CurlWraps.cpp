#include "CurlWraps.h"
#include "CurlWrapsMock.h"
#include <stdarg.h>
#include <gmock/gmock.h>

extern "C" CURLMcode __wrap_curl_multi_perform(CURLM* multi_handle, int* still_running)
{
    return CurlWraps::getInstance().curl_multi_perform(multi_handle, still_running);
}

extern "C" CURLMsg* __wrap_curl_multi_info_read(CURLM* multi_handle, int* msgs_left)
{
    return CurlWraps::getInstance().curl_multi_info_read(multi_handle, msgs_left);
}

extern "C" CURLMcode __wrap_curl_multi_poll(CURLM* multi_handle, void* fds, unsigned int nfds, int timeout_ms, int* ret)
{
    return CurlWraps::getInstance().curl_multi_poll(multi_handle, fds, nfds, timeout_ms, ret);
}

CurlWrapsImpl* CurlWraps::impl = nullptr;

CurlWraps::CurlWraps() {}

void CurlWraps::setImpl(CurlWrapsImpl* newImpl)
{
    EXPECT_TRUE((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

CurlWraps& CurlWraps::getInstance()
{
    static CurlWraps instance;
    return instance;
}

CURLMcode CurlWraps::curl_multi_perform(CURLM* multi_handle, int* still_running)
{
    EXPECT_NE(impl, nullptr);
    return impl->curl_multi_perform(multi_handle, still_running);
}

CURLMsg* CurlWraps::curl_multi_info_read(CURLM* multi_handle, int* msgs_left)
{
    EXPECT_NE(impl, nullptr);
    return impl->curl_multi_info_read(multi_handle, msgs_left);
}

CURLMcode CurlWraps::curl_multi_poll(CURLM* multi_handle, void* fds, unsigned int nfds, int timeout_ms, int* ret)
{
    EXPECT_NE(impl, nullptr);
    return impl->curl_multi_poll(multi_handle, fds, nfds, timeout_ms, ret);
}


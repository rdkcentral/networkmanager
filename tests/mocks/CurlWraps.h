#pragma once

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <stdarg.h>
#include <string>

class CurlWrapsImpl {
public:
    virtual ~CurlWrapsImpl() = default;

    virtual CURLMcode curl_multi_perform(CURLM* multi_handle, int* still_running) = 0;
    virtual CURLMsg* curl_multi_info_read(CURLM* multi_handle, int* msgs_left) = 0;
    virtual CURLMcode curl_multi_poll(CURLM* multi_handle, void* fds, unsigned int nfds, int timeout_ms, int* ret) = 0;
};

class CurlWraps {
protected:
    static CurlWrapsImpl* impl;

public:
    CurlWraps();
    CurlWraps(const CurlWraps &obj) = delete;
    static void setImpl(CurlWrapsImpl* newImpl);
    static CurlWraps& getInstance();

    static CURLMcode curl_multi_perform(CURLM* multi_handle, int* still_running);
    static CURLMsg* curl_multi_info_read(CURLM* multi_handle, int* msgs_left);
    static CURLMcode curl_multi_poll(CURLM* multi_handle, void* fds, unsigned int nfds, int timeout_ms, int* ret);
};

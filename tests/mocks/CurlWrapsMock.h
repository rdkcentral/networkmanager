#pragma once

#include <gmock/gmock.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>

#include "CurlWraps.h"

class CurlWrapsImplMock : public CurlWrapsImpl {
public:
    CurlWrapsImplMock():CurlWrapsImpl() {}
    virtual ~CurlWrapsImplMock() = default;

    MOCK_METHOD(CURLMcode, curl_multi_perform, (CURLM* multi_handle, int* still_running), (override));
    MOCK_METHOD(CURLMsg*, curl_multi_info_read, (CURLM* multi_handle, int* msgs_left), (override));
    MOCK_METHOD(CURLMcode, curl_multi_poll, (CURLM* multi_handle, void* fds, unsigned int nfds, int timeout_ms, int* ret), (override));
};

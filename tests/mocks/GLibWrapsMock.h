#pragma once

#include <gmock/gmock.h>
#include "GLibWraps.h"

extern "C" gboolean __real_g_main_loop_is_running(GMainLoop* loop);

class GLibWrapsImplMock : public GLibWrapsImpl {
public:
    GLibWrapsImplMock() : GLibWrapsImpl() {
        ON_CALL(*this, g_main_loop_is_running(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](GMainLoop* loop) -> gboolean {
                return __real_g_main_loop_is_running(loop);
            }));
    }

    virtual ~GLibWrapsImplMock() = default;

    // Mock methods
    MOCK_METHOD(gboolean, g_main_loop_is_running, (GMainLoop* loop), (override));
};

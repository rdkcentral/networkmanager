#pragma once

#include <gmock/gmock.h>
#include "GLibWraps.h"

extern "C" gboolean __real_g_main_loop_is_running(GMainLoop* loop);
extern "C" gulong __real_g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
                                         gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);

class GLibWrapsImplMock : public GLibWrapsImpl {
public:
    GLibWrapsImplMock() : GLibWrapsImpl() {
        ON_CALL(*this, g_main_loop_is_running(::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](GMainLoop* loop) -> gboolean {
                return __real_g_main_loop_is_running(loop);
            }));

        ON_CALL(*this, g_signal_connect_data(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
            [&](gpointer instance, const gchar *detailed_signal, GCallback c_handler,
                gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags) -> gulong {
                return __real_g_signal_connect_data(instance, detailed_signal, c_handler, data, destroy_data, connect_flags);
            }));
    }

    virtual ~GLibWrapsImplMock() = default;

    // Mock methods
    MOCK_METHOD(gboolean, g_main_loop_is_running, (GMainLoop* loop), (override));
    MOCK_METHOD(gulong, g_signal_connect_data, (gpointer instance, const gchar *detailed_signal, GCallback c_handler,
                                         gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags), (override));
    MOCK_METHOD(guint, signal_handlers_disconnect_by_data, (gpointer instance, gpointer data), (override));
    MOCK_METHOD(guint, signal_handlers_disconnect_by_func, (gpointer instance, gpointer func, gpointer data), (override));
};

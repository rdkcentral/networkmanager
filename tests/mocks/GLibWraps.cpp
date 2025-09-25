#include "GLibWraps.h"
#include <gmock/gmock.h>


extern "C" gboolean __wrap_g_main_loop_is_running(GMainLoop* loop) {
    return GLibWraps::getInstance().g_main_loop_is_running(loop);
}

extern "C" gulong __wrap_g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
        gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags){
    return GLibWraps::getInstance().g_signal_connect_data(instance, detailed_signal, c_handler, data, destroy_data, connect_flags);
}

extern "C" guint __wrap_g_signal_handlers_disconnect_by_data(gpointer instance, gpointer data) {
    return GLibWraps::getInstance().signal_handlers_disconnect_by_data(instance, data);
}

extern "C" guint __wrap_g_signal_handlers_disconnect_by_func(gpointer instance, gpointer func, gpointer data) {
    return GLibWraps::getInstance().signal_handlers_disconnect_by_func(instance, func, data);
}

GLibWrapsImpl* GLibWraps::impl = nullptr;
GLibWraps::GLibWraps() {}

void GLibWraps::setImpl(GLibWrapsImpl* newImpl) {
    EXPECT_TRUE((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

GLibWraps& GLibWraps::getInstance() {
    static GLibWraps instance;
    return instance;
}

gboolean GLibWraps::g_main_loop_is_running(GMainLoop* loop) {
    EXPECT_NE(impl, nullptr);
    return impl->g_main_loop_is_running(loop);
}

gulong GLibWraps::g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
                                         gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags) {
    EXPECT_NE(impl, nullptr);
    return impl->g_signal_connect_data(instance, detailed_signal, c_handler, data, destroy_data, connect_flags);
}

guint GLibWraps::signal_handlers_disconnect_by_data(gpointer instance, gpointer data) {
    EXPECT_NE(impl, nullptr);
    return impl->signal_handlers_disconnect_by_data(instance, data);
}

guint GLibWraps::signal_handlers_disconnect_by_func(gpointer instance, gpointer func, gpointer data) {
    EXPECT_NE(impl, nullptr);
    return impl->signal_handlers_disconnect_by_func(instance, func, data);
}

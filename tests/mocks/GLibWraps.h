#pragma once

#include <glib.h>
#include <glib-object.h>
#include <string>

class GLibWrapsImpl {
public:
    virtual ~GLibWrapsImpl() = default;

    virtual gboolean g_main_loop_is_running(GMainLoop* loop) = 0;
    virtual gulong g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
                                         gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags) = 0;
    virtual guint signal_handlers_disconnect_by_data(gpointer instance, gpointer data) = 0;
    virtual guint signal_handlers_disconnect_by_func(gpointer instance, gpointer func, gpointer data) = 0;
};

class GLibWraps {
protected:
    static GLibWrapsImpl* impl;

public:
    GLibWraps();
    GLibWraps(const GLibWraps &obj) = delete;
    static void setImpl(GLibWrapsImpl* newImpl);
    static GLibWraps& getInstance();

    static gboolean g_main_loop_is_running(GMainLoop* loop);
    static gulong g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
                                         gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags) __attribute__((visibility("default")));
    static guint signal_handlers_disconnect_by_data(gpointer instance, gpointer data) __attribute__((visibility("default")));
    static guint signal_handlers_disconnect_by_func(gpointer instance, gpointer func, gpointer data) __attribute__((visibility("default")));
};

extern "C" {
    gulong __wrap_g_signal_connect_data(gpointer instance, const gchar *detailed_signal, GCallback c_handler,
                                             gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags) __attribute__((visibility("default")));
    guint __wrap_g_signal_handlers_disconnect_by_data(gpointer instance, gpointer data) __attribute__((visibility("default")));
    guint __wrap_g_signal_handlers_disconnect_by_func(gpointer instance, gpointer func, gpointer data) __attribute__((visibility("default")));
}

#pragma once

#include <glib.h>
// #include <glib-object.h>
#include <string>

class GLibWrapsImpl {
public:
    virtual ~GLibWrapsImpl() = default;

    virtual gboolean g_main_loop_is_running(GMainLoop* loop) = 0;
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
};

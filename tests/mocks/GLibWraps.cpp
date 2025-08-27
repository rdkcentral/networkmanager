#include "GLibWraps.h"
#include <gmock/gmock.h>

// // Define the real function to ensure it exists for the wrapper to call
// extern "C" gboolean __real_g_main_loop_is_running(GMainLoop* loop) {
//     // Default implementation if the real function can't be called
//     return FALSE;
// }

extern "C" gboolean __wrap_g_main_loop_is_running(GMainLoop* loop) {
    return GLibWraps::getInstance().g_main_loop_is_running(loop);
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

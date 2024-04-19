#include "output.h"
#include "log.h"

#ifdef ENABLE_X11
#include "x11/x.h"
#include "x11/screen.h"
#endif

#ifdef ENABLE_WAYLAND
#include "wayland/wl.h"
#endif

bool is_running_wayland(void) {
        char* wayland_display = getenv("WAYLAND_DISPLAY");
        return !(wayland_display == NULL);
}

#ifdef ENABLE_X11
const struct output output_x11 = {
        x_setup,
        x_free,

        x_win_create,
        x_win_destroy,

        x_win_show,
        x_win_hide,

        x_display_surface,
        x_win_get_context,

        get_active_screen,

        x_is_idle,
        have_fullscreen_window,

        x_get_scale,
};
#endif

#ifdef ENABLE_WAYLAND
const struct output output_wl = {
        wl_init,
        wl_deinit,

        wl_win_create,
        wl_win_destroy,

        wl_win_show,
        wl_win_hide,

        wl_display_surface,
        wl_win_get_context,

        wl_get_active_screen,

        wl_is_idle,
        wl_have_fullscreen_window,

        wl_get_scale,
};
#endif

#ifdef ENABLE_X11
const struct output* get_x11_output(void) {
        const struct output* output = &output_x11;
        if (output->init()) {
                return output;
        } else {
                LOG_E("Couldn't initialize X11 output. Aborting...");
        }
}
#endif

#ifdef ENABLE_WAYLAND
const struct output* get_wl_output(void) {
        const struct output* output = &output_wl;
        if (output->init()) {
                return output;
        } else {
#ifdef ENABLE_X11
                LOG_W("Couldn't initialize wayland output. Falling back to X11 output.");
                output->deinit();
                return get_x11_output();
#else
                DIE("Couldn't initialize wayland output");
#endif
        }
}
#endif

const struct output* output_create(bool force_xwayland)
{
#ifdef ENABLE_WAYLAND
        if ((!force_xwayland || !X11_SUPPORT) && is_running_wayland()) {
                LOG_I("Using Wayland output");
                if (force_xwayland)
                        LOG_W("Ignoring force_xwayland setting because X11 output was not compiled");
                return get_wl_output();
        }
#endif

#ifdef ENABLE_X11
        LOG_I("Using X11 output");
        return get_x11_output();
#endif

        DIE("No applicable ouput was found (X11, Wayland)");
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

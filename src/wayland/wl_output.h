#ifndef DUNST_WL_OUTPUT_H
#define DUNST_WL_OUTPUT_H
#include <stdint.h>
#include <stdbool.h>
#include <wayland-util.h>

struct dunst_output {
        uint32_t global_name;
        char *name;
        struct wl_output *wl_output;
        struct wl_list link;

        uint32_t scale;
        uint32_t subpixel; // TODO do something with it
        int32_t width, height;
        bool fullscreen;
        struct zwlr_foreign_toplevel_handle_v1 *fullscreen_toplevel; // the toplevel that is fullscreened on this output
};

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

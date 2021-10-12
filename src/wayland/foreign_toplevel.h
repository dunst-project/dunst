#ifndef DUNST_FOREIGN_TOPLEVEL_H
#define DUNST_FOREIGN_TOPLEVEL_H
#include <wayland-client.h>

enum toplevel_state_field {
        TOPLEVEL_STATE_ACTIVATED = (1 << 0),
        TOPLEVEL_STATE_FULLSCREEN = (1 << 1),
        TOPLEVEL_STATE_INVALID = (1 << 2),
};

struct toplevel_state {
        uint32_t state;
};

struct toplevel_v1 {
        struct wl_list link;
        struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel;
        struct wl_list output_list;

        uint32_t id;
        struct toplevel_state current, pending;
};

struct toplevel_output {
        struct dunst_output *dunst_output;
        struct wl_list link;
};

extern const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl;

extern struct wl_list toplevel_list;
#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

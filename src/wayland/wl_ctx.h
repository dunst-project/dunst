#ifndef DUNST_WL_PRIVATE_H_
#define DUNST_WL_PRIVATE_H_

#include <stdbool.h>
#include <glib.h>

#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-cursor.h>
#include <wayland-util.h>

#include "../output.h"
#include "libgwater-wayland.h"
#include "pool-buffer.h"

struct wl_ctx {
        GWaterWaylandSource *esrc;
        struct wl_display *display; // owned by esrc
        struct wl_registry *registry;
        struct wl_compositor *compositor;
        struct wl_shm *shm;
        struct zwlr_layer_shell_v1 *layer_shell;
        struct xdg_wm_base *xdg_shell;

        struct wl_list outputs; /* list of struct dunst_output */
        struct wl_list seats; /* list of struct dunst_seat */

        struct wl_surface *surface;
        struct dunst_output *surface_output;
        struct zwlr_layer_surface_v1 *layer_surface;
        struct xdg_surface *xdg_surface;
        struct xdg_toplevel *xdg_toplevel;
        struct dunst_output *layer_surface_output;
        struct wl_callback *frame_callback;
        struct org_kde_kwin_idle *idle_handler;
#ifdef HAVE_WL_CURSOR_SHAPE
        struct wp_cursor_shape_manager_v1 *cursor_shape_manager;
#endif
#ifdef HAVE_WL_EXT_IDLE_NOTIFY
        struct ext_idle_notifier_v1 *ext_idle_notifier;
#endif
        uint32_t toplevel_manager_name;
        struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager;
        bool configured;
        bool dirty;

        struct dimensions cur_dim;

        int32_t width, height;
        struct pool_buffer buffers[2];
        struct pool_buffer *current_buffer;
        struct wl_cursor_theme *cursor_theme;
        const struct wl_cursor_image *cursor_image;
        struct wl_surface *cursor_surface;
};

extern struct wl_ctx ctx;

void set_dirty(void);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

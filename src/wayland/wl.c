#define _POSIX_C_SOURCE 200112L
#include "wl.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include "protocols/xdg-output-unstable-v1-client-header.h"
#include "protocols/xdg-output-unstable-v1.h"
#include "protocols/xdg-shell-client-header.h"
#include "protocols/xdg-shell.h"
#include "protocols/wlr-layer-shell-unstable-v1-client-header.h"
#include "protocols/wlr-layer-shell-unstable-v1.h"
#include "protocols/idle-client-header.h"
#include "protocols/idle.h"
#include "pool-buffer.h"


#include "../log.h"
#include "../settings.h"
#include "../queues.h"
#include "libgwater-wayland.h"

struct window_wl {
        struct wl_surface *surface;
        struct zwlr_layer_surface_v1 *layer_surface;

        struct wl_buffer *buffer;

        cairo_surface_t *c_surface;
        cairo_t * c_ctx;
        struct dimensions dim;

        GWaterWaylandSource *esrc;

        char *data;
        size_t size;
};

struct wl_ctx {
        struct wl_display *display;
        struct wl_registry *registry;
        struct wl_compositor *compositor;
        struct wl_shm *shm;
        struct zwlr_layer_shell_v1 *layer_shell;
        struct zxdg_output_manager_v1 *xdg_output_manager;
        struct wl_seat *seat;

        struct wl_list outputs;

        struct wl_surface *surface;
        struct wl_output *surface_output;
        struct zwlr_layer_surface_v1 *layer_surface;
        struct wl_output *layer_surface_output;
        struct wl_callback *frame_callback;
        struct org_kde_kwin_idle *idle_handler;
        bool configured;
        bool dirty;
        bool is_idle;
        bool has_seat;

	struct {
		struct wl_pointer *wl_pointer;
		int32_t x, y;
	} pointer;

        struct dimensions cur_dim;

        int32_t width, height;
        struct pool_buffer buffers[2];
        struct pool_buffer *current_buffer;
};

struct dunst_output {
        uint32_t global_name;
        char *name;
        struct wl_output *wl_output;
        struct zxdg_output_v1 *xdg_output;
        struct wl_list link;

        uint32_t scale;
        uint32_t subpixel; // TODO do something with it
};


struct wl_ctx ctx;

static void noop() {
        // This space intentionally left blank
}


void set_dirty();
static void xdg_output_handle_name(void *data, struct zxdg_output_v1 *xdg_output,
                const char *name) {
        struct dunst_output *output = data;
        output->name = g_strdup(name);
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
        .logical_position = noop,
        .logical_size = noop,
        .done = noop,
        .name = xdg_output_handle_name,
        .description = noop,
};

static void get_xdg_output(struct dunst_output *output) {
        if (ctx.xdg_output_manager == NULL ||
                        output->xdg_output != NULL) {
                return;
        }

        output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
                ctx.xdg_output_manager, output->wl_output);
        zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener,
                output);
}

static void output_handle_geometry(void *data, struct wl_output *wl_output,
                int32_t x, int32_t y, int32_t phy_width, int32_t phy_height,
                int32_t subpixel, const char *make, const char *model,
                int32_t transform) {
        //TODO
        struct dunst_output *output = data;
        output->subpixel = subpixel;
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
                int32_t factor) {
        struct dunst_output *output = data;
        output->scale = factor;
}

static const struct wl_output_listener output_listener = {
        .geometry = output_handle_geometry,
        .mode = noop,
        .done = noop,
        .scale = output_handle_scale,
};

static void create_output( struct wl_output *wl_output, uint32_t global_name) {
        struct dunst_output *output = g_malloc0(sizeof(struct dunst_output));
        if (output == NULL) {
                fprintf(stderr, "allocation failed\n");
                return;
        }
        static int number = 0;
        LOG_I("New output %i - id %i", global_name, number);
        output->global_name = global_name;
        output->wl_output = wl_output;
        // TODO: Fix this
        output->scale = 1;
        wl_list_insert(&ctx.outputs, &output->link);

        wl_output_set_user_data(wl_output, output);
        wl_output_add_listener(wl_output, &output_listener, output);
        get_xdg_output(output);
        number++;
}

static void destroy_output(struct dunst_output *output) {
        if (ctx.surface_output == output->wl_output) {
                ctx.surface_output = NULL;
        }
        if (ctx.layer_surface_output == output->wl_output) {
                ctx.layer_surface_output = NULL;
        }
        wl_list_remove(&output->link);
        if (output->xdg_output != NULL) {
                zxdg_output_v1_destroy(output->xdg_output);
        }
        wl_output_destroy(output->wl_output);
        free(output->name);
        free(output);
}

static void pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
                uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
        ctx.pointer.x = wl_fixed_to_int(surface_x);
        ctx.pointer.y = wl_fixed_to_int(surface_y);
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                uint32_t serial, uint32_t time, uint32_t button,
                uint32_t button_state) {

        if (button_state == 0){
                // make sure it doesn't react twice
                return;
        }

        LOG_I("Pointer handle button %i: %i", button, button_state);
        enum mouse_action *acts;

        switch (button) {
                case BTN_LEFT:
                        acts = settings.mouse_left_click;
                        break;
                case BTN_MIDDLE:
                        acts = settings.mouse_middle_click;
                        break;
                case BTN_RIGHT:
                        acts = settings.mouse_right_click;
                        break;
                default:
                        LOG_W("Unsupported mouse button: '%d'", button);
                        return;
        }

        // TODO Extract this code into seperate function
        for (int i = 0; acts[i]; i++) {
                enum mouse_action act = acts[i];
                if (act == MOUSE_CLOSE_ALL) {
                        queues_history_push_all();
                        return;
                }

                if (act == MOUSE_DO_ACTION || act == MOUSE_CLOSE_CURRENT) {
                        int y = settings.separator_height;
                        struct notification *n = NULL;
                        int first = true;
                        for (const GList *iter = queues_get_displayed(); iter;
                             iter = iter->next) {
                                n = iter->data;
                                if (ctx.pointer.y > y && ctx.pointer.y < y + n->displayed_height)
                                        break;

                                y += n->displayed_height + settings.separator_height;
                                if (first)
                                        y += settings.frame_width;
                        }

                        if (n) {
                                if (act == MOUSE_CLOSE_CURRENT)
                                        queues_notification_close(n, REASON_USER);
                                else
                                        notification_do_action(n);
                        }
                }
        }
        wake_up();
}

static const struct wl_pointer_listener pointer_listener = {
        .enter = noop,
        .leave = noop,
        .motion = pointer_handle_motion,
        .button = pointer_handle_button,
        .axis = noop,
};

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
                uint32_t capabilities) {

        if (ctx.pointer.wl_pointer != NULL) {
                wl_pointer_release(ctx.pointer.wl_pointer);
                ctx.pointer.wl_pointer = NULL;
        }
        if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
                ctx.pointer.wl_pointer = wl_seat_get_pointer(wl_seat);
                wl_pointer_add_listener(ctx.pointer.wl_pointer,
                        &pointer_listener, ctx.seat);
                LOG_I("Adding pointer");
        }
}

static const struct wl_seat_listener seat_listener = {
        .capabilities = seat_handle_capabilities,
        .name = noop,
};

// FIXME: Snipped touch handling

static void surface_handle_enter(void *data, struct wl_surface *surface,
                struct wl_output *wl_output) {
        // Don't bother keeping a list of outputs, a layer surface can only be on
        // one output a a time
        ctx.surface_output = wl_output_get_user_data(wl_output);
        set_dirty();
}

static void surface_handle_leave(void *data, struct wl_surface *surface,
                struct wl_output *wl_output) {
        ctx.surface_output = NULL;
}

static const struct wl_surface_listener surface_listener = {
        .enter = surface_handle_enter,
        .leave = surface_handle_leave,
};


static void schedule_frame_and_commit();
static void send_frame();

static void layer_surface_handle_configure(void *data,
                struct zwlr_layer_surface_v1 *surface,
                uint32_t serial, uint32_t width, uint32_t height) {
        ctx.configured = true;
        ctx.width = width;
        ctx.height = height;

        zwlr_layer_surface_v1_ack_configure(surface, serial);
        send_frame();
}

static void layer_surface_handle_closed(void *data,
                struct zwlr_layer_surface_v1 *surface) {
        LOG_W("Destroying layer");
        zwlr_layer_surface_v1_destroy(ctx.layer_surface);
        ctx.layer_surface = NULL;

        wl_surface_destroy(ctx.surface);
        ctx.surface = NULL;

        if (ctx.frame_callback) {
                wl_callback_destroy(ctx.frame_callback);
                ctx.frame_callback = NULL;
                ctx.dirty = true;
        }

        if (ctx.configured) {
                ctx.configured = false;
                ctx.width = ctx.height = 0;
                ctx.dirty = true;
        }

        if (ctx.dirty) {
                schedule_frame_and_commit();
        }
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
        .configure = layer_surface_handle_configure,
        .closed = layer_surface_handle_closed,
};


static void idle_start (void *data, struct org_kde_kwin_idle_timeout *org_kde_kwin_idle_timeout){
        ctx.is_idle = true;
        LOG_I("User went idle");
}
static void idle_stop (void *data, struct org_kde_kwin_idle_timeout *org_kde_kwin_idle_timeout) {
        ctx.is_idle = false;
        LOG_I("User isn't idle anymore");
}

static const struct org_kde_kwin_idle_timeout_listener idle_timeout_listener = {
        .idle = idle_start,
        .resumed = idle_stop,
};

static void add_seat_to_idle_handler(struct wl_seat *seat){
        uint32_t timeout_ms = settings.idle_threshold/1000;
        struct org_kde_kwin_idle_timeout *idle_timeout = org_kde_kwin_idle_get_idle_timeout(ctx.idle_handler, ctx.seat, timeout_ms);
        org_kde_kwin_idle_timeout_add_listener(idle_timeout, &idle_timeout_listener, 0);
}

static void handle_global(void *data, struct wl_registry *registry,
                uint32_t name, const char *interface, uint32_t version) {
        if (strcmp(interface, wl_compositor_interface.name) == 0) {
                ctx.compositor = wl_registry_bind(registry, name,
                        &wl_compositor_interface, 4);
        } else if (strcmp(interface, wl_shm_interface.name) == 0) {
                ctx.shm = wl_registry_bind(registry, name,
                        &wl_shm_interface, 1);
        } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
                ctx.layer_shell = wl_registry_bind(registry, name,
                        &zwlr_layer_shell_v1_interface, 1);
        } else if (strcmp(interface, wl_seat_interface.name) == 0) {
                ctx.seat = wl_registry_bind(registry, name, &wl_seat_interface, 3);
                wl_seat_add_listener(ctx.seat, &seat_listener, ctx.seat);
                add_seat_to_idle_handler(ctx.seat);
                ctx.has_seat = true;
        } else if (strcmp(interface, wl_output_interface.name) == 0) {
                struct wl_output *output =
                        wl_registry_bind(registry, name, &wl_output_interface, 3);
                create_output(output, name);
        } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0 &&
                        version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
                ctx.xdg_output_manager = wl_registry_bind(registry, name,
                        &zxdg_output_manager_v1_interface,
                        ZXDG_OUTPUT_V1_NAME_SINCE_VERSION);
        } else if (strcmp(interface, org_kde_kwin_idle_interface.name) == 0 &&
                        version >= ORG_KDE_KWIN_IDLE_TIMEOUT_IDLE_SINCE_VERSION) {
                ctx.idle_handler = wl_registry_bind(registry, name, &org_kde_kwin_idle_interface, 1);
        }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                uint32_t name) {
        struct dunst_output *output, *tmp;
        wl_list_for_each_safe(output, tmp, &ctx.outputs, link) {
                if (output->global_name == name) {
                        destroy_output(output);
                        break;
                }
        }
}

static const struct wl_registry_listener registry_listener = {
        .global = handle_global,
        .global_remove = handle_global_remove,
};

bool init_wayland() {
        wl_list_init(&ctx.outputs);
        //wl_list_init(&ctx.seats);

        ctx.display = wl_display_connect(NULL);

        if (ctx.display == NULL) {
                fprintf(stderr, "failed to create display\n");
                return false;
        }

        ctx.registry = wl_display_get_registry(ctx.display);
        wl_registry_add_listener(ctx.registry, &registry_listener, NULL);
        wl_display_roundtrip(ctx.display);

        if (ctx.compositor == NULL) {
                fprintf(stderr, "compositor doesn't support wl_compositor\n");
                return false;
        }
        if (ctx.shm == NULL) {
                fprintf(stderr, "compositor doesn't support wl_shm\n");
                return false;
        }
        if (ctx.layer_shell == NULL) {
                fprintf(stderr, "compositor doesn't support zwlr_layer_shell_v1\n");
                return false;
        }

        if (ctx.xdg_output_manager != NULL) {
                struct dunst_output *output;
                wl_list_for_each(output, &ctx.outputs, link) {
                        get_xdg_output(output);
                }
                wl_display_roundtrip(ctx.display);
        }
        //if (ctx.xdg_output_manager == NULL &&
        //                strcmp(ctx.config.output, "") != 0) {
        //        fprintf(stderr, "warning: configured an output but compositor doesn't "
        //                "support xdg-output-unstable-v1 version 2\n");
        //}

        return true;
}

void finish_wayland() {
        if (ctx.layer_surface != NULL) {
                zwlr_layer_surface_v1_destroy(ctx.layer_surface);
        }
        if (ctx.surface != NULL) {
                wl_surface_destroy(ctx.surface);
        }
        finish_buffer(&ctx.buffers[0]);
        finish_buffer(&ctx.buffers[1]);

        struct dunst_output *output, *output_tmp;
        wl_list_for_each_safe(output, output_tmp, &ctx.outputs, link) {
                destroy_output(output);
        }

        free(ctx.seat);

        if (ctx.xdg_output_manager != NULL) {
                zxdg_output_manager_v1_destroy(ctx.xdg_output_manager);
        }
        zwlr_layer_shell_v1_destroy(ctx.layer_shell);
        wl_compositor_destroy(ctx.compositor);
        wl_shm_destroy(ctx.shm);
        wl_registry_destroy(ctx.registry);
        wl_display_disconnect(ctx.display);
}

static struct dunst_output *get_configured_output() {
        struct dunst_output *output;
        // FIXME Make sure the returned output corresponds to the monitor number configured in the dunstrc
        wl_list_for_each(output, &ctx.outputs, link) {
                return output;
        }

        return NULL;
}

static void schedule_frame_and_commit();

// Draw and commit a new frame.
static void send_frame() {
        int scale = 1;

        struct dunst_output *output = get_configured_output();
        int height = ctx.cur_dim.h;

        // There are two cases where we want to tear down the surface: zero
        // notifications (height = 0) or moving between outputs.
        if (height == 0 || ctx.layer_surface_output != output->wl_output) {
                if (ctx.layer_surface != NULL) {
                        zwlr_layer_surface_v1_destroy(ctx.layer_surface);
                        ctx.layer_surface = NULL;
                }
                if (ctx.surface != NULL) {
                        wl_surface_destroy(ctx.surface);
                        ctx.surface = NULL;
                }
                ctx.width = ctx.height = 0;
                ctx.surface_output = NULL;
                ctx.configured = false;
        }

        // If there are no notifications, there's no point in recreating the
        // surface right now.
        if (height == 0) {
                ctx.dirty = false;
                wl_display_roundtrip(ctx.display);
                return;
        }

        // If we've made it here, there is something to draw. If the surface
        // doesn't exist (this is the first notification, or we moved to a
        // different output), we need to create it.
        if (ctx.layer_surface == NULL) {
                struct wl_output *wl_output = NULL;
                if (output != NULL) {
                        LOG_I("Output is not null");
                        wl_output = output->wl_output;
                } else
                        LOG_I("output is null");
                ctx.layer_surface_output = output->wl_output;
                ctx.surface = wl_compositor_create_surface(ctx.compositor);
                wl_surface_add_listener(ctx.surface, &surface_listener, NULL);

                ctx.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
                        ctx.layer_shell, ctx.surface, wl_output,
                        ZWLR_LAYER_SHELL_V1_LAYER_TOP, "notifications");
                zwlr_layer_surface_v1_add_listener(ctx.layer_surface,
                        &layer_surface_listener, NULL);

                // Because we're creating a new surface, we aren't going to draw
                // anything into it during this call. We don't know what size the
                // surface will be until we've asked the compositor for what we want
                // and it has responded with what it actually gave us. We also know
                // that the height we would _like_ to draw (greater than zero, or we
                // would have bailed already) is different from our ctx.height
                // (which has to be zero here), so we can fall through to the next
                // block to let it set the size for us.
        }

        assert(ctx.layer_surface);

        // We now want to resize the surface if it isn't the right size. If the
        // surface is brand new, it doesn't even have a size yet. If it already
        // exists, we might need to resize if the list of notifications has changed
        // since the last time we drew.
        if (ctx.height != height) {
                struct dimensions dim = ctx.cur_dim;
                // Set window size
                LOG_D("Wl: Window dimensions %ix%i", dim.w, dim.h);
                LOG_D("Wl: Window position %ix%i", dim.x, dim.y);
                zwlr_layer_surface_v1_set_size(ctx.layer_surface,
                                dim.w, dim.h);

                uint32_t anchor = 0;
                if (settings.geometry.negative_x){
                        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
                }
                else{
                        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
                }

                if (settings.geometry.negative_y){
                        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
                }
                else{
                        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
                }

                // Put the window at the right position
                zwlr_layer_surface_v1_set_anchor(ctx.layer_surface,
                        anchor);
                zwlr_layer_surface_v1_set_margin(ctx.layer_surface,
                                abs(settings.geometry.y), // top
                                abs(settings.geometry.x), // right
                                abs(settings.geometry.y), // bottom
                                abs(settings.geometry.x));// left

                wl_surface_commit(ctx.surface);

                // Now we're going to bail without drawing anything. This gives the
                // compositor a chance to create the surface and tell us what size we
                // were actually granted, which may be smaller than what we asked for
                // depending on the screen size and layout of other layer surfaces.
                // This information is provided in layer_surface_handle_configure,
                // which will then call send_frame again. When that call happens, the
                // layer surface will exist and the height will hopefully match what
                // we asked for. That means we won't return here, and will actually
                // draw into the surface down below.
                // TODO: If the compositor doesn't send a configure with the size we
                // requested, we'll enter an infinite loop. We need to keep track of
                // the fact that a request was sent separately from what height we are.
                wl_display_roundtrip(ctx.display);
                return;
        }

        assert(ctx.configured);

        // Yay we can finally draw something!
        //struct wl_region *input_region = get_input_region();
        //wl_surface_set_input_region(ctx.surface, input_region);
        //wl_region_destroy(input_region);

        wl_surface_set_buffer_scale(ctx.surface, scale);
        wl_surface_damage_buffer(ctx.surface, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_attach(ctx.surface, ctx.current_buffer->buffer, 0, 0);
        ctx.current_buffer->busy = true;

        // Schedule a frame in case the state becomes dirty again
        schedule_frame_and_commit();

        ctx.dirty = false;
}

static void frame_handle_done(void *data, struct wl_callback *callback,
                uint32_t time) {
        wl_callback_destroy(ctx.frame_callback);
        ctx.frame_callback = NULL;

        // Only draw again if we need to
        if (ctx.dirty) {
                send_frame();
        }
}

static const struct wl_callback_listener frame_listener = {
        .done = frame_handle_done,
};

static void schedule_frame_and_commit() {
        if (ctx.frame_callback) {
                return;
        }
        if (ctx.surface == NULL) {
                // We don't yet have a surface, create it immediately
                send_frame();
                return;
        }
        ctx.frame_callback = wl_surface_frame(ctx.surface);
        wl_callback_add_listener(ctx.frame_callback, &frame_listener, NULL);
        wl_surface_commit(ctx.surface);
}

void set_dirty() {
        if (ctx.dirty) {
                return;
        }
        ctx.dirty = true;
        schedule_frame_and_commit();
}

void wl_init(void) {
        init_wayland();
}

void wl_deinit(void) {
}

window wl_win_create(void) {
        struct window_wl *win = g_malloc0(sizeof(struct window_wl));

        win->esrc = g_water_wayland_source_new_for_display(NULL, ctx.display);
        return win;
}

void wl_win_destroy(window winptr) {
        struct window_wl *win = (struct window_wl*)winptr;

        g_water_wayland_source_free(win->esrc);
        // FIXME: Dealloc everything
        g_free(win);
}

void wl_win_show(window win) {
}

void wl_win_hide(window win) {
        LOG_I("Wayland: Hiding window");
        ctx.cur_dim.h = 0;
        set_dirty();
        wl_display_roundtrip(ctx.display);
}

void wl_display_surface(cairo_surface_t *srf, window winptr, const struct dimensions* dim) {
        struct window_wl *win = (struct window_wl*)winptr;
        ctx.current_buffer = get_next_buffer(ctx.shm, ctx.buffers, dim->w, dim->h);

        cairo_t *c = ctx.current_buffer->cairo;
        cairo_save(c);
        cairo_set_source_surface(c, srf, 0, 0);
        cairo_rectangle(c, 0, 0, dim->w, dim->h);
        cairo_fill(c);
        cairo_restore(c);

        ctx.cur_dim = *dim;

        set_dirty();
        wl_display_roundtrip(ctx.display);
}

bool wl_win_visible(window win) {
        return true;
}

cairo_t* wl_win_get_context(window winptr) {
        struct window_wl *win = (struct window_wl*)winptr;
        ctx.current_buffer = get_next_buffer(ctx.shm, ctx.buffers, 500, 500);
        win->c_surface = ctx.current_buffer->surface;
        win->c_ctx = ctx.current_buffer->cairo;
        return win->c_ctx;
}

const struct screen_info* wl_get_active_screen(void) {
        // TODO Screen size detection
        static struct screen_info scr = {
                .w = 1920,
                .h = 1080,
                .x = 0,
                .y = 0,
                .id = 0,
                .mmh = 500
        };
        return &scr;
}

bool wl_is_idle(void) {
        LOG_I("Idle status queried: %i", ctx.is_idle);
        // When the user doesn't have a seat, or their compositor doesn't support the idle
        // protocol, we'll assume that they are not idle.
        if (settings.idle_threshold == 0 || ctx.has_seat == false || ctx.idle_handler == NULL) {
                return false;
        } else {
                return ctx.is_idle;
        }
}
bool wl_have_fullscreen_window(void) {
        return false;
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

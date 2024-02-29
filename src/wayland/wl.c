#define _POSIX_C_SOURCE 200112L
#include "wl.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>
#include <wayland-cursor.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <string.h>
#include <glib.h>

#include "protocols/xdg-shell-client-header.h"
#include "protocols/xdg-shell.h"
#include "protocols/wlr-layer-shell-unstable-v1-client-header.h"
#include "protocols/wlr-layer-shell-unstable-v1.h"
#include "protocols/wlr-foreign-toplevel-management-unstable-v1-client-header.h"
#include "protocols/wlr-foreign-toplevel-management-unstable-v1.h"
#include "protocols/idle-client-header.h"
#include "protocols/idle.h"
#include "pool-buffer.h"

#ifdef HAVE_WL_CURSOR_SHAPE
#include "protocols/cursor-shape-v1-client-header.h"
#include "protocols/cursor-shape-v1.h"
#include "protocols/tablet-unstable-v2.h"
#endif

#ifdef HAVE_WL_EXT_IDLE_NOTIFY
#include "protocols/ext-idle-notify-v1-client-header.h"
#include "protocols/ext-idle-notify-v1.h"
#endif

#include "../log.h"
#include "../settings.h"
#include "../queues.h"
#include "../input.h"
#include "libgwater-wayland.h"
#include "foreign_toplevel.h"
#include "wl_output.h"

#define MAX_TOUCHPOINTS 10

struct window_wl {
        cairo_surface_t *c_surface;
        cairo_t * c_ctx;
};

struct dunst_seat {
        struct wl_list link;
        uint32_t global_name;
        char *name;
        struct wl_seat *wl_seat;
        struct org_kde_kwin_idle_timeout *idle_timeout;
#ifdef HAVE_WL_EXT_IDLE_NOTIFY
        struct ext_idle_notification_v1 *ext_idle_notification;
#endif
        bool is_idle;

        struct {
                struct wl_pointer *wl_pointer;
                int32_t x, y;
        } pointer;

        struct {
                struct wl_touch *wl_touch;
                struct {
                        int32_t x, y;
                } pts[MAX_TOUCHPOINTS];
        } touch;
};

struct wl_ctx {
        GWaterWaylandSource *esrc;
        struct wl_display *display; // owned by esrc
        struct wl_registry *registry;
        struct wl_compositor *compositor;
        struct wl_shm *shm;
        struct zwlr_layer_shell_v1 *layer_shell;

        struct wl_list outputs;
        struct wl_list seats;

        struct wl_surface *surface;
        struct dunst_output *surface_output;
        struct zwlr_layer_surface_v1 *layer_surface;
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

struct wl_ctx ctx;

void set_dirty(void);

static void output_handle_geometry(void *data, struct wl_output *wl_output,
                int32_t x, int32_t y, int32_t phy_width, int32_t phy_height,
                int32_t subpixel, const char *make, const char *model,
                int32_t transform) {
        //TODO do something with the subpixel data
        struct dunst_output *output = data;
        output->subpixel = subpixel;
}
static void output_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                int32_t width, int32_t height, int32_t refresh) {
        struct dunst_output *output = data;
        output->width = width;
        output->height = height;
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
                int32_t factor) {
        struct dunst_output *output = data;
        output->scale = factor;

        wake_up();
}

#ifdef WL_OUTPUT_NAME_SINCE_VERSION
static void output_handle_name(void *data, struct wl_output *wl_output,
                const char *name) {
        struct dunst_output *output = data;
        output->name = g_strdup(name);
        LOG_D("Output global %" PRIu32 " name %s", output->global_name, name);
}

static void output_handle_description(void *data, struct wl_output *output, const char* description) {
        // do nothing
}
#endif

static void output_listener_done_handler(void *data, struct wl_output *output) {
        // do nothing
}

static const struct wl_output_listener output_listener = {
        .geometry = output_handle_geometry,
        .mode = output_handle_mode,
        .done = output_listener_done_handler,
        .scale = output_handle_scale,
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
        .name = output_handle_name,
        .description = output_handle_description,
#endif
};

static void create_output(struct wl_registry *registry, uint32_t global_name, uint32_t version) {
        struct dunst_output *output = g_malloc0(sizeof(struct dunst_output));
        if (output == NULL) {
                LOG_E("allocation failed");
                return;
        }

        uint32_t max_version = 3;
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
        max_version = WL_OUTPUT_NAME_SINCE_VERSION;
#endif
        bool recreate_surface = false;
        static int number = 0;
        LOG_I("New output found - id %i", number);
        output->global_name = global_name;
        output->wl_output = wl_registry_bind(registry, global_name, &wl_output_interface,
                        CLAMP(version, 3, max_version));
        output->scale = 1;
        output->fullscreen = false;

        recreate_surface = wl_list_empty(&ctx.outputs);

        wl_list_insert(&ctx.outputs, &output->link);

        wl_output_set_user_data(output->wl_output, output);
        wl_output_add_listener(output->wl_output, &output_listener, output);
        number++;

        if (recreate_surface) {
                // We had no outputs, force our surface to redraw
                set_dirty();
        }
}

static void destroy_output(struct dunst_output *output) {
        if (ctx.surface_output == output) {
                ctx.surface_output = NULL;
        }
        if (ctx.layer_surface_output == output) {
                ctx.layer_surface_output = NULL;
        }
        wl_list_remove(&output->link);
        wl_output_destroy(output->wl_output);
        g_free(output->name);
        g_free(output);
}

static void touch_handle_motion(void *data, struct wl_touch *wl_touch,
                uint32_t time, int32_t id,
                wl_fixed_t surface_x, wl_fixed_t surface_y) {
        struct dunst_seat *seat = data;

        if (id >= MAX_TOUCHPOINTS) {
                return;
        }
        seat->touch.pts[id].x = wl_fixed_to_int(surface_x);
        seat->touch.pts[id].y = wl_fixed_to_int(surface_y);
}

static void touch_handle_down(void *data, struct wl_touch *wl_touch,
                uint32_t serial, uint32_t time, struct wl_surface *sfc, int32_t id,
                wl_fixed_t surface_x, wl_fixed_t surface_y) {
        struct dunst_seat *seat = data;

        if (id >= MAX_TOUCHPOINTS) {
                return;
        }
        seat->touch.pts[id].x = wl_fixed_to_int(surface_x);
        seat->touch.pts[id].y = wl_fixed_to_int(surface_y);
}

static void touch_handle_up(void *data, struct wl_touch *wl_touch,
                uint32_t serial, uint32_t time, int32_t id) {
        struct dunst_seat *seat = data;

        if (id >= MAX_TOUCHPOINTS) {
                return;
        }
        input_handle_click(BTN_TOUCH, false,
                        seat->touch.pts[id].x, seat->touch.pts[id].y);

}

static void pointer_handle_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
        // Change the mouse cursor to "left_ptr"
#ifdef HAVE_WL_CURSOR_SHAPE
        if (ctx.cursor_shape_manager != NULL) {
                struct wp_cursor_shape_device_v1 *device =
                        wp_cursor_shape_manager_v1_get_pointer(ctx.cursor_shape_manager, wl_pointer);
                wp_cursor_shape_device_v1_set_shape(device, serial,
                                WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
                wp_cursor_shape_device_v1_destroy(device);
                return;
        }
#endif
        if (ctx.cursor_theme != NULL) {
                wl_pointer_set_cursor(wl_pointer, serial, ctx.cursor_surface, ctx.cursor_image->hotspot_x, ctx.cursor_image->hotspot_y);
        }
}

static void pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
                uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
        struct dunst_seat *seat = data;

        seat->pointer.x = wl_fixed_to_int(surface_x);
        seat->pointer.y = wl_fixed_to_int(surface_y);
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                uint32_t serial, uint32_t time, uint32_t button,
                uint32_t button_state) {
        struct dunst_seat *seat = data;

        input_handle_click(button, button_state, seat->pointer.x, seat->pointer.y);
}

static void pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
                uint32_t serial, struct wl_surface *surface) {
}

static void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
                uint32_t time, uint32_t axis, wl_fixed_t value) {
}

static const struct wl_pointer_listener pointer_listener = {
        .enter = pointer_handle_enter,
        .leave = pointer_handle_leave,
        .motion = pointer_handle_motion,
        .button = pointer_handle_button,
        .axis = pointer_handle_axis,
};

static void touch_handle_frame(void *data, struct wl_touch *wl_touch) {
}

static void touch_handle_cancel(void *data, struct wl_touch *wl_touch) {
}

static const struct wl_touch_listener touch_listener = {
        .down = touch_handle_down,
        .up = touch_handle_up,
        .motion = touch_handle_motion,
        .frame = touch_handle_frame,
        .cancel = touch_handle_cancel,
};

static void destroy_seat_pointer(struct dunst_seat *seat) {
        wl_pointer_release(seat->pointer.wl_pointer);
        seat->pointer.wl_pointer = NULL;
}

static void destroy_seat_touch(struct dunst_seat *seat) {
        wl_touch_release(seat->touch.wl_touch);
        seat->touch.wl_touch = NULL;
}

static void destroy_seat(struct dunst_seat *seat) {
        if (seat == NULL) {
                return;
        }

        if (seat->pointer.wl_pointer != NULL) {
                destroy_seat_pointer(seat);
        }

        if (seat->touch.wl_touch != NULL) {
                destroy_seat_touch(seat);
        }

        if (seat->idle_timeout != NULL) {
                org_kde_kwin_idle_timeout_release(seat->idle_timeout);
                seat->idle_timeout = NULL;
        }

#ifdef HAVE_WL_EXT_IDLE_NOTIFY
        if (seat->ext_idle_notification != NULL) {
                ext_idle_notification_v1_destroy(seat->ext_idle_notification);
                seat->ext_idle_notification = NULL;
        }
#endif

        wl_list_remove(&seat->link);

#ifdef WL_SEAT_RELEASE_SINCE_VERSION
        if (wl_seat_get_version(seat->wl_seat) >= WL_SEAT_RELEASE_SINCE_VERSION) {
                wl_seat_release(seat->wl_seat);
        } else
#endif
        wl_seat_destroy(seat->wl_seat);
        seat->wl_seat = NULL;

        g_free(seat->name);
        seat->name = NULL;

        g_free(seat);
}

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
                uint32_t capabilities) {
        struct dunst_seat *seat = data;

        if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
                if (seat->pointer.wl_pointer == NULL) {
                        seat->pointer.wl_pointer = wl_seat_get_pointer(wl_seat);
                        wl_pointer_add_listener(seat->pointer.wl_pointer,
                                &pointer_listener, seat);
                }
        } else if (seat->pointer.wl_pointer != NULL) {
                destroy_seat_pointer(seat);
        }

        if (capabilities & WL_SEAT_CAPABILITY_TOUCH) {
                if (seat->touch.wl_touch == NULL) {
                        seat->touch.wl_touch = wl_seat_get_touch(wl_seat);
                        wl_touch_add_listener(seat->touch.wl_touch,
                                &touch_listener, seat);
                }
        } else if (seat->touch.wl_touch != NULL) {
                destroy_seat_touch(seat);
        }
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat, const char *name) {
        struct dunst_seat *seat = data;

        seat->name = g_strdup(name);
        LOG_I("New seat found - id %" PRIu32 " name %s", seat->global_name, seat->name);
}

static const struct wl_seat_listener seat_listener = {
        .capabilities = seat_handle_capabilities,
        .name = seat_handle_name,
};

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


static void schedule_frame_and_commit(void);
static void send_frame(void);

static void layer_surface_handle_configure(void *data,
                struct zwlr_layer_surface_v1 *surface,
                uint32_t serial, uint32_t width, uint32_t height) {
        zwlr_layer_surface_v1_ack_configure(surface, serial);

        if (ctx.configured &&
                        ctx.width == (int32_t) width &&
                        ctx.height == (int32_t) height) {
                wl_surface_commit(ctx.surface);
                return;
        }

        ctx.configured = true;
        ctx.width = width;
        ctx.height = height;

        send_frame();
}

static void layer_surface_handle_closed(void *data,
                struct zwlr_layer_surface_v1 *surface) {
        LOG_I("Destroying layer");
        if (ctx.layer_surface)
                zwlr_layer_surface_v1_destroy(ctx.layer_surface);
        ctx.layer_surface = NULL;

        if (ctx.surface)
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


static void idle_start (void *data, struct org_kde_kwin_idle_timeout *org_kde_kwin_idle_timeout) {
        struct dunst_seat *seat = data;

        seat->is_idle = true;
        LOG_D("User went idle on seat \"%s\"", seat->name);
}
static void idle_stop (void *data, struct org_kde_kwin_idle_timeout *org_kde_kwin_idle_timeout) {
        struct dunst_seat *seat = data;

        seat->is_idle = false;
        LOG_D("User isn't idle anymore on seat \"%s\"", seat->name);
}

static const struct org_kde_kwin_idle_timeout_listener idle_timeout_listener = {
        .idle = idle_start,
        .resumed = idle_stop,
};

#ifdef HAVE_WL_EXT_IDLE_NOTIFY
static void ext_idle_notification_handle_idled(void *data, struct ext_idle_notification_v1 *notification) {
        struct dunst_seat *seat = data;

        seat->is_idle = true;
        LOG_D("User went idle on seat \"%s\"", seat->name);
}

static void ext_idle_notification_handle_resumed(void *data, struct ext_idle_notification_v1 *notification) {
        struct dunst_seat *seat = data;

        seat->is_idle = false;
        LOG_D("User isn't idle anymore on seat \"%s\"", seat->name);
}

static const struct ext_idle_notification_v1_listener ext_idle_notification_listener = {
        .idled = ext_idle_notification_handle_idled,
        .resumed = ext_idle_notification_handle_resumed,
};
#endif

static void add_seat_to_idle_handler(struct dunst_seat *seat) {
        if (settings.idle_threshold <= 0) {
                return;
        }
        uint32_t timeout_ms = settings.idle_threshold/1000;

#ifdef HAVE_WL_EXT_IDLE_NOTIFY
        if (ctx.ext_idle_notifier != NULL) {
                if (seat->ext_idle_notification == NULL) {
                        seat->ext_idle_notification =
                                ext_idle_notifier_v1_get_idle_notification(ctx.ext_idle_notifier,
                                                timeout_ms, seat->wl_seat);
                        ext_idle_notification_v1_add_listener(seat->ext_idle_notification,
                                        &ext_idle_notification_listener, seat);
                }
                // In the unlikely case where we already set up org_kde_kwin_idle_timeout, destroy it
                if (seat->idle_timeout != NULL) {
                        org_kde_kwin_idle_timeout_release(seat->idle_timeout);
                        seat->idle_timeout = NULL;
                }
                return;
        }
#endif
        if (ctx.idle_handler != NULL && seat->idle_timeout == NULL) {
                seat->idle_timeout = org_kde_kwin_idle_get_idle_timeout(ctx.idle_handler, seat->wl_seat, timeout_ms);
                org_kde_kwin_idle_timeout_add_listener(seat->idle_timeout, &idle_timeout_listener, seat);
        }
}

static void create_seat(struct wl_registry *registry, uint32_t global_name, uint32_t version) {
        struct dunst_seat *seat = g_malloc0(sizeof(struct dunst_seat));
        if (seat == NULL) {
                LOG_E("allocation failed");
                return;
        }
        seat->global_name = global_name;
        seat->wl_seat = wl_registry_bind(registry, global_name, &wl_seat_interface, 3);
        wl_seat_add_listener(seat->wl_seat, &seat_listener, seat);
        add_seat_to_idle_handler(seat);
        wl_list_insert(&ctx.seats, &seat->link);
}

// Warning, can return NULL
static struct dunst_output *get_configured_output(void) {
        int n = 0;
        int target_monitor = settings.monitor_num;
        const char *name = settings.monitor;

        struct dunst_output *first_output = NULL, *configured_output = NULL,
                            *tmp_output = NULL;
        wl_list_for_each(tmp_output, &ctx.outputs, link) {
                if (n == 0)
                        first_output = tmp_output;
                if (n == target_monitor)
                        configured_output = tmp_output;
                if (g_strcmp0(name, tmp_output->name) == 0)
                        configured_output = tmp_output;
                n++;
        }

        // There's only 1 output, so return that
        if (n == 1)
                return first_output;

        switch (settings.f_mode){
                case FOLLOW_NONE: ; // this semicolon is neccesary
                        if (!configured_output) {
                                LOG_W("Monitor %s doesn't exist, using focused monitor", settings.monitor);
                        }
                        return configured_output;
                case FOLLOW_MOUSE:
                        // fallthrough
                case FOLLOW_KEYBOARD:
                        // fallthrough
                default:
                        return NULL;
        }
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
                create_seat(registry, name, version);
                LOG_D("Binding to seat %i", name);
        } else if (strcmp(interface, wl_output_interface.name) == 0) {
                create_output(registry, name, version);
                LOG_D("Binding to output %i", name);
        } else if (strcmp(interface, org_kde_kwin_idle_interface.name) == 0 &&
                        version >= ORG_KDE_KWIN_IDLE_TIMEOUT_IDLE_SINCE_VERSION) {
#ifdef HAVE_WL_EXT_IDLE_NOTIFY
                if (ctx.ext_idle_notifier)
                        return;
#endif
                ctx.idle_handler = wl_registry_bind(registry, name, &org_kde_kwin_idle_interface, 1);
                LOG_D("Found org_kde_kwin_idle");
#ifdef HAVE_WL_EXT_IDLE_NOTIFY
        } else if (strcmp(interface, ext_idle_notifier_v1_interface.name) == 0) {
                ctx.ext_idle_notifier = wl_registry_bind(registry, name, &ext_idle_notifier_v1_interface, 1);
                LOG_D("Found ext-idle-notify-v1");
#endif
#ifdef HAVE_WL_CURSOR_SHAPE
        } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
                ctx.cursor_shape_manager = wl_registry_bind(registry, name,
                                &wp_cursor_shape_manager_v1_interface, 1);
#endif
        } else if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0 &&
                        version >= ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN_SINCE_VERSION) {
                LOG_D("Found toplevel manager %i", name);
                ctx.toplevel_manager_name = name;
        }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
                uint32_t name) {
        struct dunst_output *output, *tmp;
        wl_list_for_each_safe(output, tmp, &ctx.outputs, link) {
                if (output->global_name == name) {
                        destroy_output(output);
                        return;
                }
        }

        struct dunst_seat *seat, *tmp_seat;
        wl_list_for_each_safe(seat, tmp_seat, &ctx.seats, link) {
                if (seat->global_name == name) {
                        destroy_seat(seat);
                        return;
                }
        }
}

static const struct wl_registry_listener registry_listener = {
        .global = handle_global,
        .global_remove = handle_global_remove,
};

bool wl_init(void) {
        wl_list_init(&ctx.outputs);
        wl_list_init(&ctx.seats);
        wl_list_init(&toplevel_list);

        ctx.esrc = g_water_wayland_source_new(NULL, NULL);
        ctx.display = g_water_wayland_source_get_display(ctx.esrc);
#if GLIB_CHECK_VERSION(2, 58, 0)
        g_water_wayland_source_set_error_callback(ctx.esrc, G_SOURCE_FUNC(quit_signal), NULL, NULL);
#else
        g_water_wayland_source_set_error_callback(ctx.esrc, (GSourceFunc)quit_signal, NULL, NULL);
#endif

        if (ctx.display == NULL) {
                LOG_W("failed to create display");
                return false;
        }

        ctx.toplevel_manager_name = UINT32_MAX;

        ctx.registry = wl_display_get_registry(ctx.display);
        wl_registry_add_listener(ctx.registry, &registry_listener, NULL);
        wl_display_roundtrip(ctx.display);
        if (ctx.toplevel_manager_name != UINT32_MAX) {
                ctx.toplevel_manager = wl_registry_bind(ctx.registry, ctx.toplevel_manager_name,
                                &zwlr_foreign_toplevel_manager_v1_interface,
                                2);
                zwlr_foreign_toplevel_manager_v1_add_listener(ctx.toplevel_manager,
                                &toplevel_manager_impl, NULL);
        }
        wl_display_roundtrip(ctx.display); // load list of toplevels
        wl_display_roundtrip(ctx.display); // load toplevel details
        wl_display_flush(ctx.display);

        if (ctx.compositor == NULL) {
                LOG_W("compositor doesn't support wl_compositor");
                return false;
        }
        if (ctx.shm == NULL) {
                LOG_W("compositor doesn't support wl_shm");
                return false;
        }
        if (ctx.layer_shell == NULL) {
                LOG_W("compositor doesn't support zwlr_layer_shell_v1");
                return false;
        }
        if (wl_list_empty(&ctx.seats)) {
                LOG_W("no seat was found, so dunst cannot see input");
        } else if (ctx.idle_handler == NULL
#ifdef HAVE_WL_EXT_IDLE_NOTIFY
                && ctx.ext_idle_notifier == NULL
#endif
        ) {
                LOG_I("compositor doesn't support org_kde_kwin_idle or ext-idle-notify-v1");
        } else {
                // Set up idle monitoring for any seats that we received before the idle protocols
                struct dunst_seat *seat = NULL;
                wl_list_for_each(seat, &ctx.seats, link) {
                        add_seat_to_idle_handler(seat);
                }
        }

        if (ctx.toplevel_manager == NULL) {
                LOG_W("compositor doesn't support zwlr_foreign_toplevel_v1. Fullscreen detection won't work");
        }

        // Set up the cursor. It needs a wl_surface with the cursor loaded into it.
        // If one of these fail, dunst will work fine without the cursor being able to change.
        const char *cursor_size_env = getenv("XCURSOR_SIZE");
        int cursor_size = 24;
        if (cursor_size_env != NULL) {
                errno = 0;
                char *end;
                int temp_size = (int)strtol(cursor_size_env, &end, 10);
                if (errno == 0 && cursor_size_env[0] != 0 && end[0] == 0 && temp_size > 0) {
                        cursor_size = temp_size;
                } else {
                        LOG_W("Error: XCURSOR_SIZE is invalid");
                }
        }
        const char *xcursor_theme = getenv("XCURSOR_THEME");
        ctx.cursor_theme = wl_cursor_theme_load(xcursor_theme, cursor_size, ctx.shm);
        if (ctx.cursor_theme == NULL) {
                LOG_W("couldn't find a cursor theme");
                return true;
        }
        struct wl_cursor *cursor = wl_cursor_theme_get_cursor(ctx.cursor_theme, "left_ptr");
        if (cursor == NULL) {
                LOG_W("couldn't find cursor icon \"left_ptr\"");
                wl_cursor_theme_destroy(ctx.cursor_theme);
                // Set to NULL so it doesn't get free'd again
                ctx.cursor_theme = NULL;
                return true;
        }
        ctx.cursor_image = cursor->images[0];
        struct wl_buffer *cursor_buffer = wl_cursor_image_get_buffer(cursor->images[0]);
        ctx.cursor_surface = wl_compositor_create_surface(ctx.compositor);
        wl_surface_attach(ctx.cursor_surface, cursor_buffer, 0, 0);
        wl_surface_commit(ctx.cursor_surface);

        return true;
}

void wl_deinit(void) {
        // We need to check if any of these are NULL, since the initialization
        // could have been aborted half way through, or the compositor doesn't
        // support some of these features.
        if (ctx.layer_surface != NULL) {
                zwlr_layer_surface_v1_destroy(ctx.layer_surface);
        }
        if (ctx.surface != NULL) {
                wl_surface_destroy(ctx.surface);
        }
        finish_buffer(&ctx.buffers[0]);
        finish_buffer(&ctx.buffers[1]);

        // The output list is initialized at the start of init, so no need to
        // check for NULL
        struct dunst_output *output, *output_tmp;
        wl_list_for_each_safe(output, output_tmp, &ctx.outputs, link) {
                destroy_output(output);
        }

        struct dunst_seat *seat, *seat_tmp;
        wl_list_for_each_safe(seat, seat_tmp, &ctx.seats, link) {
                destroy_seat(seat);
        }

#ifdef HAVE_WL_CURSOR_SHAPE
        if (ctx.cursor_shape_manager)
                wp_cursor_shape_manager_v1_destroy(ctx.cursor_shape_manager);
#endif

#ifdef HAVE_WL_EXT_IDLE_NOTIFY
        if (ctx.ext_idle_notifier)
                ext_idle_notifier_v1_destroy(ctx.ext_idle_notifier);
#endif

        if (ctx.idle_handler)
                org_kde_kwin_idle_destroy(ctx.idle_handler);

        if (ctx.layer_shell)
                zwlr_layer_shell_v1_destroy(ctx.layer_shell);

        if (ctx.compositor)
                wl_compositor_destroy(ctx.compositor);

        if (ctx.shm)
                wl_shm_destroy(ctx.shm);

        if (ctx.registry)
                wl_registry_destroy(ctx.registry);

        if (ctx.cursor_theme != NULL) {
                wl_cursor_theme_destroy(ctx.cursor_theme);
                wl_surface_destroy(ctx.cursor_surface);
        }

        // this also disconnects the wl_display
        g_water_wayland_source_free(ctx.esrc);
}

static void schedule_frame_and_commit(void);

// Draw and commit a new frame.
static void send_frame(void) {
        int scale = wl_get_scale();

        if (wl_list_empty(&ctx.outputs)) {
                ctx.dirty = false;
                return;
        }

        struct dunst_output *output = get_configured_output();
        int height = ctx.cur_dim.h;
        int width = ctx.cur_dim.w;

        // There are two cases where we want to tear down the surface: zero
        // notifications (height = 0) or moving between outputs.
        if (height == 0 || ctx.layer_surface_output != output) {
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

        {
                struct dunst_output *o;
                int i = 0;
                wl_list_for_each(o, &ctx.outputs, link) {
                        i++;
                }
                if (i == 0) {
                        // There are no outputs, so don't create a surface
                        ctx.dirty = false;
                        return;
                }
        }

        // If there are no notifications, there's no point in recreating the
        // surface right now.
        if (height == 0) {
                ctx.dirty = false;
                return;
        }

        // If we've made it here, there is something to draw. If the surface
        // doesn't exist (this is the first notification, or we moved to a
        // different output), we need to create it.
        if (ctx.layer_surface == NULL) {
                struct wl_output *wl_output = NULL;
                if (output != NULL) {
                        wl_output = output->wl_output;
                }
                ctx.layer_surface_output = output;
                ctx.surface = wl_compositor_create_surface(ctx.compositor);
                wl_surface_add_listener(ctx.surface, &surface_listener, NULL);

                ctx.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
                        ctx.layer_shell, ctx.surface, wl_output,
                        settings.layer, "notifications");
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
        if (ctx.height != height || ctx.width != width) {
                struct dimensions dim = ctx.cur_dim;
                // Set window size
                zwlr_layer_surface_v1_set_size(ctx.layer_surface,
                                dim.w, dim.h);

                // Put the window at the right position
                zwlr_layer_surface_v1_set_anchor(ctx.layer_surface,
                        settings.origin);
                zwlr_layer_surface_v1_set_margin(ctx.layer_surface,
                                // Offsets where no anchors are specified are
                                // ignored. We can safely assume the offset is
                                // positive.
                                settings.offset.y, // top
                                settings.offset.x, // right
                                settings.offset.y, // bottom
                                settings.offset.x);// left

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

static void schedule_frame_and_commit(void) {
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

void set_dirty(void) {
        if (ctx.dirty) {
                return;
        }
        ctx.dirty = true;
        schedule_frame_and_commit();
}

window wl_win_create(void) {
        struct window_wl *win = g_malloc0(sizeof(struct window_wl));
        return win;
}

void wl_win_destroy(window winptr) {
        struct window_wl *win = (struct window_wl*)winptr;
        // FIXME: Dealloc everything
        g_free(win);
}

void wl_win_show(window win) {
        // This is here for compatibilty with the X11 output. The window is
        // already shown in wl_display_surface.
}

void wl_win_hide(window win) {
        LOG_I("Wayland: Hiding window");
        ctx.cur_dim.h = 0;
        set_dirty();
        wl_display_roundtrip(ctx.display);
}

void wl_display_surface(cairo_surface_t *srf, window winptr, const struct dimensions* dim) {
        /* struct window_wl *win = (struct window_wl*)winptr; */
        int scale = wl_get_scale();
        LOG_D("Buffer size (scaled) %ix%i", dim->w * scale, dim->h * scale);
        ctx.current_buffer = get_next_buffer(ctx.shm, ctx.buffers,
                        dim->w * scale, dim->h * scale);

        if(ctx.current_buffer == NULL) {
                return;
        }

        cairo_t *c = ctx.current_buffer->cairo;
        cairo_save(c);
        cairo_set_source_surface(c, srf, 0, 0);
        cairo_rectangle(c, 0, 0, dim->w * scale, dim->h * scale);
        cairo_fill(c);
        cairo_restore(c);

        ctx.cur_dim = *dim;

        set_dirty();
        wl_display_roundtrip(ctx.display);
}

cairo_t* wl_win_get_context(window winptr) {
        struct window_wl *win = (struct window_wl*)winptr;
        ctx.current_buffer = get_next_buffer(ctx.shm, ctx.buffers, 500, 500);

        if(ctx.current_buffer == NULL) {
                return NULL;
        }

        win->c_surface = ctx.current_buffer->surface;
        win->c_ctx = ctx.current_buffer->cairo;
        return win->c_ctx;
}

const struct screen_info* wl_get_active_screen(void) {
        static struct screen_info scr = {
                .w = 3840,
                .h = 2160,
                .x = 0,
                .y = 0,
                .id = 0,
                .mmh = 500
        };
        scr.dpi = wl_get_scale() * 96;

        struct dunst_output *current_output = get_configured_output();
        if (current_output != NULL) {
                scr.w = current_output->width;
                scr.h = current_output->height;
                return &scr;
        } else {
                // Using auto output. We don't know on which output we are
                // (although we might find it out by looking at the list of
                // toplevels).
                return &scr;
        }
}

bool wl_is_idle(void) {
        struct dunst_seat *seat = NULL;
        bool is_idle = true;
        // When the user doesn't have a seat, or their compositor doesn't support the idle
        // protocol, we'll assume that they are not idle.
        if (settings.idle_threshold == 0 || wl_list_empty(&ctx.seats)) {
                is_idle = false;
        } else {
                wl_list_for_each(seat, &ctx.seats, link) {
                        // seat.is_idle cannot be set without an idle protocol
                        is_idle &= seat->is_idle;
                }
        }
        LOG_I("Idle status queried: %i", is_idle);
        return is_idle;
}

bool wl_have_fullscreen_window(void) {
        struct dunst_output *current_output = get_configured_output();
        uint32_t output_name = UINT32_MAX;
        if (current_output)
                output_name = current_output->global_name;

        struct toplevel_v1 *toplevel;
	wl_list_for_each(toplevel, &toplevel_list, link) {
                if (!(toplevel->current.state & TOPLEVEL_STATE_FULLSCREEN &&
                                        toplevel->current.state &
                                        TOPLEVEL_STATE_ACTIVATED))
                        continue;

                struct toplevel_output *pos;
                wl_list_for_each(pos, &toplevel->output_list, link) {
                        if (output_name == UINT32_MAX ||
                                        pos->dunst_output->global_name ==
                                        output_name) {
                                return true;
                        }
                }
        }
        return false;
}

double wl_get_scale(void) {
        int scale = 0;
        struct dunst_output *output = get_configured_output();
        if (output) {
                scale = output->scale;
        } else {
                // return the largest scale
                struct dunst_output *output;
                wl_list_for_each(output, &ctx.outputs, link) {
                        scale = MAX(output->scale, scale);
                }
        }
        if (scale <= 0)
                scale = 1;
        return scale;
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

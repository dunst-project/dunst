#define _POSIX_C_SOURCE 200112L
#include "wl_seat.h"

#include <linux/input-event-codes.h>

#include "protocols/idle.h"

#ifdef HAVE_WL_CURSOR_SHAPE
#include "protocols/cursor-shape-v1.h"
#include "protocols/tablet-unstable-v2.h"
#endif

#ifdef HAVE_WL_EXT_IDLE_NOTIFY
#include "protocols/ext-idle-notify-v1.h"
#endif

#include "../dunst.h"
#include "../input.h"
#include "../log.h"
#include "../settings.h"
#include "wl_ctx.h"

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

static void pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
                uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
        if (settings.pause_on_mouse_over) {
                dunst_status(S_MOUSE_OVER, true);
                wake_up();
        }

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
        if (settings.pause_on_mouse_over) {
                dunst_status(S_MOUSE_OVER, false);
                wake_up();
        }
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

static void destroy_seat_pointer(struct dunst_seat *seat) {
        wl_pointer_release(seat->pointer.wl_pointer);
        seat->pointer.wl_pointer = NULL;
}

static void destroy_seat_touch(struct dunst_seat *seat) {
        wl_touch_release(seat->touch.wl_touch);
        seat->touch.wl_touch = NULL;
}

void destroy_seat(struct dunst_seat *seat) {
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
        {
                wl_seat_destroy(seat->wl_seat);
        }
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

void add_seat_to_idle_handler(struct dunst_seat *seat) {
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

void create_seat(struct wl_registry *registry, uint32_t global_name, uint32_t version) {
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

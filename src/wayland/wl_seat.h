#ifndef DUNST_WL_SEAT_H
#define DUNST_WL_SEAT_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

#include "protocols/idle-client-header.h"

#ifdef HAVE_WL_CURSOR_SHAPE
#include "protocols/cursor-shape-v1-client-header.h"
#endif

#ifdef HAVE_WL_EXT_IDLE_NOTIFY
#include "protocols/ext-idle-notify-v1-client-header.h"
#endif

#define MAX_TOUCHPOINTS 10

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

        struct {
                struct wl_keyboard *wl_keyboard;
        } keyboard;
};

void create_seat(struct wl_registry *registry, uint32_t global_name, uint32_t version);
void destroy_seat(struct dunst_seat *seat);
void add_seat_to_idle_handler(struct dunst_seat *seat);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_X_H
#define DUNST_X_H

#define XLIB_ILLEGAL_ACCESS

#include <cairo.h>
#include <glib.h>
#include <stdbool.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "screen.h"

struct keyboard_shortcut {
        const char *str;
        KeyCode code;
        KeySym sym;
        KeySym mask;
        bool is_valid;
};

// Cyclical dependency
#include "../settings.h"

struct window_x11;

struct dimensions {
        int x;
        int y;
        int w;
        int h;

        int corner_radius;
};

struct x_context {
        Display *dpy;
        XScreenSaverInfo *screensaver_info;
};

struct color {
        double r;
        double g;
        double b;
};

extern struct x_context xctx;

/* window */
struct window_x11 *x_win_create(void);
void x_win_destroy(struct window_x11 *win);

void x_win_show(struct window_x11 *win);
void x_win_hide(struct window_x11 *win);

void x_display_surface(cairo_surface_t *srf, struct window_x11 *win, const struct dimensions *dim);

bool x_win_visible(struct window_x11 *win);
cairo_t* x_win_get_context(struct window_x11 *win);

/* X misc */
bool x_is_idle(void);
void x_setup(void);
void x_free(void);

struct geometry x_parse_geometry(const char *geom_str);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

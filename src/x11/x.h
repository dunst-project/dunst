/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_X_H
#define DUNST_X_H

#define XLIB_ILLEGAL_ACCESS

#include <cairo.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>
#include <glib.h>
#include <stdbool.h>

#include "screen.h"

typedef struct _keyboard_shortcut {
        const char *str;
        KeyCode code;
        KeySym sym;
        KeySym mask;
        bool is_valid;
} keyboard_shortcut;

// Cyclical dependency
#include "src/settings.h"

typedef struct window_x11 window_x11;

struct dimensions {
        int x;
        int y;
        int w;
        int h;

        int corner_radius;
};

typedef struct _xctx {
        Display *dpy;
        const char *colors[3][3];
        XScreenSaverInfo *screensaver_info;
} xctx_t;

typedef struct _color_t {
        double r;
        double g;
        double b;
} color_t;

extern xctx_t xctx;

/* window */
window_x11 *x_win_create(void);
void x_win_destroy(window_x11 *win);

void x_win_show(window_x11 *win);
void x_win_hide(window_x11 *win);

void x_display_surface(cairo_surface_t *srf, window_x11 *win, const struct dimensions *dim);

bool x_win_visible(window_x11 *win);
cairo_t* x_win_get_context(window_x11 *win);

/* X misc */
bool x_is_idle(void);
void x_setup(void);
void x_free(void);

struct geometry x_parse_geometry(const char *geom_str);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

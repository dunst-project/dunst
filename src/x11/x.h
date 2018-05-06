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

#define BUTTONMASK (ButtonPressMask|ButtonReleaseMask)
#define FONT_HEIGHT_BORDER 2
#define DEFFONT "Monospace-11"

typedef struct _keyboard_shortcut {
        const char *str;
        KeyCode code;
        KeySym sym;
        KeySym mask;
        bool is_valid;
} keyboard_shortcut;

// Cyclical dependency
#include "src/settings.h"

struct dimensions {
        int x;
        int y;
        int w;
        int h;
};

typedef struct {
        Window xwin;
        cairo_surface_t *root_surface;
        cairo_t *c_ctx;
        GSource *esrc;
        int cur_screen;
        bool visible;
        struct dimensions dim;
} window_x11;

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
void x_win_hide(void);
void x_win_show(void);
void x_win_destroy(window_x11 *win);
void x_display_surface(cairo_surface_t *srf, window_x11 *win, const struct dimensions *dim);

/* X misc */
bool x_is_idle(void);
void x_setup(void);
void x_free(void);

struct geometry x_parse_geometry(const char *geom_str);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

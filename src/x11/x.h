/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_X_H
#define DUNST_X_H

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
void x_win_move(int x, int y, int width, int height);
void x_win_hide(void);
void x_win_show(void);
void x_win_destroy(window_x11 *win);

/* shortcut */
void x_shortcut_init(keyboard_shortcut *shortcut);
void x_shortcut_ungrab(keyboard_shortcut *ks);
int x_shortcut_grab(keyboard_shortcut *ks);
KeySym x_shortcut_string_to_mask(const char *str);

/* X misc */
bool x_is_idle(void);
void x_setup(void);
void x_free(void);

struct geometry x_parse_geometry(const char *geom_str);

gboolean x_mainloop_fd_dispatch(GSource *source, GSourceFunc callback,
                                gpointer user_data);
gboolean x_mainloop_fd_check(GSource *source);
gboolean x_mainloop_fd_prepare(GSource *source, gint *timeout);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

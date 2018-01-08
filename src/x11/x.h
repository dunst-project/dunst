/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_X_H
#define DUNST_X_H

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

typedef struct _xctx {
        Atom utf8;
        Display *dpy;
        int cur_screen;
        Window win;
        bool visible;
        dimension_t geometry;
        const char *colors[3][3];
        XScreenSaverInfo *screensaver_info;
        dimension_t window_dim;
        unsigned long sep_custom_col;
} xctx_t;

typedef struct _color_t {
        double r;
        double g;
        double b;
        double a;
} color_t;

extern xctx_t xctx;

/* window */
void x_win_draw(void);
void x_win_hide(void);
void x_win_show(void);

/* shortcut */
void x_shortcut_init(keyboard_shortcut *shortcut);
void x_shortcut_ungrab(keyboard_shortcut *ks);
int x_shortcut_grab(keyboard_shortcut *ks);
KeySym x_shortcut_string_to_mask(const char *str);

/* X misc */
bool x_is_idle(void);
void x_setup(void);
void x_free(void);

gboolean x_mainloop_fd_dispatch(GSource *source, GSourceFunc callback,
                                gpointer user_data);
gboolean x_mainloop_fd_check(GSource *source);
gboolean x_mainloop_fd_prepare(GSource *source, gint *timeout);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DRAW_H
#define DRAW_H

#include <stdbool.h>
#include "glib.h"
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/extensions/scrnsaver.h>

#include <X11/Xft/Xft.h>

#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define FONT_HEIGHT_BORDER 2
#define DEFFONT "Monospace-11"
#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))

typedef struct _dimension_t {
        int x;
        int y;
        unsigned int h;
        unsigned int w;
        int mask;
        int negative_width;
} dimension_t;

typedef struct _screen_info {
        int scr;
        dimension_t dim;
} screen_info;

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
        Window win;
        bool visible;
        dimension_t geometry;
        const char *color_strings[2][3];
        XScreenSaverInfo *screensaver_info;
        dimension_t window_dim;
        unsigned long framec;
        unsigned long sep_custom_col;
} xctx_t;

typedef struct _color_t {
        double r;
        double g;
        double b;
} color_t;

extern xctx_t xctx;

/* window */
void x_win_draw(void);
void x_win_hide(void);
void x_win_show(void);

/* shortcut */
void x_shortcut_init(keyboard_shortcut * shortcut);
void x_shortcut_ungrab(keyboard_shortcut * ks);
int x_shortcut_grab(keyboard_shortcut * ks);
KeySym x_shortcut_string_to_mask(const char *str);

/* X misc */
bool x_is_idle(void);
void x_setup(void);
void x_free(void);

gboolean x_mainloop_fd_dispatch(GSource * source, GSourceFunc callback,
                                gpointer user_data);
gboolean x_mainloop_fd_check(GSource * source);
gboolean x_mainloop_fd_prepare(GSource * source, gint * timeout);

#endif
/* vim: set ts=8 sw=8 tw=0: */

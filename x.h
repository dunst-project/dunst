/*

MIT/X Consortium License

© 2013 Sascha Kruse <knopwob@googlemail.com> and contributors
© 2010-2011 Connor Lane Smith <cls@lubutu.com>
© 2006-2011 Anselm R Garbe <anselm@garbe.us>
© 2009 Gottox <gottox@s01.de>
© 2009 Markus Schnalke <meillo@marmaro.de>
© 2009 Evan Gates <evan.gates@gmail.com>
© 2006-2008 Sander van Dijk <a dot h dot vandijk at gmail dot com>
© 2006-2007 Michał Janeczek <janeczek at gmail dot com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

 */
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

typedef struct {
        int x, y, w, h;
        bool invert;
        Display *dpy;
        GC gc;
        Pixmap canvas;
        XftDraw *xftdraw;
        struct {
                int ascent;
                int descent;
                int height;
                int width;
                XFontSet set;
                XFontStruct *xfont;
                XftFont *xft_font;
        } font;
} DC;                           /* draw context */

typedef struct {
        unsigned long FG;
        XftColor FG_xft;
        unsigned long BG;
} ColorSet;

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

typedef struct _render_text {
        ColorSet *colors;
        GSList *lines;
} render_text;

typedef struct _xctx {
        int height_limit;
        int font_h;
        Atom utf8;
        DC *dc;
        Window win;
        bool visible;
        dimension_t geometry;
        const char *color_strings[2][3];
        XScreenSaverInfo *screensaver_info;
        dimension_t window_dim;
        unsigned long framec;
        unsigned long sep_custom_col;
} xctx_t;

extern xctx_t xctx;

void drawrect(DC * dc, int x, int y, unsigned int w, unsigned int h, bool fill,
              unsigned long color);
void drawtext(DC * dc, const char *text, ColorSet * col);
void drawtextn(DC * dc, const char *text, size_t n, ColorSet * col);
void freecol(DC * dc, ColorSet * col);
void eprintf(const char *fmt, ...);
void freedc(DC * dc);
unsigned long getcolor(DC * dc, const char *colstr);
ColorSet *initcolor(DC * dc, const char *foreground, const char *background);
DC *initdc(void);
void initfont(DC * dc, const char *fontstr);
void setopacity(DC * dc, Window win, unsigned long opacity);
void mapdc(DC * dc, Window win, unsigned int w, unsigned int h);
void resizedc(DC * dc, unsigned int w, unsigned int h);
int textnw(DC * dc, const char *text, size_t len);
int textw(DC * dc, const char *text);

/* window */
void x_win_draw(void);
void x_win_hide(void);
void x_win_show(void);
void x_win_setup(void);

/* shortcut */
void x_shortcut_init(keyboard_shortcut * shortcut);
void x_shortcut_ungrab(keyboard_shortcut * ks);
int x_shortcut_grab(keyboard_shortcut * ks);
KeySym x_shortcut_string_to_mask(const char *str);

/* X misc */
void x_handle_click(XEvent ev);
void x_screen_info(screen_info * scr);
bool x_is_idle(void);
void x_setup(void);

gboolean x_mainloop_fd_dispatch(GSource * source, GSourceFunc callback,
                                gpointer user_data);
gboolean x_mainloop_fd_check(GSource * source);
gboolean x_mainloop_fd_prepare(GSource * source, gint * timeout);

#endif
/* vim: set ts=8 sw=8 tw=0: */

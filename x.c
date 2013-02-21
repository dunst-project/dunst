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
#include <math.h>
#include <sys/time.h>
#include <ctype.h>
#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xatom.h>

#include "x.h"
#include "utils.h"
#include "dunst.h"
#include "settings.h"
#include "notification.h"

xctx_t xctx;
bool dunst_grab_errored = false;

static void x_shortcut_setup_error_handler(void);
static int x_shortcut_tear_down_error_handler(void);

void
drawrect(DC * dc, int x, int y, unsigned int w, unsigned int h, bool fill,
         unsigned long color)
{
        XSetForeground(dc->dpy, dc->gc, color);
        if (fill)
                XFillRectangle(dc->dpy, dc->canvas, dc->gc, dc->x + x,
                               dc->y + y, w, h);
        else
                XDrawRectangle(dc->dpy, dc->canvas, dc->gc, dc->x + x,
                               dc->y + y, w - 1, h - 1);
}

void drawtext(DC * dc, const char *text, ColorSet * col)
{
        char buf[BUFSIZ];
        size_t mn, n = strlen(text);

        /* shorten text if necessary */
        for (mn = MIN(n, sizeof buf);
             textnw(dc, text, mn) + dc->font.height / 2 > dc->w; mn--)
                if (mn == 0)
                        return;
        memcpy(buf, text, mn);
        if (mn < n)
                for (n = MAX(mn - 3, 0); n < mn; buf[n++] = '.') ;

        drawrect(dc, 0, 0, dc->w, dc->h, true, col->BG);
        drawtextn(dc, buf, mn, col);
}

void drawtextn(DC * dc, const char *text, size_t n, ColorSet * col)
{
        int x = dc->x + dc->font.height / 2;
        int y = dc->y + dc->font.ascent + 1;

        XSetForeground(dc->dpy, dc->gc, col->FG);
        if (dc->font.xft_font) {
                if (!dc->xftdraw)
                        eprintf("error, xft drawable does not exist");
                XftDrawStringUtf8(dc->xftdraw, &col->FG_xft,
                                  dc->font.xft_font, x, y,
                                  (unsigned char *)text, n);
        } else if (dc->font.set) {
                printf("XmbDrawString\n");
                XmbDrawString(dc->dpy, dc->canvas, dc->font.set, dc->gc, x, y,
                              text, n);
        } else {
                XSetFont(dc->dpy, dc->gc, dc->font.xfont->fid);
                XDrawString(dc->dpy, dc->canvas, dc->gc, x, y, text, n);
        }
}

void eprintf(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);

        if (fmt[0] != '\0' && fmt[strlen(fmt) - 1] == ':') {
                fputc(' ', stderr);
                perror(NULL);
        }
        exit(EXIT_FAILURE);
}

void freecol(DC * dc, ColorSet * col)
{
        if (col) {
                if (&col->FG_xft)
                        XftColorFree(dc->dpy,
                                     DefaultVisual(dc->dpy,
                                                   DefaultScreen(dc->dpy)),
                                     DefaultColormap(dc->dpy,
                                                     DefaultScreen(dc->dpy)),
                                     &col->FG_xft);
                free(col);
        }
}

void freedc(DC * dc)
{
        if (dc->font.xft_font) {
                XftFontClose(dc->dpy, dc->font.xft_font);
                XftDrawDestroy(dc->xftdraw);
        }
        if (dc->font.set)
                XFreeFontSet(dc->dpy, dc->font.set);
        if (dc->font.xfont)
                XFreeFont(dc->dpy, dc->font.xfont);
        if (dc->canvas)
                XFreePixmap(dc->dpy, dc->canvas);
        if (dc->gc)
                XFreeGC(dc->dpy, dc->gc);
        if (dc->dpy)
                XCloseDisplay(dc->dpy);
        if (dc)
                free(dc);
}

unsigned long getcolor(DC * dc, const char *colstr)
{
        Colormap cmap = DefaultColormap(dc->dpy, DefaultScreen(dc->dpy));
        XColor color;

        if (!XAllocNamedColor(dc->dpy, cmap, colstr, &color, &color))
                eprintf("cannot allocate color '%s'\n", colstr);
        return color.pixel;
}

ColorSet *initcolor(DC * dc, const char *foreground, const char *background)
{
        ColorSet *col = (ColorSet *) malloc(sizeof(ColorSet));
        if (!col) {
                eprintf("error, cannot allocate memory for color set");
                exit(EXIT_FAILURE);
        }
        col->BG = getcolor(dc, background);
        col->FG = getcolor(dc, foreground);
        if (dc->font.xft_font)
                if (!XftColorAllocName
                    (dc->dpy, DefaultVisual(dc->dpy, DefaultScreen(dc->dpy)),
                     DefaultColormap(dc->dpy, DefaultScreen(dc->dpy)),
                     foreground, &col->FG_xft))
                        eprintf("error, cannot allocate xft font color '%s'\n",
                                foreground);
        return col;
}

DC *initdc(void)
{
        DC *dc;

        if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
                fputs("no locale support\n", stderr);
        if (!(dc = calloc(1, sizeof *dc))) {
                eprintf("cannot malloc %u bytes:", sizeof *dc);
                exit(EXIT_FAILURE);
        }

        if (!(dc->dpy = XOpenDisplay(NULL))) {
                eprintf("cannot open display\n");
                exit(EXIT_FAILURE);
        }

        dc->gc = XCreateGC(dc->dpy, DefaultRootWindow(dc->dpy), 0, NULL);
        XSetLineAttributes(dc->dpy, dc->gc, 1, LineSolid, CapButt, JoinMiter);
        return dc;
}

void initfont(DC * dc, const char *fontstr)
{
        char *def, **missing, **names;
        int i, n;
        XFontStruct **xfonts;

        missing = NULL;
        if ((dc->font.xfont = XLoadQueryFont(dc->dpy, fontstr))) {
                dc->font.ascent = dc->font.xfont->ascent;
                dc->font.descent = dc->font.xfont->descent;
                dc->font.width = dc->font.xfont->max_bounds.width;
        } else
            if ((dc->font.set =
                 XCreateFontSet(dc->dpy, fontstr, &missing, &n, &def))) {
                n = XFontsOfFontSet(dc->font.set, &xfonts, &names);
                for (i = 0; i < n; i++) {
                        dc->font.ascent =
                            MAX(dc->font.ascent, xfonts[i]->ascent);
                        dc->font.descent =
                            MAX(dc->font.descent, xfonts[i]->descent);
                        dc->font.width =
                            MAX(dc->font.width, xfonts[i]->max_bounds.width);
                }
        } else
            if ((dc->font.xft_font =
                 XftFontOpenName(dc->dpy, DefaultScreen(dc->dpy), fontstr))) {
                dc->font.ascent = dc->font.xft_font->ascent;
                dc->font.descent = dc->font.xft_font->descent;
                dc->font.width = dc->font.xft_font->max_advance_width;
        } else {
                eprintf("cannot load font '%s'\n", fontstr);
        }
        if (missing)
                XFreeStringList(missing);
        dc->font.height = dc->font.ascent + dc->font.descent;
        return;
}

void setopacity(DC * dc, Window win, unsigned long opacity)
{
        Atom _NET_WM_WINDOW_OPACITY =
            XInternAtom(dc->dpy, "_NET_WM_WINDOW_OPACITY", false);
        XChangeProperty(dc->dpy, win, _NET_WM_WINDOW_OPACITY, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)&opacity, 1L);
}

void mapdc(DC * dc, Window win, unsigned int w, unsigned int h)
{
        XCopyArea(dc->dpy, dc->canvas, win, dc->gc, 0, 0, w, h, 0, 0);
}

void resizedc(DC * dc, unsigned int w, unsigned int h)
{
        int screen = DefaultScreen(dc->dpy);
        if (dc->canvas)
                XFreePixmap(dc->dpy, dc->canvas);

        dc->w = w;
        dc->h = h;
        dc->canvas = XCreatePixmap(dc->dpy, DefaultRootWindow(dc->dpy), w, h,
                                   DefaultDepth(dc->dpy, screen));
        if (dc->xftdraw) {
                XftDrawDestroy(dc->xftdraw);
        }
        if (dc->font.xft_font) {
                dc->xftdraw =
                    XftDrawCreate(dc->dpy, dc->canvas,
                                  DefaultVisual(dc->dpy, screen),
                                  DefaultColormap(dc->dpy, screen));
                if (!(dc->xftdraw))
                        eprintf("error, cannot create xft drawable\n");
        }
}

int textnw(DC * dc, const char *text, size_t len)
{
        if (dc->font.xft_font) {
                XGlyphInfo gi;
                XftTextExtentsUtf8(dc->dpy, dc->font.xft_font,
                                   (const FcChar8 *)text, len, &gi);
                return gi.width;
        } else if (dc->font.set) {
                XRectangle r;
                XmbTextExtents(dc->font.set, text, len, NULL, &r);
                return r.width;
        }
        return XTextWidth(dc->font.xfont, text, len);
}

int textw(DC * dc, const char *text)
{
        return textnw(dc, text, strlen(text)) + dc->font.height;
}


        /*
         * Helper function to use glib's mainloop mechanic
         * with Xlib
         */
gboolean x_mainloop_fd_prepare(GSource *source, gint *timeout)
{
        *timeout = -1;
        return false;
}


        /*
         * Helper function to use glib's mainloop mechanic
         * with Xlib
         */
gboolean x_mainloop_fd_check(GSource *source)
{
        return XPending(xctx.dc->dpy) > 0;
}


        /*
         * Main Dispatcher for XEvents
         */
gboolean x_mainloop_fd_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
        XEvent ev;
        while (XPending(xctx.dc->dpy) > 0) {
                XNextEvent(xctx.dc->dpy, &ev);
                switch (ev.type) {
                case Expose:
                        if (ev.xexpose.count == 0 && xctx.visible) {
                        }
                        break;
                case SelectionNotify:
                        if (ev.xselection.property == xctx.utf8)
                                break;
                case VisibilityNotify:
                        if (ev.xvisibility.state != VisibilityUnobscured)
                                XRaiseWindow(xctx.dc->dpy, xctx.win);
                        break;
                case ButtonPress:
                        if (ev.xbutton.window == xctx.win) {
                                x_handle_click(ev);
                        }
                        break;
                case KeyPress:
                        if (settings.close_ks.str
                            && XLookupKeysym(&ev.xkey, 0) == settings.close_ks.sym
                            && settings.close_ks.mask == ev.xkey.state) {
                                if (displayed) {
                                        notification_close(g_queue_peek_head_link(displayed)->data, 2);
                                }
                        }
                        if (settings.history_ks.str
                            && XLookupKeysym(&ev.xkey, 0) == settings.history_ks.sym
                            && settings.history_ks.mask == ev.xkey.state) {
                                history_pop();
                        }
                        if (settings.close_all_ks.str
                            && XLookupKeysym(&ev.xkey, 0) == settings.close_all_ks.sym
                            && settings.close_all_ks.mask == ev.xkey.state) {
                                move_all_to_history();
                        }
                        if (settings.context_ks.str
                            && XLookupKeysym(&ev.xkey, 0) == settings.context_ks.sym
                            && settings.context_ks.mask == ev.xkey.state) {
                                context_menu();
                        }
                        break;
                }
        }
        return true;
}



        /*
         * Check whether the user is currently idle.
         */
bool x_is_idle(void)
{
        XScreenSaverQueryInfo(xctx.dc->dpy, DefaultRootWindow(xctx.dc->dpy),
                              xctx.screensaver_info);
        if (settings.idle_threshold == 0) {
                return false;
        }
        return xctx.screensaver_info->idle / 1000 > settings.idle_threshold;
}

/* TODO move to x_mainloop_* */
        /*
         * Handle incoming mouse click events
         */
void x_handle_click(XEvent ev)
{
        if (ev.xbutton.button == Button3) {
                move_all_to_history();

                return;
        }

        if (ev.xbutton.button == Button1) {
                int y = settings.separator_height;
                notification *n = NULL;
                for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                        n = iter->data;
                        int text_h = MAX(xctx.font_h, settings.line_height) * n->line_count;
                        int padding = 2 * settings.h_padding;

                        int height = text_h + padding;

                        if (ev.xbutton.y > y && ev.xbutton.y < y + height)
                                break;
                        else
                                y += height + settings.separator_height;
                }
                if (n)
                        notification_close(n, 2);
        }
}



        /*
         * Return the window that currently has
         * the keyboard focus.
         */
Window get_focused_window(void)
{
        Window focused = 0;
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *prop_return = NULL;
        Window root = RootWindow(xctx.dc->dpy, DefaultScreen(xctx.dc->dpy));
        Atom netactivewindow =
            XInternAtom(xctx.dc->dpy, "_NET_ACTIVE_WINDOW", false);

        XGetWindowProperty(xctx.dc->dpy, root, netactivewindow, 0L,
                           sizeof(Window), false, XA_WINDOW,
                           &type, &format, &nitems, &bytes_after, &prop_return);
        if (prop_return) {
                focused = *(Window *) prop_return;
                XFree(prop_return);
        }

        return focused;
}

#ifdef XINERAMA
        /*
         * Select the screen on which the Window
         * should be displayed.
         */
int select_screen(XineramaScreenInfo * info, int info_len)
{
        if (settings.f_mode == FOLLOW_NONE) {
                return settings.monitor >= 0 ? settings.monitor : XDefaultScreen(xctx.dc->dpy);

        } else {
                int x, y;
                assert(settings.f_mode == FOLLOW_MOUSE || settings.f_mode == FOLLOW_KEYBOARD);
                Window root = RootWindow(xctx.dc->dpy, DefaultScreen(xctx.dc->dpy));

                if (settings.f_mode == FOLLOW_MOUSE) {
                        int dummy;
                        unsigned int dummy_ui;
                        Window dummy_win;

                        XQueryPointer(xctx.dc->dpy, root, &dummy_win,
                                      &dummy_win, &x, &y, &dummy,
                                      &dummy, &dummy_ui);
                }

                if (settings.f_mode == FOLLOW_KEYBOARD) {

                        Window focused = get_focused_window();

                        if (focused == 0) {
                                /* something went wrong. Fallback to default */
                                return settings.monitor >= 0 ? settings.monitor : XDefaultScreen(xctx.dc->dpy);
                        }

                        Window child_return;
                        XTranslateCoordinates(xctx.dc->dpy, focused, root,
                                              0, 0, &x, &y, &child_return);
                }

                for (int i = 0; i < info_len; i++) {
                        if (INRECT(x, y, info[i].x_org,
                                   info[i].y_org,
                                   info[i].width, info[i].height)) {
                                return i;
                        }
                }

                /* something seems to be wrong. Fallback to default */
                return settings.monitor >= 0 ? settings.monitor : XDefaultScreen(xctx.dc->dpy);
        }
}
#endif

        /*
         * Update the information about the monitor
         * geometry.
         */
void x_screen_info(screen_info *scr)
{
#ifdef XINERAMA
        int n;
        XineramaScreenInfo *info;
        if ((info = XineramaQueryScreens(xctx.dc->dpy, &n))) {
                int screen = select_screen(info, n);
                if (screen >= n) {
                        /* invalid monitor, fallback to default */
                        screen = 0;
                }
                scr->dim.x = info[screen].x_org;
                scr->dim.y = info[screen].y_org;
                scr->dim.h = info[screen].height;
                scr->dim.w = info[screen].width;
                XFree(info);
        } else
#endif
        {
                scr->dim.x = 0;
                scr->dim.y = 0;

                int screen;
                if (settings.monitor >= 0)
                        screen = settings.monitor;
                else
                        screen = DefaultScreen(xctx.dc->dpy);

                scr->dim.w = DisplayWidth(xctx.dc->dpy, screen);
                scr->dim.h = DisplayHeight(xctx.dc->dpy, screen);
        }
}


        /*
         * Setup X11 stuff
         */
void x_setup(void)
{

        /* initialize xctx.dc, font, keyboard, colors */
        xctx.dc = initdc();

        initfont(xctx.dc, settings.font);

        x_shortcut_init(&settings.close_ks);
        x_shortcut_init(&settings.close_all_ks);
        x_shortcut_init(&settings.history_ks);
        x_shortcut_init(&settings.context_ks);

        x_shortcut_grab(&settings.close_ks);
        x_shortcut_ungrab(&settings.close_ks);
        x_shortcut_grab(&settings.close_all_ks);
        x_shortcut_ungrab(&settings.close_all_ks);
        x_shortcut_grab(&settings.history_ks);
        x_shortcut_ungrab(&settings.history_ks);
        x_shortcut_grab(&settings.context_ks);
        x_shortcut_ungrab(&settings.context_ks);

        xctx.color_strings[ColFG][LOW] = settings.lowfgcolor;
        xctx.color_strings[ColFG][NORM] = settings.normfgcolor;
        xctx.color_strings[ColFG][CRIT] = settings.critfgcolor;

        xctx.color_strings[ColBG][LOW] = settings.lowbgcolor;
        xctx.color_strings[ColBG][NORM] = settings.normbgcolor;
        xctx.color_strings[ColBG][CRIT] = settings.critbgcolor;

        xctx.framec = getcolor(xctx.dc, settings.frame_color);

        if (settings.sep_color == CUSTOM) {
                xctx.sep_custom_col = getcolor(xctx.dc, settings.sep_custom_color_str);
        } else {
                xctx.sep_custom_col = 0;
        }

        /* parse and set xctx.geometry and monitor position */
        if (settings.geom[0] == '-') {
                xctx.geometry.negative_width = true;
                settings.geom++;
        } else {
                xctx.geometry.negative_width = false;
        }

        xctx.geometry.mask = XParseGeometry(settings.geom,
                                       &xctx.geometry.x, &xctx.geometry.y,
                                       &xctx.geometry.w, &xctx.geometry.h);


        xctx.screensaver_info = XScreenSaverAllocInfo();


        x_win_setup();
        x_shortcut_grab(&settings.history_ks);
}



/* TODO comments and naming */

GSList *do_word_wrap(char *text, int max_width)
{

        GSList *result = NULL;
        g_strstrip(text);

        if (!text || strlen(text) == 0)
                return 0;

        char *begin = text;
        char *end = text;

        while (true) {
                if (*end == '\0') {
                        result = g_slist_append(result, g_strdup(begin));
                        break;
                }
                if (*end == '\n') {
                        *end = ' ';
                        result = g_slist_append(result, g_strndup(begin, end - begin));
                        begin = ++end;
                }

                if (settings.word_wrap && max_width > 0 && textnw(xctx.dc, begin, (end - begin) + 1) > max_width) {
                        /* find previous space */
                        char *space = end;
                        while (space > begin && !isspace(*space))
                                space--;

                        if (space > begin) {
                                end = space;
                        }
                        result = g_slist_append(result, g_strndup(begin, end - begin));
                        begin = ++end;
                }
                end++;
        }

        return result;
}


char *generate_final_text(notification *n)
{
        char *msg = g_strstrip(n->msg);
        char *buf;

        /* print dup_count and msg*/
        if (n->dup_count > 0 && (n->actions || n->urls)) {
                buf = g_strdup_printf("(%d%s%s) %s",
                                n->dup_count,
                                n->actions ? "A" : "",
                                n->urls ? "U" : "",
                                msg);
        } else if (n->actions || n->urls) {
                buf = g_strdup_printf("(%s%s) %s",
                                n->actions ? "A" : "",
                                n->urls ? "U" : "",
                                msg);
        } else {
                buf = g_strdup(msg);
        }

        /* print age */
        int hours, minutes, seconds;
        time_t t_delta = time(NULL) - n->timestamp;

        if (settings.show_age_threshold >= 0 && t_delta >= settings.show_age_threshold) {
                hours = t_delta / 3600;
                minutes = t_delta / 60 % 60;
                seconds = t_delta % 60;

                char *new_buf;
                if (hours > 0) {
                        new_buf = g_strdup_printf("%s (%dh %dm %ds old)", buf, hours,
                                 minutes, seconds);
                } else if (minutes > 0) {
                        new_buf = g_strdup_printf("%s (%dm %ds old)", buf, minutes,
                                 seconds);
                } else {
                        new_buf = g_strdup_printf("%s (%ds old)", buf, seconds);
                }

                free(buf);
                buf = new_buf;
        }

        return buf;
}

int calculate_x_offset(int line_width, int text_width)
{
        int leftover = line_width - text_width;
        struct timeval t;
        float pos;
        /* If the text is wider than the frame, bouncing is enabled and word_wrap disabled */
        if (line_width < text_width && settings.bounce_freq > 0.0001 && !settings.word_wrap) {
                gettimeofday(&t, NULL);
                pos =
                    ((t.tv_sec % 100) * 1e6 + t.tv_usec) / (1e6 / settings.bounce_freq);
                return (1 + sinf(2 * 3.14159 * pos)) * leftover / 2;
        }
        switch (settings.align) {
        case left:
                return settings.frame_width + settings.h_padding;
        case center:
                return settings.frame_width + settings.h_padding + (leftover / 2);
        case right:
                return settings.frame_width + settings.h_padding + leftover;
        default:
                /* this can't happen */
                return 0;
        }
}

unsigned long calculate_foreground_color(unsigned long source_color)
{
        Colormap cmap = DefaultColormap(xctx.dc->dpy, DefaultScreen(xctx.dc->dpy));
        XColor color;

        color.pixel = source_color;
        XQueryColor(xctx.dc->dpy, cmap, &color);

        int c_delta = 10000;

        /* do we need to darken or brighten the colors? */
        int darken = (color.red + color.green + color.blue) / 3 > 65535 / 2;

        if (darken) {
                if (color.red - c_delta < 0)
                        color.red = 0;
                else
                        color.red -= c_delta;
                if (color.green - c_delta < 0)
                        color.green = 0;
                else
                        color.green -= c_delta;
                if (color.blue - c_delta < 0)
                        color.blue = 0;
                else
                        color.blue -= c_delta;
        } else {
                if (color.red + c_delta > 65535)
                        color.red = 65535;
                else
                        color.red += c_delta;
                if (color.green + c_delta > 65535)
                        color.green = 65535;
                else
                        color.green += c_delta;
                if (color.blue + c_delta > 65535)
                        color.green = 65535;
                else
                        color.green += c_delta;
        }

        color.pixel = 0;
        XAllocColor(xctx.dc->dpy, cmap, &color);
        return color.pixel;
}

int calculate_width(void)
{
        screen_info scr;
        x_screen_info(&scr);
        if (xctx.geometry.mask & WidthValue && xctx.geometry.w == 0) {
                /* dynamic width */
                return 0;
        } else if (xctx.geometry.mask & WidthValue) {
                /* fixed width */
                if (xctx.geometry.negative_width) {
                        return scr.dim.w - xctx.geometry.w;
                } else {
                        return xctx.geometry.w;
                }
        } else {
                /* across the screen */
                return scr.dim.w;
        }
}

void move_and_map(int width, int height)
{

        int x,y;
        screen_info scr;
        x_screen_info(&scr);
        /* calculate window position */
        if (xctx.geometry.mask & XNegative) {
                x = (scr.dim.x + (scr.dim.w - width)) + xctx.geometry.x;
        } else {
                x = scr.dim.x + xctx.geometry.x;
        }

        if (xctx.geometry.mask & YNegative) {
                y = scr.dim.y + (scr.dim.h + xctx.geometry.y) - height;
        } else {
                y = scr.dim.y + xctx.geometry.y;
        }

        /* move and map window */
        if (x != xctx.window_dim.x || y != xctx.window_dim.y
            || width != xctx.window_dim.w || height != xctx.window_dim.h) {

                XResizeWindow(xctx.dc->dpy, xctx.win, width, height);
                XMoveWindow(xctx.dc->dpy, xctx.win, x, y);

                xctx.window_dim.x = x;
                xctx.window_dim.y = y;
                xctx.window_dim.h = height;
                xctx.window_dim.w = width;
        }

        mapdc(xctx.dc, xctx.win, width, height);

}

GSList *generate_render_texts(int width)
{
        GSList *render_texts = NULL;

        for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                render_text *rt = g_malloc(sizeof(render_text));

                rt->colors = ((notification*)iter->data)->colors;
                char *text = generate_final_text(iter->data);
                rt->lines = do_word_wrap(text, width);
                free(text);
                render_texts = g_slist_append(render_texts, rt);
        }

        /* add (x more) */
        if (settings.indicate_hidden && queue->length > 0) {
                if (xctx.geometry.h != 1) {
                        render_text *rt = g_malloc(sizeof(render_text));
                        rt->colors = ((render_text *) g_slist_last(render_texts)->data)->colors;
                        rt->lines = g_slist_append(NULL, g_strdup_printf("%d more)", queue->length));
                        render_texts = g_slist_append(render_texts, rt);
                } else {
                        GSList *last_lines = ((render_text *) g_slist_last(render_texts)->data)->lines;
                        GSList *last_line = g_slist_last(last_lines);
                        char *old = last_line->data;
                        char *new = g_strdup_printf("%s (%d more)", old, queue->length);
                        free(old);
                        last_line->data = new;
                }
        }

        return render_texts;
}

void free_render_text(void *data) {
        g_slist_free_full(((render_text *) data)->lines, g_free);
}

void free_render_texts(GSList *texts) {
        g_slist_free_full(texts, free_render_text);
}





void x_win_draw(void)
{

        int outer_width = calculate_width();
        screen_info scr;
        x_screen_info(&scr);


        settings.line_height = MAX(settings.line_height, xctx.font_h);

        int width;
        if (outer_width == 0)
                width = 0;
        else
                width = outer_width - (2 * settings.frame_width) - (2 * settings.h_padding);


        GSList *texts = generate_render_texts(width);
        int line_count = 0;
        for (GSList *iter = texts; iter; iter = iter->next) {
                render_text *tmp = iter->data;
                line_count += g_slist_length(tmp->lines);
        }

        /* if we have a dynamic width, calculate the actual width */
        if (width == 0) {
                for (GSList *iter = texts; iter; iter = iter->next) {
                        GSList *lines = ((render_text *) iter->data)->lines;
                        for (GSList *iiter = lines; iiter; iiter = iiter->next)
                                width = MAX(width, textw(xctx.dc, iiter->data));
                }
                outer_width = width + (2 * settings.frame_width) + (2 * settings.h_padding);
        }

        /* resize xctx.dc to correct width */

        int height = (line_count * settings.line_height)
                   + displayed->length * 2 * settings.padding
                   + ((settings.indicate_hidden && queue->length > 0 && xctx.geometry.h != 1) ? 2 * settings.padding : 0)
                   + (settings.separator_height * (displayed->length - 1))
                   + (2 * settings.frame_width);

        resizedc(xctx.dc, outer_width, height);

        /* draw frame
         * this draws a big box in the frame color which get filled with
         * smaller boxes of the notification colors
         */
        xctx.dc->y = 0;
        xctx.dc->x = 0;
        if (settings.frame_width > 0) {
                drawrect(xctx.dc, 0, 0, outer_width, height, true, xctx.framec);
        }

        xctx.dc->y = settings.frame_width;
        xctx.dc->x = settings.frame_width;

        for (GSList *iter = texts; iter; iter = iter->next) {

                render_text *cur = iter->data;
                ColorSet *colors = cur->colors;


                int line_count = 0;
                bool first_line = true;
                for (GSList *iiter = cur->lines; iiter; iiter = iiter->next) {
                        char *line = iiter->data;
                        line_count++;

                        int pad = 0;
                        bool last_line = iiter->next == NULL;

                        if (first_line && last_line)
                                pad = 2*settings.padding;
                        else if (first_line || last_line)
                                pad = settings.padding;

                        xctx.dc->x = settings.frame_width;

                        /* draw background */
                        drawrect(xctx.dc, 0, 0, width + (2*settings.h_padding), pad +  settings.line_height, true, colors->BG);

                        /* draw text */
                        xctx.dc->x = calculate_x_offset(width, textw(xctx.dc, line));

                        xctx.dc->y += ((settings.line_height - xctx.font_h) / 2);
                        xctx.dc->y += first_line ? settings.padding : 0;

                        drawtextn(xctx.dc, line, strlen(line), colors);

                        xctx.dc->y += settings.line_height - ((settings.line_height - xctx.font_h) / 2);
                        xctx.dc->y += last_line ? settings.padding : 0;

                        first_line = false;
                }

                /* draw separator */
                if (settings.separator_height > 0 && iter->next) {
                        xctx.dc->x = settings.frame_width;
                        double color;
                        if (settings.sep_color == AUTO)
                                color = calculate_foreground_color(colors->BG);
                        else if (settings.sep_color == FOREGROUND)
                                color = colors->FG;
                        else if (settings.sep_color == FRAME)
                                color = xctx.framec;
                        else {
                                /* CUSTOM */
                                color = xctx.sep_custom_col;
                        }
                        drawrect(xctx.dc, 0, 0, width + (2*settings.h_padding), settings.separator_height, true, color);
                        xctx.dc->y += settings.separator_height;
                }
        }

        move_and_map(outer_width, height);

        free_render_texts(texts);
}

        /*
         * Setup the window
         */
void x_win_setup(void)
{

        Window root;
        XSetWindowAttributes wa;

        xctx.window_dim.x = 0;
        xctx.window_dim.y = 0;
        xctx.window_dim.w = 0;
        xctx.window_dim.h = 0;

        root = RootWindow(xctx.dc->dpy, DefaultScreen(xctx.dc->dpy));
        xctx.utf8 = XInternAtom(xctx.dc->dpy, "UTF8_STRING", false);
        xctx.font_h = xctx.dc->font.height + FONT_HEIGHT_BORDER;

        wa.override_redirect = true;
        wa.background_pixmap = ParentRelative;
        wa.event_mask =
            ExposureMask | KeyPressMask | VisibilityChangeMask |
            ButtonPressMask;

        screen_info scr;
        x_screen_info(&scr);
        xctx.win =
            XCreateWindow(xctx.dc->dpy, root, scr.dim.x, scr.dim.y, scr.dim.w,
                          xctx.font_h, 0, DefaultDepth(xctx.dc->dpy,
                                                  DefaultScreen(xctx.dc->dpy)),
                          CopyFromParent, DefaultVisual(xctx.dc->dpy,
                                                        DefaultScreen(xctx.dc->dpy)),
                          CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        settings.transparency = settings.transparency > 100 ? 100 : settings.transparency;
        setopacity(xctx.dc, xctx.win,
                   (unsigned long)((100 - settings.transparency) * (0xffffffff / 100)));
}

        /*
         * Show the window and grab shortcuts.
         */
void x_win_show(void)
{
        /* window is already mapped or there's nothing to show */
        if (xctx.visible || g_queue_is_empty(displayed)) {
                return;
        }

        x_shortcut_grab(&settings.close_ks);
        x_shortcut_grab(&settings.close_all_ks);
        x_shortcut_grab(&settings.context_ks);

        x_shortcut_setup_error_handler();
        XGrabButton(xctx.dc->dpy, AnyButton, AnyModifier, xctx.win, false,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        if (x_shortcut_tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab mouse button(s)\n");
        }

        XMapRaised(xctx.dc->dpy, xctx.win);
        xctx.visible = true;
}

        /*
         * Hide the window and ungrab unused keyboard_shortcuts
         */
void x_win_hide()
{
        x_shortcut_ungrab(&settings.close_ks);
        x_shortcut_ungrab(&settings.close_all_ks);
        x_shortcut_ungrab(&settings.context_ks);

        XUngrabButton(xctx.dc->dpy, AnyButton, AnyModifier, xctx.win);
        XUnmapWindow(xctx.dc->dpy, xctx.win);
        XFlush(xctx.dc->dpy);
        xctx.visible = false;
}




        /*
         * Parse a string into a modifier mask.
         */
KeySym x_shortcut_string_to_mask(const char *str)
{
        if (!strcmp(str, "ctrl")) {
                return ControlMask;
        } else if (!strcmp(str, "mod4")) {
                return Mod4Mask;
        } else if (!strcmp(str, "mod3")) {
                return Mod3Mask;
        } else if (!strcmp(str, "mod2")) {
                return Mod2Mask;
        } else if (!strcmp(str, "mod1")) {
                return Mod1Mask;
        } else if (!strcmp(str, "shift")) {
                return ShiftMask;
        } else {
                fprintf(stderr, "Warning: Unknown Modifier: %s\n", str);
                return 0;
        }

}

        /*
         * Error handler for grabbing mouse and keyboard errors.
         */
static int GrabXErrorHandler(Display * display, XErrorEvent * e)
{
        dunst_grab_errored = true;
        char err_buf[BUFSIZ];
        XGetErrorText(display, e->error_code, err_buf, BUFSIZ);
        fputs(err_buf, stderr);
        fputs("\n", stderr);

        if (e->error_code != BadAccess) {
                exit(EXIT_FAILURE);
        }

        return 0;
}

        /*
         * Setup the Error handler.
         */
static void x_shortcut_setup_error_handler(void)
{
        dunst_grab_errored = false;

        XFlush(xctx.dc->dpy);
        XSetErrorHandler(GrabXErrorHandler);
}

        /*
         * Tear down the Error handler.
         */
static int x_shortcut_tear_down_error_handler(void)
{
        XFlush(xctx.dc->dpy);
        XSync(xctx.dc->dpy, false);
        XSetErrorHandler(NULL);
        return dunst_grab_errored;
}

        /*
         * Grab the given keyboard shortcut.
         */
int x_shortcut_grab(keyboard_shortcut *ks)
{
        if (!ks->is_valid)
                return 1;
        Window root;
        root = RootWindow(xctx.dc->dpy, DefaultScreen(xctx.dc->dpy));

        x_shortcut_setup_error_handler();

        if (ks->is_valid)
                XGrabKey(xctx.dc->dpy, ks->code, ks->mask, root,
                         true, GrabModeAsync, GrabModeAsync);

        if (x_shortcut_tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab key \"%s\"\n", ks->str);
                ks->is_valid = false;
                return 1;
        }
        return 0;
}

        /*
         * Ungrab the given keyboard shortcut.
         */
void x_shortcut_ungrab(keyboard_shortcut *ks)
{
        Window root;
        root = RootWindow(xctx.dc->dpy, DefaultScreen(xctx.dc->dpy));
        if (ks->is_valid)
                XUngrabKey(xctx.dc->dpy, ks->code, ks->mask, root);
}

        /*
         * Initialize the keyboard shortcut.
         */
void x_shortcut_init(keyboard_shortcut *ks)
{
        if (ks == NULL || ks->str == NULL)
                return;

        if (!strcmp(ks->str, "none") || (!strcmp(ks->str, ""))) {
                ks->is_valid = false;
                return;
        }

        char *str = g_strdup(ks->str);
        char *str_begin = str;

        if (str == NULL)
                die("Unable to allocate memory", EXIT_FAILURE);

        while (strstr(str, "+")) {
                char *mod = str;
                while (*str != '+')
                        str++;
                *str = '\0';
                str++;
                g_strchomp(mod);
                ks->mask = ks->mask | x_shortcut_string_to_mask(mod);
        }
        g_strstrip(str);

        ks->sym = XStringToKeysym(str);
        /* find matching keycode for ks->sym */
        int min_keysym, max_keysym;
        XDisplayKeycodes(xctx.dc->dpy, &min_keysym, &max_keysym);

        ks->code = NoSymbol;

        for (int i = min_keysym; i <= max_keysym; i++) {
                if (XkbKeycodeToKeysym(xctx.dc->dpy, i, 0, 0) == ks->sym
                    || XkbKeycodeToKeysym(xctx.dc->dpy, i, 0, 1) == ks->sym) {
                        ks->code = i;
                        break;
                }
        }

        if (ks->sym == NoSymbol || ks->code == NoSymbol) {
                fprintf(stderr, "Warning: Unknown keyboard shortcut: %s\n",
                        ks->str);
                ks->is_valid = false;
        } else {
                ks->is_valid = true;
        }

        free(str_begin);
}

/* vim: set ts=8 sw=8 tw=0: */

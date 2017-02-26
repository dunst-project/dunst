#include "screen.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#ifdef XRANDR
#include <X11/extensions/Xrandr.h>
#include <assert.h>
#elif XINERAMA
#include <X11/extensions/Xinerama.h>
#include <assert.h>
#endif
#include <locale.h>
#include <pango/pangocairo.h>
#include <pango/pango-types.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/settings.h"
#include "x.h"

bool dunst_follow_errored = false;

static void x_follow_setup_error_handler(void);
static int x_follow_tear_down_error_handler(void);
static int FollowXErrorHandler(Display *display, XErrorEvent *e);
static Window get_focused_window(void);

static double get_xft_dpi_value()
{
        double dpi = 0.0;
        XrmInitialize();
        char * xRMS = XResourceManagerString(xctx.dpy);

        if ( xRMS != NULL ) {
                XrmDatabase xDB = XrmGetStringDatabase(xRMS);
                char * xrmType;
                XrmValue xrmValue;

                if ( XrmGetResource(xDB, "Xft.dpi", NULL, &xrmType, &xrmValue))
                        dpi = strtod(xrmValue.addr, NULL);

        }

        return dpi;
}
static void set_dpi_value(screen_info * scr, double dpi)
{
        if (dpi > 0.0) {
                PangoFontMap *font_map = pango_cairo_font_map_get_default();
                pango_cairo_font_map_set_resolution((PangoCairoFontMap *) font_map, dpi);
        }
#ifdef XRANDR
        //fallback to auto-detect method
        else {
                dpi = (double)scr->dim.h * 25.4 / (double)scr->dim.mmh;
                PangoFontMap *font_map = pango_cairo_font_map_get_default();
                pango_cairo_font_map_set_resolution((PangoCairoFontMap *) font_map, dpi);
        }
#endif
}

#ifdef XRANDR
static int lookup_active_screen(XRRMonitorInfo *info, int n, int x, int y)
{
        int ret = -1;
        for (int i = 0; i < n; i++) {
                if (INRECT(x, y, info[i].x, info[i].y,
                        info[i].width, info[i].height)) {
                        ret = i;
                }
        }

        return ret;

}
#elif XINERAMA
static int lookup_active_screen(XineramaScreenInfo * info, int n, int x, int y)
{
        int ret = -1;
        for (int i = 0; i < n; i++) {
                if (INRECT(x, y, info[i].x_org, info[i].y_org,
                        info[i].width, info[i].height)) {
                        ret = i;
                }
        }

        return ret;
}
#endif


#ifdef XRANDR
/*
 * Select the screen on which the Window
 * should be displayed.
 */
static int select_screen(XRRMonitorInfo *info, int n)
#elif XINERAMA
static int select_screen(XineramaScreenInfo * info, int info_len)
#endif
#if defined(XRANDR) || defined(XINERAMA)
{
         int ret = 0;
         x_follow_setup_error_handler();
         if (settings.f_mode == FOLLOW_NONE) {
                  ret = settings.monitor >=
                     0 ? settings.monitor : XDefaultScreen(xctx.dpy);
                  goto sc_cleanup;

         } else {
                 int x, y;
                 assert(settings.f_mode == FOLLOW_MOUSE
                        || settings.f_mode == FOLLOW_KEYBOARD);
                 Window root =
                     RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));

                 if (settings.f_mode == FOLLOW_MOUSE) {
                         int dummy;
                         unsigned int dummy_ui;
                         Window dummy_win;

                         XQueryPointer(xctx.dpy, root, &dummy_win,
                                       &dummy_win, &x, &y, &dummy,
                                       &dummy, &dummy_ui);
                 }

                 if (settings.f_mode == FOLLOW_KEYBOARD) {

                         Window focused = get_focused_window();

                         if (focused == 0) {
                                 /* something went wrong. Fallback to default */
                                 ret = settings.monitor >=
                                     0 ? settings.monitor : XDefaultScreen(xctx.dpy);
                                 goto sc_cleanup;
                         }

                         Window child_return;
                         XTranslateCoordinates(xctx.dpy, focused, root,
                                               0, 0, &x, &y, &child_return);
                 }

                 ret = lookup_active_screen(info, n, x, y);

                 if (ret > 0)
                        goto sc_cleanup;

                 /* something seems to be wrong. Fallback to default */
                 ret = settings.monitor >=
                     0 ? settings.monitor : XDefaultScreen(xctx.dpy);
                 goto sc_cleanup;
         }
 sc_cleanup:
         x_follow_tear_down_error_handler();
         return ret;
 }
#endif

/*
 * Update the information about the monitor
 * geometry.
 */
void x_screen_info(screen_info * scr)
{
#ifdef XRANDR
        int n;
        XRRMonitorInfo	*m;

        m = XRRGetMonitors(xctx.dpy, RootWindow(xctx.dpy, DefaultScreen(xctx.dpy)), true, &n);
        int screen = select_screen(m, n);
        if (screen >= n) {
                /* invalid monitor, fallback to default */
                screen = 0;
        }

        scr->dim.x = m[screen].x;
        scr->dim.y = m[screen].y;
        scr->dim.w = m[screen].width;
        scr->dim.h = m[screen].height;
        scr->dim.mmh = m[screen].mheight;
        XRRFreeMonitors(m);
#elif XINERAMA
        int n;
        XineramaScreenInfo *info;
        if ((info = XineramaQueryScreens(xctx.dpy, &n))) {
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
                        screen = DefaultScreen(xctx.dpy);

                scr->dim.w = DisplayWidth(xctx.dpy, screen);
                scr->dim.h = DisplayHeight(xctx.dpy, screen);
        }

        //Update dpi
        double dpi = 0.0;

        dpi = get_xft_dpi_value();
        set_dpi_value(scr, dpi);
}

/*
 * Return the window that currently has
 * the keyboard focus.
 */
static Window get_focused_window(void)
{
        Window focused = 0;
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *prop_return = NULL;
        Window root = RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));
        Atom netactivewindow =
            XInternAtom(xctx.dpy, "_NET_ACTIVE_WINDOW", false);

        XGetWindowProperty(xctx.dpy, root, netactivewindow, 0L,
                           sizeof(Window), false, XA_WINDOW,
                           &type, &format, &nitems, &bytes_after, &prop_return);
        if (prop_return) {
                focused = *(Window *) prop_return;
                XFree(prop_return);
        }

        return focused;
}

static void x_follow_setup_error_handler(void)
{
        dunst_follow_errored = false;

        XFlush(xctx.dpy);
        XSetErrorHandler(FollowXErrorHandler);
}

static int x_follow_tear_down_error_handler(void)
{
        XFlush(xctx.dpy);
        XSync(xctx.dpy, false);
        XSetErrorHandler(NULL);
        return dunst_follow_errored;
}

static int FollowXErrorHandler(Display * display, XErrorEvent * e)
{
        dunst_follow_errored = true;
        char err_buf[BUFSIZ];
        XGetErrorText(display, e->error_code, err_buf, BUFSIZ);
        fputs(err_buf, stderr);
        fputs("\n", stderr);

        return 0;
}

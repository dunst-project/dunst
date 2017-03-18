#include "screen.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#ifdef XRANDR
#include <X11/extensions/randr.h>
#include <X11/extensions/Xrandr.h>
#elif XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <assert.h>
#include <glib.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/settings.h"
#include "x.h"

screen_info *screens;
int screens_len;

bool dunst_follow_errored = false;

void x_update_screens_fallback();
static void x_follow_setup_error_handler(void);
static int x_follow_tear_down_error_handler(void);
static int FollowXErrorHandler(Display *display, XErrorEvent *e);
static Window get_focused_window(void);

static double get_xft_dpi_value()
{
        static double dpi = -1;
        //Only run this once, we don't expect dpi changes during runtime
        if (dpi <= -1) {
                XrmInitialize();
                char *xRMS = XResourceManagerString(xctx.dpy);

                if (xRMS == NULL) {
                        dpi = 0;
                        return 0;
                }

                XrmDatabase xDB = XrmGetStringDatabase(xRMS);
                char *xrmType;
                XrmValue xrmValue;

                if (XrmGetResource(xDB, "Xft.dpi", NULL, &xrmType, &xrmValue)) {
                        dpi = strtod(xrmValue.addr, NULL);
                } else {
                        dpi = 0;
                }
                XrmDestroyDatabase(xDB);
        }
        return dpi;
}

void alloc_screen_ar(int n)
{
        assert(n > 0);
        if (n <= screens_len) return;

        screens = g_realloc(screens, n * sizeof(screen_info));

        memset(screens, 0, n * sizeof(screen_info));

        screens_len = n;
}

#ifdef XRANDR
int randr_event_base = 0;

void init_screens()
{
        int randr_error_base = 0;
        XRRQueryExtension(xctx.dpy, &randr_event_base, &randr_error_base);
        XRRSelectInput(xctx.dpy, RootWindow(xctx.dpy, DefaultScreen(xctx.dpy)), RRScreenChangeNotifyMask);
        x_update_screens();
}

void x_update_screens()
{
        int n;
        XRRMonitorInfo *m = XRRGetMonitors(xctx.dpy, RootWindow(xctx.dpy, DefaultScreen(xctx.dpy)), true, &n);

        if (n == -1) {
                x_update_screens_fallback();
                return;
        }

        alloc_screen_ar(n);

        for (int i = 0; i < n; i++) {
                screens[i].dim.x = m[i].x;
                screens[i].dim.y = m[i].y;
                screens[i].dim.w = m[i].width;
                screens[i].dim.h = m[i].height;
                screens[i].dim.mmh = m[i].mheight;
        }

        XRRFreeMonitors(m);
}

static int autodetect_dpi(screen_info *scr)
{
        return (double)scr->dim.h * 25.4 / (double)scr->dim.mmh;
}

void screen_check_event(XEvent event)
{
        if (event.type == randr_event_base + RRScreenChangeNotify)
                x_update_screens();
}

#elif XINERAMA

void init_screens()
{
        x_update_screens();
}

void x_update_screens()
{
        int n;
        XineramaScreenInfo *info = XineramaQueryScreens(xctx.dpy, &n);

        if (!info) {
                x_update_screens_fallback();
                return;
        }

        alloc_screen_ar(n);

        for (int i = 0; i < n; i++) {
                screens[i].dim.x = info[i].x_org;
                screens[i].dim.y = info[i].y_org;
                screens[i].dim.h = info[i].height;
                screens[i].dim.w = info[i].width;
        }
        XFree(info);
}

void screen_check_event(XEvent event) {} //No-op

#define autodetect_dpi(x) 0

#else

void init_screens()
{
        x_update_screens_fallback();
}

void x_update_screens()
{
        x_update_screens_fallback();
}

void screen_check_event(XEvent event) {} //No-op

#define autodetect_dpi(x) 0

#endif

void x_update_screens_fallback()
{
        alloc_screen_ar(1);

        int screen;
        if (settings.monitor >= 0)
                screen = settings.monitor;
        else
                screen = DefaultScreen(xctx.dpy);

        screens[0].dim.w = DisplayWidth(xctx.dpy, screen);
        screens[0].dim.h = DisplayHeight(xctx.dpy, screen);

}

/*
 * Select the screen on which the Window
 * should be displayed.
 */
screen_info *get_active_screen()
{
        int ret = 0;
        if (settings.monitor > 0 && settings.monitor < screens_len) {
                ret = settings.monitor;
                goto sc_cleanup;
        }

        x_follow_setup_error_handler();

        if (settings.f_mode == FOLLOW_NONE) {
                ret = XDefaultScreen(xctx.dpy);
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
                                ret = XDefaultScreen(xctx.dpy);
                                goto sc_cleanup;
                        }

                        Window child_return;
                        XTranslateCoordinates(xctx.dpy, focused, root,
                                        0, 0, &x, &y, &child_return);
                }

                for (int i = 0; i < screens_len; i++) {
                        if (INRECT(x, y, screens[i].dim.x, screens[i].dim.y,
                                         screens[i].dim.w, screens[i].dim.h)) {
                                ret = i;
                        }
                }

                if (ret > 0)
                        goto sc_cleanup;

                /* something seems to be wrong. Fallback to default */
                ret = XDefaultScreen(xctx.dpy);
                goto sc_cleanup;
        }
sc_cleanup:
        x_follow_tear_down_error_handler();
        assert(screens);
        assert(ret >= 0 && ret < screens_len);
        return &screens[ret];
}

double get_dpi_for_screen(screen_info *scr)
{
        double dpi = 0;
        if ((dpi = get_xft_dpi_value()))
                return dpi;
        else if (settings.per_monitor_dpi && (dpi = autodetect_dpi(scr)))
                return dpi;
        else
                return 96;
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
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

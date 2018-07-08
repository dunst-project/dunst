#include "screen.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>
#include <assert.h>
#include <glib.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/log.h"
#include "src/settings.h"
#include "x.h"

screen_info *screens;
int screens_len;

bool dunst_follow_errored = false;

int randr_event_base = 0;

static int randr_major_version = 0;
static int randr_minor_version = 0;

void randr_init(void);
void randr_update(void);
void xinerama_update(void);
void screen_update_fallback(void);
static void x_follow_setup_error_handler(void);
static int x_follow_tear_down_error_handler(void);
static int FollowXErrorHandler(Display *display, XErrorEvent *e);
static Window get_focused_window(void);

static double get_xft_dpi_value(void)
{
        static double dpi = -1;
        //Only run this once, we don't expect dpi changes during runtime
        if (dpi <= -1) {
                XrmInitialize();
                char *xRMS = XResourceManagerString(xctx.dpy);

                if (!xRMS) {
                        dpi = 0;
                        return 0;
                }

                XrmDatabase xDB = XrmGetStringDatabase(xRMS);
                char *xrmType;
                XrmValue xrmValue;

                if (XrmGetResource(xDB, "Xft.dpi", "Xft.dpi", &xrmType, &xrmValue)) {
                        dpi = strtod(xrmValue.addr, NULL);
                } else {
                        dpi = 0;
                }
                XrmDestroyDatabase(xDB);
        }
        return dpi;
}

void init_screens(void)
{
        if (!settings.force_xinerama) {
                randr_init();
                randr_update();
        } else {
                xinerama_update();
        }
}

void alloc_screen_ar(int n)
{
        assert(n > 0);
        if (n <= screens_len) return;

        screens = g_realloc(screens, n * sizeof(screen_info));

        memset(screens, 0, n * sizeof(screen_info));

        screens_len = n;
}

void randr_init(void)
{
        int randr_error_base = 0;
        if (!XRRQueryExtension(xctx.dpy, &randr_event_base, &randr_error_base)) {
                LOG_W("Could not initialize the RandR extension. "
                      "Falling back to single monitor mode.");
                return;
        }
        XRRQueryVersion(xctx.dpy, &randr_major_version, &randr_minor_version);
        XRRSelectInput(xctx.dpy, RootWindow(xctx.dpy, DefaultScreen(xctx.dpy)), RRScreenChangeNotifyMask);
}

void randr_update(void)
{
        if (randr_major_version < 1
            || (randr_major_version == 1 && randr_minor_version < 5)) {
                LOG_W("Server RandR version too low (%i.%i). "
                      "Falling back to single monitor mode.",
                      randr_major_version,
                      randr_minor_version);
                screen_update_fallback();
                return;
        }

        int n = 0;
        XRRMonitorInfo *m = XRRGetMonitors(xctx.dpy, RootWindow(xctx.dpy, DefaultScreen(xctx.dpy)), true, &n);

        if (n < 1) {
                LOG_C("Get monitors reported %i monitors. "
                      "Falling back to single monitor mode.", n);
                screen_update_fallback();
                return;
        }

        assert(m);

        alloc_screen_ar(n);

        for (int i = 0; i < n; i++) {
                screens[i].id = i;
                screens[i].x = m[i].x;
                screens[i].y = m[i].y;
                screens[i].w = m[i].width;
                screens[i].h = m[i].height;
                screens[i].mmh = m[i].mheight;
        }

        XRRFreeMonitors(m);
}

static int autodetect_dpi(screen_info *scr)
{
        return (double)scr->h * 25.4 / (double)scr->mmh;
}

void screen_check_event(XEvent event)
{
        if (event.type == randr_event_base + RRScreenChangeNotify) {
                LOG_D("XEvent: processing 'RRScreenChangeNotify'");
                randr_update();

        } else {
                LOG_D("XEvent: Ignoring '%d'", event.type);
        }
}

void xinerama_update(void)
{
        int n;
        XineramaScreenInfo *info = XineramaQueryScreens(xctx.dpy, &n);

        if (!info) {
                LOG_W("Could not get xinerama screen info. "
                      "Falling back to single monitor mode.");
                screen_update_fallback();
                return;
        }

        alloc_screen_ar(n);

        for (int i = 0; i < n; i++) {
                screens[i].id = i;
                screens[i].x = info[i].x_org;
                screens[i].y = info[i].y_org;
                screens[i].h = info[i].height;
                screens[i].w = info[i].width;
        }
        XFree(info);
}

void screen_update_fallback(void)
{
        alloc_screen_ar(1);

        int screen;
        if (settings.monitor >= 0)
                screen = settings.monitor;
        else
                screen = DefaultScreen(xctx.dpy);

        screens[0].w = DisplayWidth(xctx.dpy, screen);
        screens[0].h = DisplayHeight(xctx.dpy, screen);
}

/* see screen.h */
bool have_fullscreen_window(void)
{
        return window_is_fullscreen(get_focused_window());
}

/**
 * X11 ErrorHandler to mainly discard BadWindow parameter error
 */
static int XErrorHandlerFullscreen(Display *display, XErrorEvent *e)
{
        /* Ignore BadWindow errors. Window may have been gone */
        if (e->error_code == BadWindow) {
                return 0;
        }

        char err_buf[BUFSIZ];
        XGetErrorText(display, e->error_code, err_buf, BUFSIZ);
        fputs(err_buf, stderr);
        fputs("\n", stderr);

        return 0;
}

/* see screen.h */
bool window_is_fullscreen(Window window)
{
        bool fs = false;

        if (!window)
                return false;

        Atom has_wm_state = XInternAtom(xctx.dpy, "_NET_WM_STATE", True);
        if (has_wm_state == None){
                return false;
        }

        XFlush(xctx.dpy);
        XSetErrorHandler(XErrorHandlerFullscreen);

        Atom actual_type_return;
        int actual_format_return;
        unsigned long bytes_after_return;
        unsigned char *prop_to_return;
        unsigned long n_items;
        int result = XGetWindowProperty(
                        xctx.dpy,
                        window,
                        has_wm_state,
                        0,                     /* long_offset */
                        sizeof(window),        /* long_length */
                        false,                 /* delete */
                        AnyPropertyType,       /* req_type */
                        &actual_type_return,
                        &actual_format_return,
                        &n_items,
                        &bytes_after_return,
                        &prop_to_return);

        XFlush(xctx.dpy);
        XSync(xctx.dpy, false);
        XSetErrorHandler(NULL);

        if (result == Success) {
                for(int i = 0; i < n_items; i++) {
                        char *atom = XGetAtomName(xctx.dpy, ((Atom*)prop_to_return)[i]);

                        if (atom) {
                                if(0 == strcmp("_NET_WM_STATE_FULLSCREEN", atom))
                                        fs = true;
                                XFree(atom);
                                if(fs)
                                        break;
                        }
                }
        }

        if (prop_to_return)
                XFree(prop_to_return);

        return fs;
}

/*
 * Select the screen on which the Window
 * should be displayed.
 */
screen_info *get_active_screen(void)
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

                        XQueryPointer(xctx.dpy,
                                      root,
                                      &dummy_win,
                                      &dummy_win,
                                      &x,
                                      &y,
                                      &dummy,
                                      &dummy,
                                      &dummy_ui);
                }

                if (settings.f_mode == FOLLOW_KEYBOARD) {

                        Window focused = get_focused_window();

                        if (focused == 0) {
                                /* something went wrong. Fall back to default */
                                ret = XDefaultScreen(xctx.dpy);
                                goto sc_cleanup;
                        }

                        Window child_return;
                        XTranslateCoordinates(xctx.dpy, focused, root,
                                        0, 0, &x, &y, &child_return);
                }

                for (int i = 0; i < screens_len; i++) {
                        if (INRECT(x, y, screens[i].x, screens[i].y,
                                         screens[i].w, screens[i].h)) {
                                ret = i;
                        }
                }

                if (ret > 0)
                        goto sc_cleanup;

                /* something seems to be wrong. Fall back to default */
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
        if ((!settings.force_xinerama && settings.per_monitor_dpi &&
                (dpi = autodetect_dpi(scr))))
                return dpi;
        else if ((dpi = get_xft_dpi_value()))
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

        XGetWindowProperty(xctx.dpy,
                           root,
                           netactivewindow,
                           0L,
                           sizeof(Window),
                           false,
                           XA_WINDOW,
                           &type,
                           &format,
                           &nitems,
                           &bytes_after,
                           &prop_return);
        if (prop_return) {
                focused = *(Window *)prop_return;
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

static int FollowXErrorHandler(Display *display, XErrorEvent *e)
{
        dunst_follow_errored = true;
        char err_buf[BUFSIZ];
        XGetErrorText(display, e->error_code, err_buf, BUFSIZ);
        LOG_W("%s", err_buf);

        return 0;
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

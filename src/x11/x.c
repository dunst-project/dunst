/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#include "x.h"

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <cairo-xlib.h>
#include <cairo.h>
#include <glib-object.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "src/draw.h"
#include "src/dbus.h"
#include "src/dunst.h"
#include "src/log.h"
#include "src/markup.h"
#include "src/notification.h"
#include "src/queues.h"
#include "src/settings.h"
#include "src/utils.h"

#include "screen.h"

#define WIDTH 400
#define HEIGHT 400

xctx_t xctx;
bool dunst_grab_errored = false;

static bool fullscreen_last = false;

/* FIXME refactor setup teardown handlers into one setup and one teardown */
static void x_shortcut_setup_error_handler(void);
static int x_shortcut_tear_down_error_handler(void);
static void setopacity(Window win, unsigned long opacity);
static void x_handle_click(XEvent ev);
static void x_win_setup(void);

cairo_surface_t *x_create_cairo_surface(void)
{
        return cairo_xlib_surface_create(xctx.dpy,
                        xctx.win, DefaultVisual(xctx.dpy, 0), WIDTH, HEIGHT);
}

void x_win_move(int width, int height)
{

        int x, y;
        screen_info *scr = get_active_screen();
        xctx.cur_screen = scr->scr;
        /* calculate window position */
        if (xctx.geometry.mask & XNegative) {
                x = (scr->dim.x + (scr->dim.w - width)) + xctx.geometry.x;
        } else {
                x = scr->dim.x + xctx.geometry.x;
        }

        if (xctx.geometry.mask & YNegative) {
                y = scr->dim.y + (scr->dim.h + xctx.geometry.y) - height;
        } else {
                y = scr->dim.y + xctx.geometry.y;
        }

        /* move and resize */
        if (x != xctx.window_dim.x || y != xctx.window_dim.y) {
                XMoveWindow(xctx.dpy, xctx.win, x, y);
        }
        if (width != xctx.window_dim.w || height != xctx.window_dim.h) {
                XResizeWindow(xctx.dpy, xctx.win, width, height);
        }

        xctx.window_dim.x = x;
        xctx.window_dim.y = y;
        xctx.window_dim.h = height;
        xctx.window_dim.w = width;
}

static void setopacity(Window win, unsigned long opacity)
{
        Atom _NET_WM_WINDOW_OPACITY =
            XInternAtom(xctx.dpy, "_NET_WM_WINDOW_OPACITY", false);
        XChangeProperty(xctx.dpy,
                        win,
                        _NET_WM_WINDOW_OPACITY,
                        XA_CARDINAL,
                        32,
                        PropModeReplace,
                        (unsigned char *)&opacity,
                        1L);
}

/*
 * Returns the modifier which is NumLock.
 */
static KeySym x_numlock_mod(void)
{
        static KeyCode nl = 0;
        KeySym sym = 0;
        XModifierKeymap *map = XGetModifierMapping(xctx.dpy);

        if (!nl)
                nl = XKeysymToKeycode(xctx.dpy, XStringToKeysym("Num_Lock"));

        for (int mod = 0; mod < 8; mod++) {
                for (int j = 0; j < map->max_keypermod; j++) {
                        if (map->modifiermap[mod*map->max_keypermod+j] == nl) {
                                /* In theory, one could use `1 << mod`, but this
                                 * could count as 'using implementation details',
                                 * so use this large switch. */
                                switch (mod) {
                                case ShiftMapIndex:
                                        sym = ShiftMask;
                                        goto end;
                                case LockMapIndex:
                                        sym = LockMask;
                                        goto end;
                                case ControlMapIndex:
                                        sym = ControlMask;
                                        goto end;
                                case Mod1MapIndex:
                                        sym = Mod1Mask;
                                        goto end;
                                case Mod2MapIndex:
                                        sym = Mod2Mask;
                                        goto end;
                                case Mod3MapIndex:
                                        sym = Mod3Mask;
                                        goto end;
                                case Mod4MapIndex:
                                        sym = Mod4Mask;
                                        goto end;
                                case Mod5MapIndex:
                                        sym = Mod5Mask;
                                        goto end;
                                }
                        }
                }
        }

end:
        XFreeModifiermap(map);
        return sym;
}

/*
 * Helper function to use glib's mainloop mechanic
 * with Xlib
 */
gboolean x_mainloop_fd_prepare(GSource *source, gint *timeout)
{
        if (timeout)
                *timeout = -1;
        return false;
}

/*
 * Helper function to use glib's mainloop mechanic
 * with Xlib
 */
gboolean x_mainloop_fd_check(GSource *source)
{
        return XPending(xctx.dpy) > 0;
}

/*
 * Main Dispatcher for XEvents
 */
gboolean x_mainloop_fd_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
        bool fullscreen_now;
        XEvent ev;
        unsigned int state;
        while (XPending(xctx.dpy) > 0) {
                XNextEvent(xctx.dpy, &ev);
                LOG_D("XEvent: processing '%d'", ev.type);

                switch (ev.type) {
                case Expose:
                        if (ev.xexpose.count == 0 && xctx.visible) {
                                draw();
                        }
                        break;
                case SelectionNotify:
                        if (ev.xselection.property == xctx.utf8)
                                break;
                case ButtonRelease:
                        if (ev.xbutton.window == xctx.win) {
                                x_handle_click(ev);
                                wake_up();
                        }
                        break;
                case KeyPress:
                        state = ev.xkey.state;
                        /* NumLock is also encoded in the state. Remove it. */
                        state &= ~x_numlock_mod();
                        if (settings.close_ks.str
                            && XLookupKeysym(&ev.xkey,
                                             0) == settings.close_ks.sym
                            && settings.close_ks.mask == state) {
                                const GList *displayed = queues_get_displayed();
                                if (displayed && displayed->data) {
                                        queues_notification_close(displayed->data, REASON_USER);
                                        wake_up();
                                }
                        }
                        if (settings.history_ks.str
                            && XLookupKeysym(&ev.xkey,
                                             0) == settings.history_ks.sym
                            && settings.history_ks.mask == state) {
                                queues_history_pop();
                                wake_up();
                        }
                        if (settings.close_all_ks.str
                            && XLookupKeysym(&ev.xkey,
                                             0) == settings.close_all_ks.sym
                            && settings.close_all_ks.mask == state) {
                                queues_history_push_all();
                                wake_up();
                        }
                        if (settings.context_ks.str
                            && XLookupKeysym(&ev.xkey,
                                             0) == settings.context_ks.sym
                            && settings.context_ks.mask == state) {
                                context_menu();
                                wake_up();
                        }
                        break;
                case FocusIn:
                case FocusOut:
                        wake_up();
                        break;
                case PropertyNotify:
                        fullscreen_now = have_fullscreen_window();

                        if (fullscreen_now != fullscreen_last) {
                                fullscreen_last = fullscreen_now;
                                wake_up();
                        } else if (   settings.f_mode != FOLLOW_NONE
                        /* Ignore PropertyNotify, when we're still on the
                         * same screen. PropertyNotify is only neccessary
                         * to detect a focus change to another screen
                         */
                                   && xctx.visible
                                   && get_active_screen()->scr != xctx.cur_screen) {
                                draw();
                        }
                        break;
                default:
                        screen_check_event(ev);
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
        XScreenSaverQueryInfo(xctx.dpy, DefaultRootWindow(xctx.dpy),
                              xctx.screensaver_info);
        if (settings.idle_threshold == 0) {
                return false;
        }
        return xctx.screensaver_info->idle > settings.idle_threshold / 1000;
}

/* TODO move to x_mainloop_* */
/*
 * Handle incoming mouse click events
 */
static void x_handle_click(XEvent ev)
{
        if (ev.xbutton.button == Button3) {
                queues_history_push_all();

                return;
        }

        if (ev.xbutton.button == Button1 || ev.xbutton.button == Button2) {
                int y = settings.separator_height;
                notification *n = NULL;
                int first = true;
                for (const GList *iter = queues_get_displayed(); iter;
                     iter = iter->next) {
                        n = iter->data;
                        if (ev.xbutton.y > y && ev.xbutton.y < y + n->displayed_height)
                                break;

                        y += n->displayed_height + settings.separator_height;
                        if (first)
                                y += settings.frame_width;
                }

                if (n) {
                        if (ev.xbutton.button == Button1)
                                queues_notification_close(n, REASON_USER);
                        else
                                notification_do_action(n);
                }
        }
}

void x_free(void)
{
        if (xctx.dpy)
                XCloseDisplay(xctx.dpy);
}

/*
 * Setup X11 stuff
 */
void x_setup(void)
{

        /* initialize xctx.dc, font, keyboard, colors */
        if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
                LOG_W("No locale support");
        if (!(xctx.dpy = XOpenDisplay(NULL))) {
                DIE("Cannot open X11 display.");
        }

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

        xctx.colors[ColFG][URG_LOW] = settings.lowfgcolor;
        xctx.colors[ColFG][URG_NORM] = settings.normfgcolor;
        xctx.colors[ColFG][URG_CRIT] = settings.critfgcolor;

        xctx.colors[ColBG][URG_LOW] = settings.lowbgcolor;
        xctx.colors[ColBG][URG_NORM] = settings.normbgcolor;
        xctx.colors[ColBG][URG_CRIT] = settings.critbgcolor;

        if (settings.lowframecolor)
                xctx.colors[ColFrame][URG_LOW] = settings.lowframecolor;
        else
                xctx.colors[ColFrame][URG_LOW] = settings.frame_color;
        if (settings.normframecolor)
                xctx.colors[ColFrame][URG_NORM] = settings.normframecolor;
        else
                xctx.colors[ColFrame][URG_NORM] = settings.frame_color;
        if (settings.critframecolor)
                xctx.colors[ColFrame][URG_CRIT] = settings.critframecolor;
        else
                xctx.colors[ColFrame][URG_CRIT] = settings.frame_color;

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

        /* calculate maximum notification count and push information to queue */
        if (xctx.geometry.h == 0) {
                queues_displayed_limit(0);
        } else if (xctx.geometry.h == 1) {
                queues_displayed_limit(1);
        } else if (settings.indicate_hidden) {
                queues_displayed_limit(xctx.geometry.h - 1);
        } else {
                queues_displayed_limit(xctx.geometry.h);
        }

        xctx.screensaver_info = XScreenSaverAllocInfo();

        init_screens();
        x_win_setup();
        x_shortcut_grab(&settings.history_ks);
}

static void x_set_wm(Window win)
{

        Atom data[2];

        /* set window title */
        char *title = settings.title != NULL ? settings.title : "Dunst";
        Atom _net_wm_title =
                XInternAtom(xctx.dpy, "_NET_WM_NAME", false);

        XStoreName(xctx.dpy, win, title);
        XChangeProperty(xctx.dpy,
                        win,
                        _net_wm_title,
                        XInternAtom(xctx.dpy, "UTF8_STRING", false),
                        8,
                        PropModeReplace,
                        (unsigned char *)title,
                        strlen(title));

        /* set window class */
        char *class = settings.class != NULL ? settings.class : "Dunst";
        XClassHint classhint = { class, "Dunst" };

        XSetClassHint(xctx.dpy, win, &classhint);

        /* set window type */
        Atom net_wm_window_type =
                XInternAtom(xctx.dpy, "_NET_WM_WINDOW_TYPE", false);

        data[0] = XInternAtom(xctx.dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", false);
        data[1] = XInternAtom(xctx.dpy, "_NET_WM_WINDOW_TYPE_UTILITY", false);

        XChangeProperty(xctx.dpy,
                        win,
                        net_wm_window_type,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char *)data,
                        2L);

        /* set state above */
        Atom net_wm_state =
                XInternAtom(xctx.dpy, "_NET_WM_STATE", false);

        data[0] = XInternAtom(xctx.dpy, "_NET_WM_STATE_ABOVE", false);

        XChangeProperty(xctx.dpy, win, net_wm_state, XA_ATOM, 32,
                PropModeReplace, (unsigned char *) data, 1L);
}

/*
 * Setup the window
 */
static void x_win_setup(void)
{

        Window root;
        XSetWindowAttributes wa;

        xctx.window_dim.x = 0;
        xctx.window_dim.y = 0;
        xctx.window_dim.w = 0;
        xctx.window_dim.h = 0;

        root = RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));
        xctx.utf8 = XInternAtom(xctx.dpy, "UTF8_STRING", false);

        wa.override_redirect = true;
        wa.background_pixmap = ParentRelative;
        wa.event_mask =
            ExposureMask | KeyPressMask | VisibilityChangeMask |
            ButtonReleaseMask | FocusChangeMask| StructureNotifyMask;

        screen_info *scr = get_active_screen();
        xctx.win = XCreateWindow(xctx.dpy,
                                 root,
                                 scr->dim.x,
                                 scr->dim.y,
                                 scr->dim.w,
                                 1,
                                 0,
                                 DefaultDepth(xctx.dpy, DefaultScreen(xctx.dpy)),
                                 CopyFromParent,
                                 DefaultVisual(xctx.dpy, DefaultScreen(xctx.dpy)),
                                 CWOverrideRedirect | CWBackPixmap | CWEventMask,
                                 &wa);

        x_set_wm(xctx.win);
        settings.transparency =
            settings.transparency > 100 ? 100 : settings.transparency;
        setopacity(xctx.win,
                   (unsigned long)((100 - settings.transparency) *
                                   (0xffffffff / 100)));

        if (settings.f_mode != FOLLOW_NONE) {
                long root_event_mask = FocusChangeMask | PropertyChangeMask;
                XSelectInput(xctx.dpy, root, root_event_mask);
        }
}

/*
 * Show the window and grab shortcuts.
 */
void x_win_show(void)
{
        /* window is already mapped or there's nothing to show */
        if (xctx.visible || queues_length_displayed() == 0) {
                return;
        }

        x_shortcut_grab(&settings.close_ks);
        x_shortcut_grab(&settings.close_all_ks);
        x_shortcut_grab(&settings.context_ks);

        x_shortcut_setup_error_handler();
        XGrabButton(xctx.dpy,
                    AnyButton,
                    AnyModifier,
                    xctx.win,
                    false,
                    BUTTONMASK,
                    GrabModeAsync,
                    GrabModeSync,
                    None,
                    None);
        if (x_shortcut_tear_down_error_handler()) {
                LOG_W("Unable to grab mouse button(s).");
        }

        XMapRaised(xctx.dpy, xctx.win);
        xctx.visible = true;
}

/*
 * Hide the window and ungrab unused keyboard_shortcuts
 */
void x_win_hide(void)
{
        x_shortcut_ungrab(&settings.close_ks);
        x_shortcut_ungrab(&settings.close_all_ks);
        x_shortcut_ungrab(&settings.context_ks);

        XUngrabButton(xctx.dpy, AnyButton, AnyModifier, xctx.win);
        XUnmapWindow(xctx.dpy, xctx.win);
        XFlush(xctx.dpy);
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
                LOG_W("Unknown Modifier: '%s'", str);
                return 0;
        }
}

/*
 * Error handler for grabbing mouse and keyboard errors.
 */
static int GrabXErrorHandler(Display *display, XErrorEvent *e)
{
        dunst_grab_errored = true;
        char err_buf[BUFSIZ];
        XGetErrorText(display, e->error_code, err_buf, BUFSIZ);

        if (e->error_code != BadAccess) {
                DIE("%s", err_buf);
        } else {
                LOG_W("%s", err_buf);
        }

        return 0;
}

/*
 * Setup the Error handler.
 */
static void x_shortcut_setup_error_handler(void)
{
        dunst_grab_errored = false;

        XFlush(xctx.dpy);
        XSetErrorHandler(GrabXErrorHandler);
}

/*
 * Tear down the Error handler.
 */
static int x_shortcut_tear_down_error_handler(void)
{
        XFlush(xctx.dpy);
        XSync(xctx.dpy, false);
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
        root = RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));

        x_shortcut_setup_error_handler();

        if (ks->is_valid) {
                XGrabKey(xctx.dpy,
                         ks->code,
                         ks->mask,
                         root,
                         true,
                         GrabModeAsync,
                         GrabModeAsync);
                XGrabKey(xctx.dpy,
                         ks->code,
                         ks->mask | x_numlock_mod(),
                         root,
                         true,
                         GrabModeAsync,
                         GrabModeAsync);
        }

        if (x_shortcut_tear_down_error_handler()) {
                LOG_W("Unable to grab key '%s'.", ks->str);
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
        root = RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));
        if (ks->is_valid) {
                XUngrabKey(xctx.dpy, ks->code, ks->mask, root);
                XUngrabKey(xctx.dpy, ks->code, ks->mask | x_numlock_mod(), root);
        }
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

        while (strchr(str, '+')) {
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
        XDisplayKeycodes(xctx.dpy, &min_keysym, &max_keysym);

        ks->code = NoSymbol;

        for (int i = min_keysym; i <= max_keysym; i++) {
                if (XkbKeycodeToKeysym(xctx.dpy, i, 0, 0) == ks->sym
                    || XkbKeycodeToKeysym(xctx.dpy, i, 0, 1) == ks->sym) {
                        ks->code = i;
                        break;
                }
        }

        if (ks->sym == NoSymbol || ks->code == NoSymbol) {
                LOG_W("Unknown keyboard shortcut: '%s'", ks->str);
                ks->is_valid = false;
        } else {
                ks->is_valid = true;
        }

        g_free(str_begin);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

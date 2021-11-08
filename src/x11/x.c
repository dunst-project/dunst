/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#include "x.h"

#include <assert.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <glib-object.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <linux/input-event-codes.h>

#include "../dbus.h"
#include "../draw.h"
#include "../dunst.h"
#include "../log.h"
#include "../markup.h"
#include "../menu.h"
#include "../notification.h"
#include "../queues.h"
#include "../settings.h"
#include "../utils.h"
#include "../input.h"

#include "screen.h"

#define WIDTH 400
#define HEIGHT 400

struct window_x11 {
        Window xwin;
        cairo_surface_t *root_surface;
        cairo_t *c_ctx;
        GSource *esrc;
        int cur_screen;
        bool visible;
        struct dimensions dim;
};

struct x11_source {
        GSource source;
        struct window_x11 *win;
};

struct x_context xctx;
bool dunst_grab_errored = false;

static bool fullscreen_last = false;

static void XRM_update_db(void);

static void x_shortcut_init(struct keyboard_shortcut *ks);
static int x_shortcut_grab(struct keyboard_shortcut *ks);
static void x_shortcut_ungrab(struct keyboard_shortcut *ks);
/* FIXME refactor setup teardown handlers into one setup and one teardown */
static void x_shortcut_setup_error_handler(void);
static int x_shortcut_tear_down_error_handler(void);
static void setopacity(Window win, unsigned long opacity);
static void x_handle_click(XEvent ev);

static void x_win_move(window winptr, int x, int y, int width, int height)
{
        struct window_x11 *win = (struct window_x11*)winptr;

        /* move and resize */
        if (x != win->dim.x || y != win->dim.y) {
                XMoveWindow(xctx.dpy, win->xwin, x, y);

                win->dim.x = x;
                win->dim.y = y;
        }

        if (width != win->dim.w || height != win->dim.h) {
                XResizeWindow(xctx.dpy, win->xwin, width, height);

                win->dim.h = height;
                win->dim.w = width;
        }
}

static void x_win_corners_shape(struct window_x11 *win, const int rad)
{
        const int width = win->dim.w;
        const int height = win->dim.h;

        Pixmap mask;
        cairo_surface_t * cxbm;
        cairo_t * cr;
        Screen * scr;

        mask = XCreatePixmap(xctx.dpy, win->xwin, width, height, 1);
        scr = ScreenOfDisplay(xctx.dpy, win->cur_screen);
        cxbm = cairo_xlib_surface_create_for_bitmap(xctx.dpy, mask, scr, width, height);
        cr = cairo_create(cxbm);

        cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_paint(cr);
        cairo_set_source_rgba(cr, 1, 1, 1, 1);

        draw_rounded_rect(cr, 0, 0,
                          width, height,
                          rad, 1,
                          true, true);
        cairo_fill(cr);

        cairo_show_page(cr);
        cairo_destroy(cr);
        cairo_surface_flush(cxbm);
        cairo_surface_destroy(cxbm);

        XShapeCombineMask(xctx.dpy, win->xwin, ShapeBounding, 0, 0, mask, ShapeSet);

        XFreePixmap(xctx.dpy, mask);

        XShapeSelectInput(xctx.dpy,
                win->xwin, ShapeNotifyMask);
}

static void x_win_corners_unshape(window winptr)
{
        struct window_x11 *win = (struct window_x11*)winptr;
        XRectangle rect = {
                .x = 0,
                .y = 0,
                .width = win->dim.w,
                .height = win->dim.h };
        XShapeCombineRectangles(xctx.dpy, win->xwin, ShapeBounding, 0, 0, &rect, 1, ShapeSet, 1);
        XShapeSelectInput(xctx.dpy,
                win->xwin, ShapeNotifyMask);
}

static bool x_win_composited(struct window_x11 *win)
{
        char astr[sizeof("_NET_WM_CM_S") / sizeof(char) + 8];
        Atom cm_sel;

        sprintf(astr, "_NET_WM_CM_S%i", win->cur_screen);
        cm_sel = XInternAtom(xctx.dpy, astr, true);

        if (cm_sel == None) {
                return false;
        } else {
                return XGetSelectionOwner(xctx.dpy, cm_sel) != None;
        }
}

void x_display_surface(cairo_surface_t *srf, window winptr, const struct dimensions *dim)
{
        struct window_x11 *win = (struct window_x11*)winptr;
        const struct screen_info *scr = get_active_screen();
        double scale = x_get_scale();
        int x, y;

        calc_window_pos(scr, round(dim->w * scale), round(dim->h * scale), &x, &y);

        x_win_move(win, x, y, round(dim->w * scale), round(dim->h * scale));
        cairo_xlib_surface_set_size(win->root_surface, round(dim->w * scale), round(dim->h * scale));

        XClearWindow(xctx.dpy, win->xwin);

        cairo_set_source_surface(win->c_ctx, srf, 0, 0);
        cairo_paint(win->c_ctx);
        cairo_show_page(win->c_ctx);

        if (settings.corner_radius != 0 && ! x_win_composited(win))
                x_win_corners_shape(win, round(dim->corner_radius * scale));
        else
                x_win_corners_unshape(win);

        XFlush(xctx.dpy);

}

cairo_t* x_win_get_context(window winptr)
{
        return ((struct window_x11*)win)->c_ctx;
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
        struct window_x11 *win = ((struct x11_source*) source)->win;

        bool fullscreen_now;
        const struct screen_info *scr;
        XEvent ev;
        unsigned int state;
        while (XPending(xctx.dpy) > 0) {
                XNextEvent(xctx.dpy, &ev);

                switch (ev.type) {
                case Expose:
                        LOG_D("XEvent: processing 'Expose'");
                        if (ev.xexpose.count == 0 && win->visible) {
                                draw();
                        }
                        break;
                case ButtonRelease:
                        LOG_D("XEvent: processing 'ButtonRelease'");
                        if (ev.xbutton.window == win->xwin) {
                                x_handle_click(ev);
                                wake_up();
                        }
                        break;
                case KeyPress:
                        LOG_D("XEvent: processing 'KeyPress'");
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
                case CreateNotify:
                        LOG_D("XEvent: processing 'CreateNotify'");
                        if (win->visible &&
                            ev.xcreatewindow.override_redirect == 0)
                                XRaiseWindow(xctx.dpy, win->xwin);
                        break;
                case PropertyNotify:
                        if (ev.xproperty.atom == XA_RESOURCE_MANAGER) {
                                LOG_D("XEvent: processing PropertyNotify for Resource manager");
                                XRM_update_db();
                                screen_dpi_xft_cache_purge();

                                if (win->visible) {
                                        draw();
                                }
                                break;
                        }
                        /* Explicitly fallthrough. Other PropertyNotify events, e.g. catching
                         * _NET_WM get handled in the Focus(In|Out) section */
                case ConfigureNotify:
                case FocusIn:
                case FocusOut:
                        LOG_D("XEvent: Checking for active screen changes");
                        fullscreen_now = have_fullscreen_window();
                        scr = get_active_screen();

                        if (fullscreen_now != fullscreen_last) {
                                fullscreen_last = fullscreen_now;
                                wake_up();
                        } else if (   settings.f_mode != FOLLOW_NONE
                        /* Ignore PropertyNotify, when we're still on the
                         * same screen. PropertyNotify is only necessary
                         * to detect a focus change to another screen
                         */
                                   && win->visible
                                   && scr->id != win->cur_screen) {
                                draw();
                                win->cur_screen = scr->id;
                        }
                        break;
                default:
                        if (!screen_check_event(&ev)) {
                                LOG_D("XEvent: Ignoring '%d'", ev.type);
                        }

                        break;
                }
        }
        return G_SOURCE_CONTINUE;
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

/*
 * Convert x button code to linux event code
 * Returns 0 if button is not recognized.
 */
static unsigned int x_mouse_button_to_linux_event_code(unsigned int x_button)
{
        switch (x_button) {
                case Button1:
                        return BTN_LEFT;
                case Button2:
                        return BTN_MIDDLE;
                case Button3:
                        return BTN_RIGHT;
                default:
                        LOG_W("Unsupported mouse button: '%d'", x_button);
                        return 0;
        }
}

/* TODO move to x_mainloop_* */
/*
 * Handle incoming mouse click events
 */
static void x_handle_click(XEvent ev)
{
        unsigned int linux_code = x_mouse_button_to_linux_event_code(ev.xbutton.button);

        if (linux_code == 0) {
                return;
        }

        bool button_state;
        if(ev.type == ButtonRelease) {
                button_state = false; // button is up
        } else {
                // this shouldn't happen, because this function
                // is only called when it'a a ButtonRelease event
                button_state = true; // button is down
        }

        float scale = x_get_scale();
        input_handle_click(linux_code, button_state, ev.xbutton.x/scale,
                        ev.xbutton.y/scale);
}

void x_free(void)
{
        if (xctx.screensaver_info)
                XFree(xctx.screensaver_info);

        if (xctx.dpy)
                XCloseDisplay(xctx.dpy);
}

static int XErrorHandlerDB(Display *display, XErrorEvent *e)
{
        char err_buf[BUFSIZ];
        XGetErrorText(display, e->error_code, err_buf, BUFSIZ);
        LOG_W("%s", err_buf);
        return 0;
}

static void XRM_update_db(void)
{
        XrmDatabase db;
        XTextProperty prop;
        Window root;
        // We shouldn't destroy the first DB coming
        // from the display object itself
        static bool runonce = false;

        XFlush(xctx.dpy);
        XSetErrorHandler(XErrorHandlerDB);

        root = RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));

        XLockDisplay(xctx.dpy);
        if (XGetTextProperty(xctx.dpy, root, &prop, XA_RESOURCE_MANAGER)) {
                if (runonce) {
                        db = XrmGetDatabase(xctx.dpy);
                        XrmDestroyDatabase(db);
                }

                db = XrmGetStringDatabase((const char*)prop.value);
                XrmSetDatabase(xctx.dpy, db);
        }
        XUnlockDisplay(xctx.dpy);

        runonce = true;

        XFlush(xctx.dpy);
        XSync(xctx.dpy, false);
        XSetErrorHandler(NULL);
}

/*
 * Setup X11 stuff
 */
bool x_setup(void)
{

        /* initialize xctx.dc, font, keyboard, colors */
        if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
                LOG_W("No locale support");

        if (!(xctx.dpy = XOpenDisplay(NULL))) {
                LOG_W("Cannot open X11 display.");
                return false;
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

        xctx.screensaver_info = XScreenSaverAllocInfo();

        XrmInitialize();
        XRM_update_db();

        init_screens();
        x_shortcut_grab(&settings.history_ks);
        return true;
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

GSource* x_win_reg_source(struct window_x11 *win)
{
        // Static is necessary here because glib keeps the pointer and we need
        // to keep the reference alive.
        static GSourceFuncs xsrc_fn = {
                x_mainloop_fd_prepare,
                x_mainloop_fd_check,
                x_mainloop_fd_dispatch,
                NULL,
                NULL,
                NULL
        };

        struct x11_source *xsrc = (struct x11_source*) g_source_new(&xsrc_fn,
                                                        sizeof(struct x11_source));

        xsrc->win = win;

        g_source_add_unix_fd((GSource*) xsrc, xctx.dpy->fd, G_IO_IN | G_IO_HUP | G_IO_ERR);

        g_source_attach((GSource*) xsrc, NULL);

        return (GSource*)xsrc;
}

/*
 * Setup the window
 */
window x_win_create(void)
{
        struct window_x11 *win = g_malloc0(sizeof(struct window_x11));

        Window root;
        int scr_n;
        int depth;
        Visual * vis;
        XVisualInfo vi;
        XSetWindowAttributes wa;

        scr_n = DefaultScreen(xctx.dpy);
        root = RootWindow(xctx.dpy, scr_n);
        if (XMatchVisualInfo(xctx.dpy, scr_n, 32, TrueColor, &vi)) {
                vis  = vi.visual;
                depth = vi.depth;
        } else {
                vis = DefaultVisual(xctx.dpy, scr_n);
                depth = DefaultDepth(xctx.dpy, scr_n);
        }

        wa.override_redirect = true;
        wa.background_pixmap = None;
        wa.background_pixel = 0;
        wa.border_pixel = 0;
        wa.colormap = XCreateColormap(xctx.dpy, root, vis, AllocNone);
        wa.event_mask =
            ExposureMask | KeyPressMask | VisibilityChangeMask |
            ButtonReleaseMask | FocusChangeMask| StructureNotifyMask;

        const struct screen_info *scr = get_active_screen();
        win->xwin = XCreateWindow(xctx.dpy,
                                 root,
                                 scr->x,
                                 scr->y,
                                 scr->w,
                                 1,
                                 0,
                                 depth,
                                 CopyFromParent,
                                 vis,
                                 CWOverrideRedirect | CWBackPixmap | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
                                 &wa);

        x_set_wm(win->xwin);
        settings.transparency =
            settings.transparency > 100 ? 100 : settings.transparency;
        setopacity(win->xwin,
                   (unsigned long)((100 - settings.transparency) *
                                   (0xffffffff / 100)));

        win->root_surface = cairo_xlib_surface_create(xctx.dpy, win->xwin,
                                                      vis,
                                                      WIDTH, HEIGHT);
        win->c_ctx = cairo_create(win->root_surface);

        win->esrc = x_win_reg_source(win);

        /* SubstructureNotifyMask is required for receiving CreateNotify events
         * in order to raise the window when something covers us. See #160
         *
         * PropertyChangeMask is requred for getting screen change events when follow_mode != none
         *                    and it's also needed to receive
         *                    XA_RESOURCE_MANAGER events to update the dpi when
         *                    the xresource value is updated
         */
        long root_event_mask = SubstructureNotifyMask | PropertyChangeMask;
        if (settings.f_mode != FOLLOW_NONE) {
                root_event_mask |= FocusChangeMask;
        }
        XSelectInput(xctx.dpy, root, root_event_mask);

        return (window)win;
}

void x_win_destroy(window winptr)
{
        struct window_x11 *win = (struct window_x11*)winptr;

        g_source_destroy(win->esrc);
        g_source_unref(win->esrc);

        cairo_destroy(win->c_ctx);
        cairo_surface_destroy(win->root_surface);
        XDestroyWindow(xctx.dpy, win->xwin);

        g_free(win);
}

/*
 * Show the window and grab shortcuts.
 */
void x_win_show(window winptr)
{
        struct window_x11 *win = (struct window_x11*)winptr;

        /* window is already mapped or there's nothing to show */
        if (win->visible)
                return;


        x_shortcut_grab(&settings.close_ks);
        x_shortcut_grab(&settings.close_all_ks);
        x_shortcut_grab(&settings.context_ks);

        x_shortcut_setup_error_handler();
        XGrabButton(xctx.dpy,
                    AnyButton,
                    AnyModifier,
                    win->xwin,
                    false,
                    (ButtonPressMask|ButtonReleaseMask),
                    GrabModeAsync,
                    GrabModeSync,
                    None,
                    None);
        if (x_shortcut_tear_down_error_handler()) {
                LOG_W("Unable to grab mouse button(s).");
        }

        XMapRaised(xctx.dpy, win->xwin);
        win->visible = true;

        x_display_surface(win->root_surface, win, &win->dim);
}

/*
 * Hide the window and ungrab unused keyboard_shortcuts
 */
void x_win_hide(window winptr)
{
        LOG_I("X11: Hiding window");
        struct window_x11 *win = (struct window_x11*)winptr;
        ASSERT_OR_RET(win->visible,);

        x_shortcut_ungrab(&settings.close_ks);
        x_shortcut_ungrab(&settings.close_all_ks);
        x_shortcut_ungrab(&settings.context_ks);

        XUngrabButton(xctx.dpy, AnyButton, AnyModifier, win->xwin);
        XUnmapWindow(xctx.dpy, win->xwin);
        XFlush(xctx.dpy);
        win->visible = false;
}

/*
 * Parse a string into a modifier mask.
 */
KeySym x_shortcut_string_to_mask(const char *str)
{
        if (STR_EQ(str, "ctrl")) {
                return ControlMask;
        } else if (STR_EQ(str, "mod4")) {
                return Mod4Mask;
        } else if (STR_EQ(str, "mod3")) {
                return Mod3Mask;
        } else if (STR_EQ(str, "mod2")) {
                return Mod2Mask;
        } else if (STR_EQ(str, "mod1")) {
                return Mod1Mask;
        } else if (STR_EQ(str, "shift")) {
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
static int x_shortcut_grab(struct keyboard_shortcut *ks)
{
        ASSERT_OR_RET(ks->is_valid, 1);
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
static void x_shortcut_ungrab(struct keyboard_shortcut *ks)
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
static void x_shortcut_init(struct keyboard_shortcut *ks)
{
        ASSERT_OR_RET(ks && ks->str,);

        if (STR_EQ(ks->str, "none") || (STR_EQ(ks->str, ""))) {
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

double x_get_scale(void) {
        if (settings.scale > 0)
                return settings.scale;

        const struct screen_info *scr_info = get_active_screen();
        double scale = MAX(1, scr_info->dpi/96.);
        LOG_D("X11 dpi: %i", scr_info->dpi);
        LOG_D("X11 scale: %f", scale);
        return scale;
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

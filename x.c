/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
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
#include <pango/pangocairo.h>
#include <cairo-xlib.h>

#include "x.h"
#include "utils.h"
#include "dunst.h"
#include "settings.h"
#include "notification.h"

#define WIDTH 400
#define HEIGHT 400

xctx_t xctx;
bool dunst_grab_errored = false;

typedef struct _cairo_ctx {
        cairo_status_t status;
        cairo_surface_t *surface;
        cairo_t *context;
        PangoFontDescription *desc;
} cairo_ctx_t;

typedef struct _colored_layout {
        PangoLayout *l;
        color_t fg;
        color_t bg;
        char *text;
        PangoAttrList *attr;
} colored_layout;

cairo_ctx_t cairo_ctx;

static color_t frame_color;

static void x_shortcut_setup_error_handler(void);
static int x_shortcut_tear_down_error_handler(void);
static void x_win_move(int width, int height);


static color_t x_color_hex_to_double(int hexValue)
{
        color_t color;
        color.r = ((hexValue >> 16) & 0xFF) / 255.0;
        color.g = ((hexValue >> 8) & 0xFF) / 255.0;
        color.b = ((hexValue) & 0xFF) / 255.0;

  return color;
}

static color_t x_string_to_color_t(const char *str)
{
        char *end;
        long int val = strtol(str+1, &end, 16);
        if (*end != '\0' && *(end+1) != '\0') {
                printf("WARNING: Invalid color string: \"%s\"\n", str);
        }

        return x_color_hex_to_double(val);
}

color_t calculate_foreground_color(color_t bg)
{
        double c_delta = 0.1;
        color_t color = bg;

        /* do we need to darken or brighten the colors? */
        bool darken = (bg.r + bg.g + bg.b) / 3 > 0.5;

        if (darken) {
                if (color.r - c_delta < 0)
                        color.r = 0;
                else
                        color.r -= c_delta;
                if (color.g - c_delta < 0)
                        color.g = 0;
                else
                        color.g -= c_delta;
                if (color.b - c_delta < 0)
                        color.b = 0;
                else
                        color.b -= c_delta;
        } else {
                if (color.r + c_delta > 1)
                        color.r = 1;
                else
                        color.r += c_delta;
                if (color.g + c_delta > 1)
                        color.g = 1;
                else
                        color.g += c_delta;
                if (color.b + c_delta > 1)
                        color.g = 1;
                else
                        color.g += c_delta;
        }

        return color;
}


static color_t x_get_separator_color(color_t fg, color_t bg)
{
        switch (settings.sep_color) {
                case FRAME:
                        return x_string_to_color_t(settings.frame_color);
                case CUSTOM:
                        return x_string_to_color_t(settings.sep_custom_color_str);
                case FOREGROUND:
                        return fg;
                case AUTO:
                        return calculate_foreground_color(bg);
                default:
                        printf("Unknown separator color type. Please file a Bugreport.\n");
                        return fg;

        }
}

void x_cairo_setup(void)
{
        cairo_ctx.surface = cairo_xlib_surface_create(xctx.dpy,
                        xctx.win, DefaultVisual(xctx.dpy, 0), WIDTH, HEIGHT);

        cairo_ctx.context = cairo_create(cairo_ctx.surface);

        cairo_ctx.desc = pango_font_description_from_string(settings.font);

        frame_color = x_string_to_color_t(settings.frame_color);
}

void r_setup_pango_layout(PangoLayout *layout, int width)
{
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_width(layout, width * PANGO_SCALE);
        pango_layout_set_font_description(layout, cairo_ctx.desc);
        pango_layout_set_spacing(layout, settings.line_height * PANGO_SCALE);

        PangoAlignment align;
        switch (settings.align) {
                case left:
                default:
                        align = PANGO_ALIGN_LEFT;
                        break;
                case center:
                        align = PANGO_ALIGN_CENTER;
                        break;
                case right:
                        align = PANGO_ALIGN_RIGHT;
                        break;
        }
        pango_layout_set_alignment(layout, align);

}

static void free_colored_layout(void *data)
{
        colored_layout *cl = data;
        g_object_unref(cl->l);
        g_free(cl->text);
}

static bool have_dynamic_width(void)
{
        return (xctx.geometry.mask & WidthValue && xctx.geometry.w == 0);
}

static dimension_t calculate_dimensions(GSList *layouts)
{
        dimension_t dim;
        dim.w = 0;
        dim.h = 0;
        dim.x = 0;
        dim.y = 0;
        dim.mask = xctx.geometry.mask;

        screen_info scr;
        x_screen_info(&scr);
        if (have_dynamic_width()) {
                /* dynamic width */
                dim.w = 0;
        } else if (xctx.geometry.mask & WidthValue) {
                /* fixed width */
                if (xctx.geometry.negative_width) {
                        dim.w = scr.dim.w - xctx.geometry.w;
                } else {
                        dim.w = xctx.geometry.w;
                }
        } else {
                /* across the screen */
                dim.w = scr.dim.w;
        }

        dim.h += (g_slist_length(layouts) - 1) * settings.separator_height;
        dim.h += g_slist_length(layouts) * settings.padding * 2;

        int text_width = 0;
        for (GSList *iter = layouts; iter; iter = iter->next) {
                colored_layout *cl = iter->data;
                int w,h;
                pango_layout_get_pixel_size(cl->l, &w, &h);
                dim.h += h;
                text_width = MAX(w, text_width);
        }

        if (dim.w <= 0) {
                dim.w = text_width + 2 * settings.h_padding;
                dim.w += 2 * settings.frame_width;
        }

        return dim;
}

static colored_layout *r_init_shared(cairo_t *c, notification *n)
{
        colored_layout *cl = malloc(sizeof(colored_layout));
        cl->l = pango_cairo_create_layout(c);

        if (!settings.word_wrap) {
                pango_layout_set_ellipsize(cl->l, PANGO_ELLIPSIZE_MIDDLE);
        }


        cl->fg = x_string_to_color_t(n->color_strings[ColFG]);
        cl->bg = x_string_to_color_t(n->color_strings[ColBG]);

        dimension_t dim = calculate_dimensions(NULL);
        int width = dim.w;

        if (have_dynamic_width()) {
                r_setup_pango_layout(cl->l, -1);
        } else {
                width -= 2 * settings.h_padding;
                width -= 2 * settings.frame_width;
                r_setup_pango_layout(cl->l, width);
        }

        return cl;
}

static colored_layout *r_create_layout_for_xmore(cairo_t *c, notification *n, int qlen)
{
       colored_layout *cl = r_init_shared(c, n);
       cl->text = g_strdup_printf("(%d more)", qlen);
       cl->attr = NULL;
       pango_layout_set_text(cl->l, cl->text, -1);
       return cl;
}

colored_layout *r_create_layout_from_notification(cairo_t *c, notification *n)
{

        colored_layout *cl = r_init_shared(c, n);

        /* markup */
        GError *err = NULL;
        pango_parse_markup(n->text_to_render, -1, 0, &(cl->attr), &(cl->text), NULL, &err);

        if (!err) {
                pango_layout_set_text(cl->l, cl->text, -1);
                pango_layout_set_attributes(cl->l, cl->attr);
        } else {
                /* remove markup and display plain message instead */
                n->text_to_render = notification_fix_markup(n->text_to_render);
                cl->text = NULL;
                cl->attr = NULL;
                pango_layout_set_text(cl->l, n->text_to_render, -1);
                printf("Error parsing markup: %s\n", err->message);
                g_error_free(err);
        }


        pango_layout_get_pixel_size(cl->l, NULL, &(n->displayed_height));
        n->displayed_height += 2 * settings.padding;

        return cl;
}

GSList *r_create_layouts(cairo_t *c)
{
        GSList *layouts = NULL;

        int qlen = g_list_length(g_queue_peek_head_link(queue));
        bool xmore_is_needed = qlen > 0 && settings.indicate_hidden;

        notification *last = NULL;
        for (GList *iter = g_queue_peek_head_link(displayed);
                        iter; iter = iter->next)
        {
                notification *n = iter->data;
                last = n;

                notification_update_text_to_render(n);

                if (!iter->next && xmore_is_needed && xctx.geometry.h == 1) {
                        char *new_ttr = g_strdup_printf("%s (%d more)", n->text_to_render, qlen);
                        g_free(n->text_to_render);
                        n->text_to_render = new_ttr;
                }
                layouts = g_slist_append(layouts,
                                r_create_layout_from_notification(c, n));
        }

                if (xmore_is_needed && xctx.geometry.h != 1) {
                        /* append xmore message as new message */
                        layouts = g_slist_append(layouts,
                                r_create_layout_for_xmore(c, last, qlen));
        }

        return layouts;
}

void r_free_layouts(GSList *layouts)
{
        g_slist_free_full(layouts, free_colored_layout);
}

static dimension_t x_render_layout(cairo_t *c, colored_layout *cl, dimension_t dim, bool first, bool last)
{
        int h;
        pango_layout_get_pixel_size(cl->l, NULL, &h);

        int bg_x = 0;
        int bg_y = dim.y;
        int bg_width = dim.w;
        int bg_height = (2 * settings.padding) + h;

        /* adding frame */
        bg_x += settings.frame_width;
        if (first) {
                bg_y += settings.frame_width;
                bg_height -= settings.frame_width;
        }
        bg_width -= 2 * settings.frame_width;
        if (last)
                bg_height -= settings.frame_width;

        cairo_set_source_rgb(c, cl->bg.r, cl->bg.g, cl->bg.b);
        cairo_rectangle(c, bg_x, bg_y, bg_width, bg_height);
        cairo_fill(c);

        dim.y += settings.padding;
        cairo_move_to(c, settings.h_padding, dim.y);
        cairo_set_source_rgb(c, cl->fg.r, cl->fg.g, cl->fg.b);
        pango_cairo_update_layout(c, cl->l);
        pango_cairo_show_layout(c, cl->l);
        dim.y += h + settings.padding;
        color_t sep_color = x_get_separator_color(cl->fg, cl->bg);
        if (settings.separator_height > 0 && !last) {
                cairo_set_source_rgb(c, sep_color.r, sep_color.g, sep_color.b);

                cairo_rectangle(c, settings.frame_width, dim.y,
                                dim.w - 2 * settings.frame_width
                                , settings.separator_height);

                cairo_fill(c);
                dim.y += settings.separator_height;
        }
        cairo_move_to(c, settings.h_padding, dim.y);

        return dim;
}

void x_win_draw(void)
{

        GSList *layouts = r_create_layouts(cairo_ctx.context);

        dimension_t dim = calculate_dimensions(layouts);
        int width = dim.w;
        int height = dim.h;

        cairo_t *c;
        cairo_surface_t *image_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
        c = cairo_create(image_surface);

        x_win_move(width, height);
        cairo_xlib_surface_set_size(cairo_ctx.surface, width, height);

        cairo_set_source_rgb(c, frame_color.r, frame_color.g, frame_color.b);
        cairo_rectangle(c, 0.0, 0.0, width, height);
        cairo_fill(c);

        cairo_move_to(c, 0, 0);

        bool first = true;
        for (GSList *iter = layouts; iter; iter = iter->next) {
                colored_layout *cl = iter->data;
                dim = x_render_layout(c, cl, dim, first, iter->next == NULL);
                first = false;
        }

        cairo_set_source_surface(cairo_ctx.context, image_surface, 0, 0);
        cairo_paint(cairo_ctx.context);
        cairo_show_page(cairo_ctx.context);

        cairo_destroy(c);
        cairo_surface_destroy(image_surface);
        r_free_layouts(layouts);

}

static void x_win_move(int width, int height)
{

        int x, y;
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


void setopacity(Window win, unsigned long opacity)
{
        Atom _NET_WM_WINDOW_OPACITY =
            XInternAtom(xctx.dpy, "_NET_WM_WINDOW_OPACITY", false);
        XChangeProperty(xctx.dpy, win, _NET_WM_WINDOW_OPACITY, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *)&opacity, 1L);
}








        /*
         * Helper function to use glib's mainloop mechanic
         * with Xlib
         */
gboolean x_mainloop_fd_prepare(GSource * source, gint * timeout)
{
        *timeout = -1;
        return false;
}

        /*
         * Helper function to use glib's mainloop mechanic
         * with Xlib
         */
gboolean x_mainloop_fd_check(GSource * source)
{
        return XPending(xctx.dpy) > 0;
}

        /*
         * Main Dispatcher for XEvents
         */
gboolean x_mainloop_fd_dispatch(GSource * source, GSourceFunc callback,
                                gpointer user_data)
{
        XEvent ev;
        while (XPending(xctx.dpy) > 0) {
                XNextEvent(xctx.dpy, &ev);
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
                                XRaiseWindow(xctx.dpy, xctx.win);
                        break;
                case ButtonPress:
                        if (ev.xbutton.window == xctx.win) {
                                x_handle_click(ev);
                        }
                        break;
                case KeyPress:
                        if (settings.close_ks.str
                            && XLookupKeysym(&ev.xkey,
                                             0) == settings.close_ks.sym
                            && settings.close_ks.mask == ev.xkey.state) {
                                if (displayed) {
                                        notification_close
                                            (g_queue_peek_head_link(displayed)->
                                             data, 2);
                                }
                        }
                        if (settings.history_ks.str
                            && XLookupKeysym(&ev.xkey,
                                             0) == settings.history_ks.sym
                            && settings.history_ks.mask == ev.xkey.state) {
                                history_pop();
                        }
                        if (settings.close_all_ks.str
                            && XLookupKeysym(&ev.xkey,
                                             0) == settings.close_all_ks.sym
                            && settings.close_all_ks.mask == ev.xkey.state) {
                                move_all_to_history();
                        }
                        if (settings.context_ks.str
                            && XLookupKeysym(&ev.xkey,
                                             0) == settings.context_ks.sym
                            && settings.context_ks.mask == ev.xkey.state) {
                                context_menu();
                        }
                        break;
                case FocusIn:
                case FocusOut:
                case PropertyNotify:
                        wake_up();
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
                int first = true;
                for (GList * iter = g_queue_peek_head_link(displayed); iter;
                     iter = iter->next) {
                        n = iter->data;
                        if (ev.xbutton.y > y && ev.xbutton.y < y + n->displayed_height)
                                break;

                        y += n->displayed_height + settings.separator_height;
                        if (first)
                                y += settings.frame_width;
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

#ifdef XINERAMA
        /*
         * Select the screen on which the Window
         * should be displayed.
         */
int select_screen(XineramaScreenInfo * info, int info_len)
{
        if (settings.f_mode == FOLLOW_NONE) {
                return settings.monitor >=
                    0 ? settings.monitor : XDefaultScreen(xctx.dpy);

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
                                return settings.monitor >=
                                    0 ? settings.monitor : XDefaultScreen(xctx.dpy);
                        }

                        Window child_return;
                        XTranslateCoordinates(xctx.dpy, focused, root,
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
                return settings.monitor >=
                    0 ? settings.monitor : XDefaultScreen(xctx.dpy);
        }
}
#endif

        /*
         * Update the information about the monitor
         * geometry.
         */
void x_screen_info(screen_info * scr)
{
#ifdef XINERAMA
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
}

        /*
         * Setup X11 stuff
         */
void x_setup(void)
{

        /* initialize xctx.dc, font, keyboard, colors */
        if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
                fputs("no locale support\n", stderr);
        if (!(xctx.dpy = XOpenDisplay(NULL))) {
                die("cannot open display\n", EXIT_FAILURE);
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

        xctx.color_strings[ColFG][LOW] = settings.lowfgcolor;
        xctx.color_strings[ColFG][NORM] = settings.normfgcolor;
        xctx.color_strings[ColFG][CRIT] = settings.critfgcolor;

        xctx.color_strings[ColBG][LOW] = settings.lowbgcolor;
        xctx.color_strings[ColBG][NORM] = settings.normbgcolor;
        xctx.color_strings[ColBG][CRIT] = settings.critbgcolor;

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
        x_cairo_setup();
        x_shortcut_grab(&settings.history_ks);
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

        root = RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));
        xctx.utf8 = XInternAtom(xctx.dpy, "UTF8_STRING", false);

        wa.override_redirect = true;
        wa.background_pixmap = ParentRelative;
        wa.event_mask =
            ExposureMask | KeyPressMask | VisibilityChangeMask |
            ButtonPressMask | FocusChangeMask| StructureNotifyMask;

        screen_info scr;
        x_screen_info(&scr);
        xctx.win =
            XCreateWindow(xctx.dpy, root, scr.dim.x, scr.dim.y, scr.dim.w,
                          1, 0, DefaultDepth(xctx.dpy,
                                                       DefaultScreen(xctx.dpy)),
                          CopyFromParent, DefaultVisual(xctx.dpy,
                                                        DefaultScreen(xctx.dpy)),
                          CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
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
        if (xctx.visible || g_queue_is_empty(displayed)) {
                return;
        }

        x_shortcut_grab(&settings.close_ks);
        x_shortcut_grab(&settings.close_all_ks);
        x_shortcut_grab(&settings.context_ks);

        x_shortcut_setup_error_handler();
        XGrabButton(xctx.dpy, AnyButton, AnyModifier, xctx.win, false,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        if (x_shortcut_tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab mouse button(s)\n");
        }

        XMapRaised(xctx.dpy, xctx.win);
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
int x_shortcut_grab(keyboard_shortcut * ks)
{
        if (!ks->is_valid)
                return 1;
        Window root;
        root = RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));

        x_shortcut_setup_error_handler();

        if (ks->is_valid)
                XGrabKey(xctx.dpy, ks->code, ks->mask, root,
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
void x_shortcut_ungrab(keyboard_shortcut * ks)
{
        Window root;
        root = RootWindow(xctx.dpy, DefaultScreen(xctx.dpy));
        if (ks->is_valid)
                XUngrabKey(xctx.dpy, ks->code, ks->mask, root);
}

        /*
         * Initialize the keyboard shortcut.
         */
void x_shortcut_init(keyboard_shortcut * ks)
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
                fprintf(stderr, "Warning: Unknown keyboard shortcut: %s\n",
                        ks->str);
                ks->is_valid = false;
        } else {
                ks->is_valid = true;
        }

        free(str_begin);
}

/* vim: set ts=8 sw=8 tw=0: */

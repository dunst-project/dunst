/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

// {{{ INCLUDES
#define _GNU_SOURCE
#define XLIB_ILLEGAL_ACCESS

#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/time.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <glib.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/extensions/scrnsaver.h>

#include "dunst.h"
#include "x.h"
#include "dbus.h"
#include "utils.h"
#include "rules.h"
#include "notification.h"

#include "option_parser.h"
#include "settings.h"

// }}}

// {{{ DEFINES
#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define FONT_HEIGHT_BORDER 2

#define DEFFONT "Monospace-11"

#ifndef VERSION
#define VERSION "version info needed"
#endif

#define MSG 1
#define INFO 2
#define DEBUG 3
//}}}

// {{{ STRUCTS
typedef struct _x11_source {
        GSource source;
        Display *dpy;
        Window w;
} x11_source_t;
// }}}


// {{{ GLOBALS
int height_limit;
int font_h;

/* index of colors fit to urgency level */
const char *color_strings[2][3];
Atom utf8;
DC *dc;
Window win;
bool visible = false;
dimension_t geometry;
XScreenSaverInfo *screensaver_info;
dimension_t window_dim;
bool pause_display = false;
unsigned long framec;
unsigned long sep_custom_col;

GMainLoop *mainloop = NULL;
bool timer_active = false;

bool dunst_grab_errored = false;
bool force_redraw = false;


/* notification lists */
GQueue *queue = NULL;             /* all new notifications get into here */
GQueue *displayed = NULL;   /* currently displayed notifications */
GQueue *history = NULL;      /* history of displayed notifications */
GSList *rules = NULL;
// }}}

// {{{ FUNCTION DEFINITIONS


/* window */
void x_win_draw(void);
void x_win_hide(void);
void x_win_show(void);
void x_win_setup(void);

/* shortcut */
void x_shortcut_init(keyboard_shortcut *shortcut);
void x_shortcut_ungrab(keyboard_shortcut *ks);
int x_shortcut_grab(keyboard_shortcut *ks);
KeySym x_shortcut_string_to_mask(const char *str);
static void x_shortcut_setup_error_handler(void);
static int x_shortcut_tear_down_error_handler(void);

/* X misc */
void x_handle_click(XEvent ev);
void x_screen_info(screen_info *scr);
bool x_is_idle(void);
void x_setup(void);

/* X mainloop */
static gboolean x_mainloop_fd_prepare(GSource *source, gint *timeout);
static gboolean x_mainloop_fd_check(GSource *source);
static gboolean x_mainloop_fd_dispatch(GSource *source, GSourceFunc callback, gpointer user_data);

/* misc funtions */
void check_timeouts(void);
void history_pop(void);
void usage(int exit_status);
void move_all_to_history(void);
void print_version(void);
char *extract_urls(const char *str);
void context_menu(void);
void wake_up(void);
void pause_signal_handler(int sig);

// }}}










// {{{ X
// {{{ X_MAINLOOP

        /*
         * Helper function to use glib's mainloop mechanic
         * with Xlib
         */
static gboolean x_mainloop_fd_prepare(GSource *source, gint *timeout)
{ // {{{
        *timeout = -1;
        return false;
}
// }}}


        /*
         * Helper function to use glib's mainloop mechanic
         * with Xlib
         */
static gboolean x_mainloop_fd_check(GSource *source)
{ // {{{
        return XPending(dc->dpy) > 0;
}
// }}}


        /*
         * Main Dispatcher for XEvents
         */
static gboolean x_mainloop_fd_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{ // {{{
        XEvent ev;
        while (XPending(dc->dpy) > 0) {
                XNextEvent(dc->dpy, &ev);
                switch (ev.type) {
                case Expose:
                        if (ev.xexpose.count == 0 && visible) {
                        }
                        break;
                case SelectionNotify:
                        if (ev.xselection.property == utf8)
                                break;
                case VisibilityNotify:
                        if (ev.xvisibility.state != VisibilityUnobscured)
                                XRaiseWindow(dc->dpy, win);
                        break;
                case ButtonPress:
                        if (ev.xbutton.window == win) {
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
// }}}

// }}}

// {{{ X_MISC

        /*
         * Check whether the user is currently idle.
         */
bool x_is_idle(void)
{ // {{{
        XScreenSaverQueryInfo(dc->dpy, DefaultRootWindow(dc->dpy),
                              screensaver_info);
        if (settings.idle_threshold == 0) {
                return false;
        }
        return screensaver_info->idle / 1000 > settings.idle_threshold;
}
// }}}

/* TODO move to x_mainloop_* */
        /*
         * Handle incoming mouse click events
         */
void x_handle_click(XEvent ev)
{ // {{{
        if (ev.xbutton.button == Button3) {
                move_all_to_history();

                return;
        }

        if (ev.xbutton.button == Button1) {
                int y = settings.separator_height;
                notification *n = NULL;
                for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                        n = iter->data;
                        int text_h = MAX(font_h, settings.line_height) * n->line_count;
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
// }}}



        /*
         * Return the window that currently has
         * the keyboard focus.
         */
Window get_focused_window(void)
{ // {{{
        Window focused = 0;
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *prop_return = NULL;
        Window root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));
        Atom netactivewindow =
            XInternAtom(dc->dpy, "_NET_ACTIVE_WINDOW", false);

        XGetWindowProperty(dc->dpy, root, netactivewindow, 0L,
                           sizeof(Window), false, XA_WINDOW,
                           &type, &format, &nitems, &bytes_after, &prop_return);
        if (prop_return) {
                focused = *(Window *) prop_return;
                XFree(prop_return);
        }

        return focused;
}
// }}}

#ifdef XINERAMA
        /*
         * Select the screen on which the Window
         * should be displayed.
         */
int select_screen(XineramaScreenInfo * info, int info_len)
{ // {{{
        if (settings.f_mode == FOLLOW_NONE) {
                return settings.monitor >= 0 ? settings.monitor : XDefaultScreen(dc->dpy);

        } else {
                int x, y;
                assert(settings.f_mode == FOLLOW_MOUSE || settings.f_mode == FOLLOW_KEYBOARD);
                Window root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));

                if (settings.f_mode == FOLLOW_MOUSE) {
                        int dummy;
                        unsigned int dummy_ui;
                        Window dummy_win;

                        XQueryPointer(dc->dpy, root, &dummy_win,
                                      &dummy_win, &x, &y, &dummy,
                                      &dummy, &dummy_ui);
                }

                if (settings.f_mode == FOLLOW_KEYBOARD) {

                        Window focused = get_focused_window();

                        if (focused == 0) {
                                /* something went wrong. Fallback to default */
                                return settings.monitor >= 0 ? settings.monitor : XDefaultScreen(dc->dpy);
                        }

                        Window child_return;
                        XTranslateCoordinates(dc->dpy, focused, root,
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
                return settings.monitor >= 0 ? settings.monitor : XDefaultScreen(dc->dpy);
        }
}
// }}}
#endif

        /*
         * Update the information about the monitor
         * geometry.
         */
void x_screen_info(screen_info *scr)
{ // {{{
#ifdef XINERAMA
        int n;
        XineramaScreenInfo *info;
        if ((info = XineramaQueryScreens(dc->dpy, &n))) {
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
                        screen = DefaultScreen(dc->dpy);

                scr->dim.w = DisplayWidth(dc->dpy, screen);
                scr->dim.h = DisplayHeight(dc->dpy, screen);
        }
}
// }}}


        /*
         * Setup X11 stuff
         */
void x_setup(void)
{ // {{{

        /* initialize dc, font, keyboard, colors */
        dc = initdc();

        initfont(dc, settings.font);

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

        color_strings[ColFG][LOW] = settings.lowfgcolor;
        color_strings[ColFG][NORM] = settings.normfgcolor;
        color_strings[ColFG][CRIT] = settings.critfgcolor;

        color_strings[ColBG][LOW] = settings.lowbgcolor;
        color_strings[ColBG][NORM] = settings.normbgcolor;
        color_strings[ColBG][CRIT] = settings.critbgcolor;

        framec = getcolor(dc, settings.frame_color);

        if (settings.sep_color == CUSTOM) {
                sep_custom_col = getcolor(dc, settings.sep_custom_color_str);
        } else {
                sep_custom_col = 0;
        }

        /* parse and set geometry and monitor position */
        if (settings.geom[0] == '-') {
                geometry.negative_width = true;
                settings.geom++;
        } else {
                geometry.negative_width = false;
        }

        geometry.mask = XParseGeometry(settings.geom,
                                       &geometry.x, &geometry.y,
                                       &geometry.w, &geometry.h);


        screensaver_info = XScreenSaverAllocInfo();


        x_win_setup();
        x_shortcut_grab(&settings.history_ks);
}
// }}}


// }}}

/* TODO comments and naming */
// {{{ X_RENDER

GSList *do_word_wrap(char *text, int max_width)
{ // {{{

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

                if (settings.word_wrap && max_width > 0 && textnw(dc, begin, (end - begin) + 1) > max_width) {
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
// }}}


char *generate_final_text(notification *n)
{ // {{{
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
// }}}

int calculate_x_offset(int line_width, int text_width)
{ // {{{
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
// }}}

unsigned long calculate_foreground_color(unsigned long source_color)
{ // {{{
        Colormap cmap = DefaultColormap(dc->dpy, DefaultScreen(dc->dpy));
        XColor color;

        color.pixel = source_color;
        XQueryColor(dc->dpy, cmap, &color);

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
        XAllocColor(dc->dpy, cmap, &color);
        return color.pixel;
}
// }}}

int calculate_width(void)
{ // {{{
        screen_info scr;
        x_screen_info(&scr);
        if (geometry.mask & WidthValue && geometry.w == 0) {
                /* dynamic width */
                return 0;
        } else if (geometry.mask & WidthValue) {
                /* fixed width */
                if (geometry.negative_width) {
                        return scr.dim.w - geometry.w;
                } else {
                        return geometry.w;
                }
        } else {
                /* across the screen */
                return scr.dim.w;
        }
}
// }}}

void move_and_map(int width, int height)
{ // {{{

        int x,y;
        screen_info scr;
        x_screen_info(&scr);
        /* calculate window position */
        if (geometry.mask & XNegative) {
                x = (scr.dim.x + (scr.dim.w - width)) + geometry.x;
        } else {
                x = scr.dim.x + geometry.x;
        }

        if (geometry.mask & YNegative) {
                y = scr.dim.y + (scr.dim.h + geometry.y) - height;
        } else {
                y = scr.dim.y + geometry.y;
        }

        /* move and map window */
        if (x != window_dim.x || y != window_dim.y
            || width != window_dim.w || height != window_dim.h) {

                XResizeWindow(dc->dpy, win, width, height);
                XMoveWindow(dc->dpy, win, x, y);

                window_dim.x = x;
                window_dim.y = y;
                window_dim.h = height;
                window_dim.w = width;
        }

        mapdc(dc, win, width, height);

}
// }}}

GSList *generate_render_texts(int width)
{ // {{{
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
                if (geometry.h != 1) {
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
// }}}

void free_render_text(void *data) {
        g_slist_free_full(((render_text *) data)->lines, g_free);
}

void free_render_texts(GSList *texts) {
        g_slist_free_full(texts, free_render_text);
}


// }}}


// {{{ X_WIN

void x_win_draw(void)
{ // {{{

        int outer_width = calculate_width();
        screen_info scr;
        x_screen_info(&scr);


        settings.line_height = MAX(settings.line_height, font_h);

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
                                width = MAX(width, textw(dc, iiter->data));
                }
                outer_width = width + (2 * settings.frame_width) + (2 * settings.h_padding);
        }

        /* resize dc to correct width */

        int height = (line_count * settings.line_height)
                   + displayed->length * 2 * settings.padding
                   + ((settings.indicate_hidden && queue->length > 0 && geometry.h != 1) ? 2 * settings.padding : 0)
                   + (settings.separator_height * (displayed->length - 1))
                   + (2 * settings.frame_width);

        resizedc(dc, outer_width, height);

        /* draw frame
         * this draws a big box in the frame color which get filled with
         * smaller boxes of the notification colors
         */
        dc->y = 0;
        dc->x = 0;
        if (settings.frame_width > 0) {
                drawrect(dc, 0, 0, outer_width, height, true, framec);
        }

        dc->y = settings.frame_width;
        dc->x = settings.frame_width;

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

                        dc->x = settings.frame_width;

                        /* draw background */
                        drawrect(dc, 0, 0, width + (2*settings.h_padding), pad +  settings.line_height, true, colors->BG);

                        /* draw text */
                        dc->x = calculate_x_offset(width, textw(dc, line));

                        dc->y += ((settings.line_height - font_h) / 2);
                        dc->y += first_line ? settings.padding : 0;

                        drawtextn(dc, line, strlen(line), colors);

                        dc->y += settings.line_height - ((settings.line_height - font_h) / 2);
                        dc->y += last_line ? settings.padding : 0;

                        first_line = false;
                }

                /* draw separator */
                if (settings.separator_height > 0 && iter->next) {
                        dc->x = settings.frame_width;
                        double color;
                        if (settings.sep_color == AUTO)
                                color = calculate_foreground_color(colors->BG);
                        else if (settings.sep_color == FOREGROUND)
                                color = colors->FG;
                        else if (settings.sep_color == FRAME)
                                color = framec;
                        else {
                                /* CUSTOM */
                                color = sep_custom_col;
                        }
                        drawrect(dc, 0, 0, width + (2*settings.h_padding), settings.separator_height, true, color);
                        dc->y += settings.separator_height;
                }
        }

        move_and_map(outer_width, height);

        free_render_texts(texts);
}
// }}}

        /*
         * Setup the window
         */
void x_win_setup(void)
{ // {{{

        Window root;
        XSetWindowAttributes wa;

        window_dim.x = 0;
        window_dim.y = 0;
        window_dim.w = 0;
        window_dim.h = 0;

        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));
        utf8 = XInternAtom(dc->dpy, "UTF8_STRING", false);
        font_h = dc->font.height + FONT_HEIGHT_BORDER;

        wa.override_redirect = true;
        wa.background_pixmap = ParentRelative;
        wa.event_mask =
            ExposureMask | KeyPressMask | VisibilityChangeMask |
            ButtonPressMask;

        screen_info scr;
        x_screen_info(&scr);
        win =
            XCreateWindow(dc->dpy, root, scr.dim.x, scr.dim.y, scr.dim.w,
                          font_h, 0, DefaultDepth(dc->dpy,
                                                  DefaultScreen(dc->dpy)),
                          CopyFromParent, DefaultVisual(dc->dpy,
                                                        DefaultScreen(dc->dpy)),
                          CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        settings.transparency = settings.transparency > 100 ? 100 : settings.transparency;
        setopacity(dc, win,
                   (unsigned long)((100 - settings.transparency) * (0xffffffff / 100)));
}
// }}}

        /*
         * Show the window and grab shortcuts.
         */
void x_win_show(void)
{ // {{{
        /* window is already mapped or there's nothing to show */
        if (visible || g_queue_is_empty(displayed)) {
                return;
        }

        x_shortcut_grab(&settings.close_ks);
        x_shortcut_grab(&settings.close_all_ks);
        x_shortcut_grab(&settings.context_ks);

        x_shortcut_setup_error_handler();
        XGrabButton(dc->dpy, AnyButton, AnyModifier, win, false,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        if (x_shortcut_tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab mouse button(s)\n");
        }

        XMapRaised(dc->dpy, win);
        visible = true;
}
// }}}

        /*
         * Hide the window and ungrab unused keyboard_shortcuts
         */
void x_win_hide()
{ // {{{
        x_shortcut_ungrab(&settings.close_ks);
        x_shortcut_ungrab(&settings.close_all_ks);
        x_shortcut_ungrab(&settings.context_ks);

        XUngrabButton(dc->dpy, AnyButton, AnyModifier, win);
        XUnmapWindow(dc->dpy, win);
        XFlush(dc->dpy);
        visible = false;
}
// }}}

// }}}


// {{{ X_SHORTCUT

        /*
         * Parse a string into a modifier mask.
         */
KeySym x_shortcut_string_to_mask(const char *str)
{ // {{{
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
// }}}

        /*
         * Error handler for grabbing mouse and keyboard errors.
         */
static int GrabXErrorHandler(Display * display, XErrorEvent * e)
{ // {{{
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
// }}}

        /*
         * Setup the Error handler.
         */
static void x_shortcut_setup_error_handler(void)
{ // {{{
        dunst_grab_errored = false;

        XFlush(dc->dpy);
        XSetErrorHandler(GrabXErrorHandler);
}
// }}}

        /*
         * Tear down the Error handler.
         */
static int x_shortcut_tear_down_error_handler(void)
{ // {{{
        XFlush(dc->dpy);
        XSync(dc->dpy, false);
        XSetErrorHandler(NULL);
        return dunst_grab_errored;
}
// }}}

        /*
         * Grab the given keyboard shortcut.
         */
int x_shortcut_grab(keyboard_shortcut *ks)
{ // {{{
        if (!ks->is_valid)
                return 1;
        Window root;
        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));

        x_shortcut_setup_error_handler();

        if (ks->is_valid)
                XGrabKey(dc->dpy, ks->code, ks->mask, root,
                         true, GrabModeAsync, GrabModeAsync);

        if (x_shortcut_tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab key \"%s\"\n", ks->str);
                ks->is_valid = false;
                return 1;
        }
        return 0;
}
// }}}

        /*
         * Ungrab the given keyboard shortcut.
         */
void x_shortcut_ungrab(keyboard_shortcut *ks)
{ // {{{
        Window root;
        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));
        if (ks->is_valid)
                XUngrabKey(dc->dpy, ks->code, ks->mask, root);
}
// }}}

        /*
         * Initialize the keyboard shortcut.
         */
void x_shortcut_init(keyboard_shortcut *ks)
{ // {{{
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
        XDisplayKeycodes(dc->dpy, &min_keysym, &max_keysym);

        ks->code = NoSymbol;

        for (int i = min_keysym; i <= max_keysym; i++) {
                if (XkbKeycodeToKeysym(dc->dpy, i, 0, 0) == ks->sym
                    || XkbKeycodeToKeysym(dc->dpy, i, 0, 1) == ks->sym) {
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
// }}}

// }}}
// }}}


// {{{ RUN
void check_timeouts(void)
{ // {{{
        /* nothing to do */
        if (displayed->length == 0)
                return;

        for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                notification *n = iter->data;

                /* don't timeout when user is idle */
                if (x_is_idle()) {
                        n->start = time(NULL);
                        continue;
                }

                /* skip hidden and sticky messages */
                if (n->start == 0 || n->timeout == 0) {
                        continue;
                }

                /* remove old message */
                if (difftime(time(NULL), n->start) > n->timeout) {
                        force_redraw = true;
                        /* close_notification may conflict with iter, so restart */
                        notification_close(n, 1);
                        check_timeouts();
                        return;
                }
        }
}
// }}}

void update_lists()
{ // {{{
        int limit;

        check_timeouts();

        if (pause_display) {
                while (displayed->length > 0) {
                        g_queue_insert_sorted(queue, g_queue_pop_head(queue), notification_cmp_data, NULL);
                }
                return;
        }

        if (geometry.h == 0) {
                limit = 0;
        } else if (geometry.h == 1) {
                limit = 1;
        } else if (settings.indicate_hidden) {
                limit = geometry.h - 1;
        } else {
                limit = geometry.h;
        }


        /* move notifications from queue to displayed */
        while (queue->length > 0) {

                if (limit > 0 && displayed->length >= limit) {
                        /* the list is full */
                        break;
                }

                force_redraw = true;

                notification *n = g_queue_pop_head(queue);

                if (!n)
                        return;
                n->start = time(NULL);
                if (!n->redisplayed && n->script) {
                        notification_run_script(n);
                }

                g_queue_insert_sorted(displayed, n, notification_cmp_data, NULL);
        }
}
// }}}


void move_all_to_history()
{ // {{{
        while (displayed->length > 0) {
                notification_close(g_queue_peek_head_link(displayed)->data, 2);
        }

        notification *n = g_queue_pop_head(queue);
        while (n) {
                g_queue_push_tail(history, n);
                n = g_queue_pop_head(queue);
        }
}
// }}}

void history_pop(void)
{ // {{{

        if (g_queue_is_empty(history))
                return;

        notification *n = g_queue_pop_tail(history);
        n->redisplayed = true;
        n->start = 0;
        n->timeout = settings.sticky_history ? 0 : n->timeout;
        g_queue_push_head(queue, n);

        if (!visible) {
                wake_up();
        }
}
// }}}

void update(void)
{ // {{{
        time_t last_time = time(&last_time);
        static time_t last_redraw = 0;

        /* move messages from notification_queue to displayed_notifications */
        update_lists();
        if (displayed->length > 0 && ! visible) {
                x_win_show();
        }
        if (displayed->length == 0 && visible) {
                x_win_hide();
        }

        if (visible && (force_redraw || time(NULL) - last_redraw > 0)) {
                x_win_draw();
                force_redraw = false;
                last_redraw = time(NULL);
        }
}
// }}}

void wake_up(void)
{ // {{{
        force_redraw = true;
        update();
        if (!timer_active) {
                timer_active = true;
                g_timeout_add(1000, run, mainloop);
        }
}
// }}}

gboolean run(void *data)
{ // {{{

        update();

        if (visible && !timer_active) {
                g_timeout_add(200, run, mainloop);
                timer_active = true;
        }

        if (!visible && timer_active) {
                timer_active = false;
                /* returning false disables timeout */
                return false;
        }

        return true;
}
// }}}

//}}}




// {{{ MAIN
int main(int argc, char *argv[])
{ // {{{

        history = g_queue_new();
        displayed = g_queue_new();
        queue = g_queue_new();


        cmdline_load(argc, argv);

        if (cmdline_get_bool("-v/-version", false, "Print version")
            || cmdline_get_bool("--version", false, "Print version")) {
                print_version();
        }

        char *cmdline_config_path;
        cmdline_config_path =
            cmdline_get_string("-conf/-config", NULL,
                               "Path to configuration file");
        load_settings(cmdline_config_path);

        if (cmdline_get_bool("-h/-help", false, "Print help")
            || cmdline_get_bool("--help", false, "Print help")) {
                usage(EXIT_SUCCESS);
        }

        int owner_id = initdbus();

        x_setup();

        signal (SIGUSR1, pause_signal_handler);
        signal (SIGUSR2, pause_signal_handler);

        if (settings.startup_notification) {
                notification *n = malloc(sizeof (notification));
                n->appname = "dunst";
                n->summary = "startup";
                n->body = "dunst is up and running";
                n->progress = 0;
                n->timeout = 10;
                n->urgency = LOW;
                n->icon = NULL;
                n->msg = NULL;
                n->dbus_client = NULL;
                n->color_strings[0] = NULL;
                n->color_strings[1] = NULL;
                n->actions = NULL;
                n->urls = NULL;
                notification_init(n, 0);
        }

        mainloop = g_main_loop_new(NULL, FALSE);

        GPollFD dpy_pollfd = {dc->dpy->fd,
                G_IO_IN | G_IO_HUP | G_IO_ERR, 0 };

        GSourceFuncs x11_source_funcs = {
                x_mainloop_fd_prepare,
                x_mainloop_fd_check,
                x_mainloop_fd_dispatch,
                NULL,
                NULL,
                NULL };

        GSource *x11_source =
                g_source_new(&x11_source_funcs, sizeof(x11_source_t));
                ((x11_source_t*)x11_source)->dpy = dc->dpy;
                ((x11_source_t*)x11_source)->w = win;
                g_source_add_poll(x11_source, &dpy_pollfd);

      g_source_attach(x11_source, NULL);

      run(NULL);
      g_main_loop_run(mainloop);

      dbus_tear_down(owner_id);

        return 0;
}
// }}}

void pause_signal_handler(int sig)
{ // {{{
        if (sig == SIGUSR1) {
                pause_display = true;
        }
        if (sig == SIGUSR2) {
                pause_display = false;
        }

        signal (sig, pause_signal_handler);
}
// }}}

void usage(int exit_status)
{ // {{{
        fputs("usage:\n", stderr);
        char *us = cmdline_create_usage();
        fputs(us, stderr);
        fputs("\n", stderr);
        exit(exit_status);
}
// }}}

void print_version(void)
{ // {{{
        printf("Dunst - A customizable and lightweight notification-daemon %s\n",
               VERSION);
        exit(EXIT_SUCCESS);
}
// }}}
// }}}

/* vim: set ts=8 sw=8 tw=0: */

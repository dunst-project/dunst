/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

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
#include <regex.h>
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
#include <basedir.h>
#include <basedir_fs.h>

#include "dunst.h"
#include "draw.h"
#include "dbus.h"
#include "utils.h"

#include "options.h"

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

typedef struct _x11_source {
        GSource source;
        Display *dpy;
        Window w;
} x11_source_t;

/* global variables */

#include "config.h"

int height_limit;

/* index of colors fit to urgency level */
static ColorSet *colors[3];
static const char *color_strings[2][3];
static Atom utf8;
static DC *dc;
static Window win;
static bool visible = false;
static screen_info scr;
static dimension_t geometry;
static XScreenSaverInfo *screensaver_info;
static int font_h;
static bool print_notifications = false;
static dimension_t window_dim;
static bool pause_display = false;
static char **dmenu_cmd;
static unsigned long framec;
static unsigned long sep_custom_col;

GMainLoop *mainloop = NULL;
bool timer_active = false;

bool dunst_grab_errored = false;
bool force_redraw = false;

int next_notification_id = 1;

/* notification lists */
GQueue *queue = NULL;             /* all new notifications get into here */
GQueue *displayed = NULL;   /* currently displayed notifications */
GQueue *history = NULL;      /* history of displayed notifications */
GSList *rules = NULL;

/* rules */
void rule_apply(rule_t *r, notification *n);
void rule_apply_all(notification *n);
bool rule_matches_notification(rule_t *r, notification *n);
void rule_init(rule_t *r);

/* misc funtions */
int cmp_notification(const void *a, const void *b);
int cmp_notification_data(const void *va, const void *vb, void *data);
void check_timeouts(void);
char *fix_markup(char *str);
void handle_mouse_click(XEvent ev);
void history_pop(void);
bool is_idle(void);
void setup(void);
void update_screen_info();
void usage(int exit_status);
void draw_win(void);
void hide_win(void);
void move_all_to_history(void);
void print_version(void);
char *extract_urls(const char *str);
void context_menu(void);
void run_script(notification *n);
void wake_up(void);

void init_shortcut(keyboard_shortcut * shortcut);
KeySym string_to_mask(char *str);

/*******
 *
 * MainLoop functions for xlib
 */

static gboolean x11_fd_prepare(GSource *source, gint *timeout)
{
        *timeout = -1;
        return false;
}

static gboolean x11_fd_check(GSource *source)
{
        return XPending(dc->dpy) > 0;
}

static gboolean
x11_fd_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
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
                                handle_mouse_click(ev);
                        }
                        break;
                case KeyPress:
                        if (close_ks.str
                            && XLookupKeysym(&ev.xkey, 0) == close_ks.sym
                            && close_ks.mask == ev.xkey.state) {
                                if (displayed) {
                                        close_notification(g_queue_peek_head_link(displayed)->data, 2);
                                }
                        }
                        if (history_ks.str
                            && XLookupKeysym(&ev.xkey, 0) == history_ks.sym
                            && history_ks.mask == ev.xkey.state) {
                                history_pop();
                        }
                        if (close_all_ks.str
                            && XLookupKeysym(&ev.xkey, 0) == close_all_ks.sym
                            && close_all_ks.mask == ev.xkey.state) {
                                move_all_to_history();
                        }
                        if (context_ks.str
                            && XLookupKeysym(&ev.xkey, 0) == context_ks.sym
                            && context_ks.mask == ev.xkey.state) {
                                context_menu();
                        }
                        break;
                }
        }
        return true;
}

int cmp_notification(const void *va, const void *vb)
{
        notification *a = (notification*) va;
        notification *b = (notification*) vb;

        if (!sort)
                return 1;

        if (a->urgency != b->urgency) {
                return b->urgency - a->urgency;
        } else {
                return a->timestamp - b->timestamp;
        }
}

int cmp_notification_data(const void *va, const void *vb, void *data)
{
        return cmp_notification(va, vb);
}


char *extract_urls( const char * to_match)
{
    static bool is_initialized = false;
    static regex_t cregex;

    if (!is_initialized) {
        char *regex = "((http|ftp|https)(://))?(www\\.)?[[:alnum:]_-]+\\.[^[:space:]]+";
        int ret = regcomp(&cregex, regex, REG_EXTENDED|REG_ICASE);
        if (ret != 0) {
            printf("failed to compile regex\n");
            return NULL;
        } else {
            is_initialized = true;
        }
    }

    char *urls = NULL;

    const char * p = to_match;
    regmatch_t m;

    while (1) {
        int nomatch = regexec (&cregex, p, 1, &m, 0);
        if (nomatch) {
                return urls;
        }
        int start;
        int finish;
        if (m.rm_so == -1) {
            break;
        }
        start = m.rm_so + (p - to_match);
        finish = m.rm_eo + (p - to_match);

        char *match = strndup(to_match+start, finish-start);

        urls = string_append(urls, match, "\n");

        p += m.rm_eo;
    }
    return urls;
}


void open_browser(const char *url)
{
    int browser_pid1 = fork();

    if (browser_pid1) {
            int status;
            waitpid(browser_pid1, &status, 0);
    } else {
            int browser_pid2 = fork();
            if (browser_pid2) {
                    exit(0);
            } else {
                    browser = string_append(browser, url, " ");
                    char **cmd = g_strsplit(browser, " ", 0);
                    execvp(cmd[0], cmd);
            }
    }
}

void invoke_action(const char *action)
{
        notification *invoked = NULL;
        char *action_identifier = NULL;

        char *name_begin = strstr(action, "(");
        if (!name_begin) {
                printf("invalid action: %s\n", action);
                return;
        }
        name_begin++;


        for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                notification *n = iter->data;
                if (g_str_has_prefix(action, n->appname)) {
                        if (! n->actions)
                                continue;

                        for (int i = 0; i < n->actions->count; i += 2) {
                                char *a_identifier = n->actions->actions[i];
                                char *name = n->actions->actions[i+1];
                                if (g_str_has_prefix(name_begin, name)) {
                                        invoked = n;
                                        action_identifier = a_identifier;
                                        break;
                                }
                        }
                }
        }

        if (invoked && action_identifier) {
                actionInvoked(invoked, action_identifier);
        }
}

void dispatch_menu_result(const char *input)
{
        char *maybe_url = extract_urls(input);
        if (maybe_url) {
                open_browser(maybe_url);
                free(maybe_url);
                return;
        }

        invoke_action(input);
}

void context_menu(void) {
        char *dmenu_input = NULL;

        for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                notification *n = iter->data;
                dmenu_input = string_append(dmenu_input, n->urls, "\n");
                if (n->actions)
                        dmenu_input = string_append(dmenu_input, n->actions->dmenu_str, "\n");
        }


        if (!dmenu_input)
                return;

        char buf[1024];
        int child_io[2];
        int parent_io[2];
        if (pipe(child_io) != 0) {
                PERR("pipe()", errno);
                return;
        }
        if (pipe(parent_io) != 0) {
                PERR("pipe()", errno);
                return;
        }
        int pid = fork();

    if (pid == 0) {
        close(child_io[1]);
        close(parent_io[0]);
        close(0);
        if (dup(child_io[0]) == -1) {
                PERR("dup()", errno);
                exit(EXIT_FAILURE);
        }
        close(1);
        if (dup(parent_io[1]) == -1) {
                PERR("dup()", errno);
                exit(EXIT_FAILURE);
        }
        execvp(dmenu_cmd[0], dmenu_cmd);
    } else {
        close(child_io[0]);
        close(parent_io[1]);
        size_t wlen = strlen(dmenu_input);
        if (write(child_io[1], dmenu_input, wlen) != wlen) {
                PERR("write()", errno);
        }
        close(child_io[1]);

        size_t len = read(parent_io[0], buf, 1023);
        if (len == 0)
            return;
        buf[len - 1] = '\0';

        int status;
        waitpid(pid, &status, 0);
    }

    close(parent_io[0]);


    dispatch_menu_result(buf);
}


void run_script(notification *n)
{
        if (!n->script || strlen(n->script) < 1)
                return;

        char *appname = n->appname ? n->appname : "";
        char *summary = n->summary ? n->summary : "";
        char *body = n->body ? n->body : "";
        char *icon = n->icon ? n->icon : "";

        char *urgency;
        switch (n->urgency) {
                case LOW:
                        urgency = "LOW";
                        break;
                case NORM:
                        urgency = "NORMAL";
                        break;
                case CRIT:
                        urgency = "CRITICAL";
                        break;
                default:
                        urgency = "NORMAL";
                        break;
        }

        int pid1 = fork();

        if (pid1) {
                int status;
                waitpid(pid1, &status, 0);
        } else {
                int pid2 = fork();
                if (pid2) {
                        exit(0);
                } else {
                        int ret = execlp(n->script, n->script,
                                        appname,
                                        summary,
                                        body,
                                        icon,
                                        urgency,
                                        (char *) NULL
                                        );
                        if (ret != 0) {
                                PERR("Unable to run script", errno);
                                exit(EXIT_FAILURE);
                        }
                }
        }
}

void pause_signal_handler(int sig)
{
        if (sig == SIGUSR1) {
                pause_display = true;
        }
        if (sig == SIGUSR2) {
                pause_display = false;
        }

        signal (sig, pause_signal_handler);
}

static void print_notification(notification * n)
{
        printf("{\n");
        printf("\tappname: '%s'\n", n->appname);
        printf("\tsummary: '%s'\n", n->summary);
        printf("\tbody: '%s'\n", n->body);
        printf("\ticon: '%s'\n", n->icon);
        printf("\turgency: %d\n", n->urgency);
        printf("\tformatted: '%s'\n", n->msg);
        printf("\tid: %d\n", n->id);
        if (n->urls) {
                printf("\turls\n");
                printf("\t{\n");
                printf("%s\n", n->urls);
                printf("\t}\n");
        }

        if (n->actions) {
                printf("\tactions:\n");
                printf("\t{\n");
                for (int i = 0; i < n->actions->count; i += 2) {
                        printf("\t\t [%s,%s]\n", n->actions->actions[i], n->actions->actions[i+1]);
                }
                printf("actions_dmenu: %s\n", n->actions->dmenu_str);
                printf("\t]\n");
        }
        printf("\tscript: %s\n", n->script);
        printf("}\n");
}

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

static void setup_error_handler(void)
{
        dunst_grab_errored = false;

        XFlush(dc->dpy);
        XSetErrorHandler(GrabXErrorHandler);
}

static int tear_down_error_handler(void)
{
        XFlush(dc->dpy);
        XSync(dc->dpy, false);
        XSetErrorHandler(NULL);
        return dunst_grab_errored;
}

int grab_key(keyboard_shortcut * ks)
{
        if (!ks->is_valid)
                return 1;
        Window root;
        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));

        setup_error_handler();

        if (ks->is_valid)
                XGrabKey(dc->dpy, ks->code, ks->mask, root,
                         true, GrabModeAsync, GrabModeAsync);

        if (tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab key \"%s\"\n", ks->str);
                ks->is_valid = false;
                return 1;
        }
        return 0;
}

void ungrab_key(keyboard_shortcut * ks)
{
        Window root;
        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));
        if (ks->is_valid)
                XUngrabKey(dc->dpy, ks->code, ks->mask, root);
}


void rule_apply(rule_t *r, notification *n)
{
        if (r->timeout != -1)
                n->timeout = r->timeout;
        if (r->urgency != -1)
                n->urgency = r->urgency;
        if (r->fg)
                n->color_strings[ColFG] = r->fg;
        if (r->bg)
                n->color_strings[ColBG] = r->bg;
        if (r->format)
                n->format = r->format;
        if (r->script)
                n->script = r->script;
}

void rule_apply_all(notification *n)
{
        for (GSList *iter = rules; iter; iter = iter->next) {
                rule_t *r = iter->data;
                if (rule_matches_notification(r, n)) {
                        rule_apply(r, n);
                }
        }
}

void rule_init(rule_t *r)
{
        r->name = NULL;
        r->appname = NULL;
        r->summary = NULL;
        r->body = NULL;
        r->icon = NULL;
        r->timeout = -1;
        r->urgency = -1;
        r->fg = NULL;
        r->bg = NULL;
        r->format = NULL;
}

bool rule_matches_notification(rule_t *r, notification *n)
{

                return ((!r->appname || !fnmatch(r->appname, n->appname, 0))
                    && (!r->summary || !fnmatch(r->summary, n->summary, 0))
                    && (!r->body || !fnmatch(r->body, n->body, 0))
                    && (!r->icon || !fnmatch(r->icon, n->icon, 0)));
}

void check_timeouts(void)
{
        /* nothing to do */
        if (displayed->length == 0)
                return;

        for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                notification *n = iter->data;

                /* don't timeout when user is idle */
                if (is_idle()) {
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
                        close_notification(n, 1);
                        check_timeouts();
                        return;
                }
        }
}

void update_lists()
{
        int limit;

        check_timeouts();

        if (pause_display) {
                while (displayed->length > 0) {
                        g_queue_insert_sorted(queue, g_queue_pop_head(queue), cmp_notification_data, NULL);
                }
                return;
        }

        if (geometry.h == 0) {
                limit = 0;
        } else if (geometry.h == 1) {
                limit = 1;
        } else if (indicate_hidden) {
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
                        run_script(n);
                }

                g_queue_insert_sorted(displayed, n, cmp_notification_data, NULL);
        }
}


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

                if (word_wrap && max_width > 0 && textnw(dc, begin, (end - begin) + 1) > max_width) {
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

        if (show_age_threshold >= 0 && t_delta >= show_age_threshold) {
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
        if (line_width < text_width && bounce_freq > 0.0001 && !word_wrap) {
                gettimeofday(&t, NULL);
                pos =
                    ((t.tv_sec % 100) * 1e6 + t.tv_usec) / (1e6 / bounce_freq);
                return (1 + sinf(2 * 3.14159 * pos)) * leftover / 2;
        }
        switch (align) {
        case left:
                return frame_width + h_padding;
        case center:
                return frame_width + h_padding + (leftover / 2);
        case right:
                return frame_width + h_padding + leftover;
        default:
                /* this can't happen */
                return 0;
        }
}

unsigned long calculate_foreground_color(unsigned long source_color)
{
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

int calculate_width(void)
{
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

void move_and_map(int width, int height)
{

        int x,y;
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
        if (indicate_hidden && queue->length > 0) {
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

void free_render_text(void *data) {
        g_slist_free_full(((render_text *) data)->lines, g_free);
}

void free_render_texts(GSList *texts) {
        g_slist_free_full(texts, free_render_text);
}


void draw_win(void)
{

        update_screen_info();
        int outer_width = calculate_width();


        line_height = MAX(line_height, font_h);

        int width;
        if (outer_width == 0)
                width = 0;
        else
                width = outer_width - (2 * frame_width) - (2 * h_padding);


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
                outer_width = width + (2 * frame_width) + (2 * h_padding);
        }

        /* resize dc to correct width */

        int height = (line_count * line_height)
                   + displayed->length * 2 * padding
                   + ((indicate_hidden && queue->length > 0 && geometry.h != 1) ? 2 * padding : 0)
                   + (separator_height * (displayed->length - 1))
                   + (2 * frame_width);

        resizedc(dc, outer_width, height);

        /* draw frame
         * this draws a big box in the frame color which get filled with
         * smaller boxes of the notification colors
         */
        dc->y = 0;
        dc->x = 0;
        if (frame_width > 0) {
                drawrect(dc, 0, 0, outer_width, height, true, framec);
        }

        dc->y = frame_width;
        dc->x = frame_width;

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
                                pad = 2*padding;
                        else if (first_line || last_line)
                                pad = padding;

                        dc->x = frame_width;

                        /* draw background */
                        drawrect(dc, 0, 0, width + (2*h_padding), pad +  line_height, true, colors->BG);

                        /* draw text */
                        dc->x = calculate_x_offset(width, textw(dc, line));

                        dc->y += ((line_height - font_h) / 2);
                        dc->y += first_line ? padding : 0;

                        drawtextn(dc, line, strlen(line), colors);

                        dc->y += line_height - ((line_height - font_h) / 2);
                        dc->y += last_line ? padding : 0;

                        first_line = false;
                }

                /* draw separator */
                if (separator_height > 0 && iter->next) {
                        dc->x = frame_width;
                        double color;
                        if (sep_color == AUTO)
                                color = calculate_foreground_color(colors->BG);
                        else if (sep_color == FOREGROUND)
                                color = colors->FG;
                        else if (sep_color == FRAME)
                                color = framec;
                        else {
                                /* CUSTOM */
                                color = sep_custom_col;
                        }
                        drawrect(dc, 0, 0, width + (2*h_padding), separator_height, true, color);
                        dc->y += separator_height;
                }
        }

        move_and_map(outer_width, height);

        free_render_texts(texts);
}

char
*fix_markup(char *str)
{
        char *replace_buf, *start, *end;

        if (str == NULL) {
                return NULL;
        }

        str = string_replace_all("&quot;", "\"", str);
        str = string_replace_all("&apos;", "'", str);
        str = string_replace_all("&amp;", "&", str);
        str = string_replace_all("&lt;", "<", str);
        str = string_replace_all("&gt;", ">", str);

        /* remove tags */
        str = string_replace_all("<b>", "", str);
        str = string_replace_all("</b>", "", str);
        str = string_replace_all("<br>", " ", str);
        str = string_replace_all("<br/>", " ", str);
        str = string_replace_all("<br />", " ", str);
        str = string_replace_all("<i>", "", str);
        str = string_replace_all("</i>", "", str);
        str = string_replace_all("<u>", "", str);
        str = string_replace_all("</u>", "", str);
        str = string_replace_all("</a>", "", str);

        start = strstr(str, "<a href");
        if (start != NULL) {
                end = strstr(str, ">");
                if (end != NULL) {
                        replace_buf = strndup(start, end - start + 1);
                        str = string_replace(replace_buf, "", str);
                        free(replace_buf);
                }
        }
        start = strstr(str, "<img src");
        if (start != NULL) {
                end = strstr(str, "/>");
                if (end != NULL) {
                        replace_buf = strndup(start, end - start + 2);
                        str = string_replace(replace_buf, "", str);
                        free(replace_buf);
                }
        }
        return str;

}

void handle_mouse_click(XEvent ev)
{
        if (ev.xbutton.button == Button3) {
                move_all_to_history();

                return;
        }

        if (ev.xbutton.button == Button1) {
                int y = separator_height;
                notification *n = NULL;
                for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                        n = iter->data;
                        int text_h = MAX(font_h, line_height) * n->line_count;
                        int padding = 2 * h_padding;

                        int height = text_h + padding;

                        if (ev.xbutton.y > y && ev.xbutton.y < y + height)
                                break;
                        else
                                y += height + separator_height;
                }
                if (n)
                        close_notification(n, 2);
        }
}


void move_all_to_history()
{
        while (displayed->length > 0) {
                close_notification(g_queue_peek_head_link(displayed)->data, 2);
        }

        notification *n = g_queue_pop_head(queue);
        while (n) {
                g_queue_push_tail(history, n);
                n = g_queue_pop_head(queue);
        }
}

void history_pop(void)
{

        if (g_queue_is_empty(history))
                return;

        notification *n = g_queue_pop_tail(history);
        n->redisplayed = true;
        n->start = 0;
        n->timeout = sticky_history ? 0 : n->timeout;
        g_queue_push_head(queue, n);

        if (!visible) {
                wake_up();
        }
}

void free_notification(notification * n)
{
        if (n == NULL)
                return;
        free(n->appname);
        free(n->summary);
        free(n->body);
        free(n->icon);
        free(n->msg);
        free(n->dbus_client);
        free(n);
}

int init_notification(notification * n, int id)
{
        const char *fg = NULL;
        const char *bg = NULL;

        if (n == NULL)
                return -1;

        if (strcmp("DUNST_COMMAND_PAUSE", n->summary) == 0) {
                pause_display = true;
                return 0;
        }

        if (strcmp("DUNST_COMMAND_RESUME", n->summary) == 0) {
                pause_display = false;
                return 0;
        }

        n->script = NULL;

        n->format = format;

        rule_apply_all(n);

        n->msg = string_replace("%a", n->appname, g_strdup(n->format));
        n->msg = string_replace("%s", n->summary, n->msg);
        if (n->icon) {
                n->msg = string_replace("%I", basename(n->icon), n->msg);
                n->msg = string_replace("%i", n->icon, n->msg);
        }
        n->msg = string_replace("%b", n->body, n->msg);
        if (n->progress) {
                char pg[10];
                sprintf(pg, "[%3d%%]", n->progress - 1);
                n->msg = string_replace("%p", pg, n->msg);
        } else {
                n->msg = string_replace("%p", "", n->msg);
        }

        n->msg = fix_markup(n->msg);

        while (strstr(n->msg, "\\n") != NULL)
                n->msg = string_replace("\\n", "\n", n->msg);

        if (ignore_newline)
                while (strstr(n->msg, "\n") != NULL)
                        n->msg = string_replace("\n", " ", n->msg);

        n->msg = g_strstrip(n->msg);


        n->dup_count = 0;

        /* check if n is a duplicate */
        for (GList *iter = g_queue_peek_head_link(queue); iter; iter = iter->next) {
                notification *orig = iter->data;
                if (strcmp(orig->appname, n->appname) == 0
                    && strcmp(orig->msg, n->msg) == 0) {
                        orig->dup_count++;
                        free_notification(n);
                        wake_up();
                        return orig->id;
                }
        }

        for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                notification *orig = iter->data;
                if (strcmp(orig->appname, n->appname) == 0
                    && strcmp(orig->msg, n->msg) == 0) {
                        orig->dup_count++;
                        orig->start = time(NULL);
                        free_notification(n);
                        wake_up();
                        return orig->id;
                }
        }

        /* urgency > CRIT -> array out of range */
        n->urgency = n->urgency > CRIT ? CRIT : n->urgency;

        if (n->color_strings[ColFG]) {
                fg = n->color_strings[ColFG];
        } else {
                fg = color_strings[ColFG][n->urgency];
        }

        if (n->color_strings[ColBG]) {
                bg = n->color_strings[ColBG];
        } else {
                bg = color_strings[ColBG][n->urgency];
        }

        n->colors = initcolor(dc, fg, bg);

        n->timeout = n->timeout == -1 ? timeouts[n->urgency] : n->timeout;
        n->start = 0;

        n->timestamp = time(NULL);

        n->redisplayed = false;

        if (id == 0) {
                n->id = ++next_notification_id;
        } else {
                close_notification_by_id(id, -1);
                n->id = id;
        }

        if (strlen(n->msg) == 0) {
                close_notification(n, 2);
                printf("skipping notification: %s %s\n", n->body, n->summary);
        } else {
                g_queue_insert_sorted(queue, n, cmp_notification_data, NULL);
        }

        char *tmp = g_strconcat(n->summary, " ", n->body, NULL);

        n->urls = extract_urls(tmp);


        if (n->actions) {
                n->actions->dmenu_str = NULL;
                for (int i = 0; i < n->actions->count; i += 2) {
                        char *human_readable = n->actions->actions[i+1];
                        printf("debug: %s\n", n->appname);
                        printf("debug: %s\n", human_readable);
                        char *tmp = g_strdup_printf("%s %s", n->appname, human_readable);
                        printf("debug: %s\n", tmp);

                        n->actions->dmenu_str = string_append(n->actions->dmenu_str,
                                        g_strdup_printf("%s(%s)",
                                        n->appname,
                                        human_readable), "\n");
                }
        }

        free(tmp);


        if (print_notifications)
                print_notification(n);

        return n->id;
}

/*
 * reasons:
 * -1 -> notification is a replacement, no NotificationClosed signal emitted
 *  1 -> the notification expired
 *  2 -> the notification was dismissed by the user_data
 *  3 -> The notification was closed by a call to CloseNotification
 */
int close_notification_by_id(int id, int reason)
{
        notification *target = NULL;

        for (GList *iter = g_queue_peek_head_link(displayed); iter; iter = iter->next) {
                notification *n = iter->data;
                if (n->id == id) {
                        g_queue_remove(displayed, n);
                        g_queue_push_tail(history, n);
                        target = n;
                        break;
                }
        }

        for (GList *iter = g_queue_peek_head_link(queue); iter; iter = iter->next) {
                notification *n = iter->data;
                if (n->id == id) {
                        g_queue_remove(queue, n);
                        g_queue_push_tail(history, n);
                        target = n;
                        break;
                }
        }

        if (reason > 0 && reason < 4 && target != NULL) {
                notificationClosed(target, reason);
        }

        wake_up();
        return reason;
}

int close_notification(notification * n, int reason)
{
        if (n == NULL)
                return -1;
        return close_notification_by_id(n->id, reason);
}

KeySym string_to_mask(char *str)
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

void init_shortcut(keyboard_shortcut * ks)
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
                ks->mask = ks->mask | string_to_mask(mod);
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


bool is_idle(void)
{
        XScreenSaverQueryInfo(dc->dpy, DefaultRootWindow(dc->dpy),
                              screensaver_info);
        if (idle_threshold == 0) {
                return false;
        }
        return screensaver_info->idle / 1000 > idle_threshold;
}

void update(void)
{
        time_t last_time = time(&last_time);
        static time_t last_redraw = 0;

        /* move messages from notification_queue to displayed_notifications */
        update_lists();
        if (displayed->length > 0 && ! visible) {
                map_win();
        }
        if (displayed->length == 0 && visible) {
                hide_win();
        }

        if (visible && (force_redraw || time(NULL) - last_redraw > 0)) {
                draw_win();
                force_redraw = false;
                last_redraw = time(NULL);
        }
}

void wake_up(void)
{
        force_redraw = true;
        update();
        if (!timer_active) {
                timer_active = true;
                g_timeout_add(1000, run, mainloop);
        }
}

gboolean run(void *data)
{

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

void hide_win()
{
        ungrab_key(&close_ks);
        ungrab_key(&close_all_ks);
        ungrab_key(&context_ks);

        XUngrabButton(dc->dpy, AnyButton, AnyModifier, win);
        XUnmapWindow(dc->dpy, win);
        XFlush(dc->dpy);
        visible = false;
}

Window get_focused_window(void)
{
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

#ifdef XINERAMA
int select_screen(XineramaScreenInfo * info, int info_len)
{
        if (f_mode == FOLLOW_NONE) {
                return scr.scr;

        } else {
                int x, y;
                assert(f_mode == FOLLOW_MOUSE || f_mode == FOLLOW_KEYBOARD);
                Window root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));

                if (f_mode == FOLLOW_MOUSE) {
                        int dummy;
                        unsigned int dummy_ui;
                        Window dummy_win;

                        XQueryPointer(dc->dpy, root, &dummy_win,
                                      &dummy_win, &x, &y, &dummy,
                                      &dummy, &dummy_ui);
                }

                if (f_mode == FOLLOW_KEYBOARD) {

                        Window focused = get_focused_window();

                        if (focused == 0) {
                                /* something went wrong. Fallback to default */
                                return scr.scr;
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
                return scr.scr;
        }
}
#endif

void update_screen_info()
{
#ifdef XINERAMA
        int n;
        XineramaScreenInfo *info;
        if ((info = XineramaQueryScreens(dc->dpy, &n))) {
                int screen = select_screen(info, n);
                if (screen >= n) {
                        /* invalid monitor, fallback to default */
                        screen = 0;
                }
                scr.dim.x = info[screen].x_org;
                scr.dim.y = info[screen].y_org;
                scr.dim.h = info[screen].height;
                scr.dim.w = info[screen].width;
                XFree(info);
        } else
#endif
        {
                scr.dim.x = 0;
                scr.dim.y = 0;
                scr.dim.w = DisplayWidth(dc->dpy, scr.scr);
                scr.dim.h = DisplayHeight(dc->dpy, scr.scr);
        }
}

void setup(void)
{

        /* initialize dc, font, keyboard, colors */
        dc = initdc();

        initfont(dc, font);

        init_shortcut(&close_ks);
        init_shortcut(&close_all_ks);
        init_shortcut(&history_ks);
        init_shortcut(&context_ks);

        grab_key(&close_ks);
        ungrab_key(&close_ks);
        grab_key(&close_all_ks);
        ungrab_key(&close_all_ks);
        grab_key(&history_ks);
        ungrab_key(&history_ks);
        grab_key(&context_ks);
        ungrab_key(&context_ks);

        colors[LOW] = initcolor(dc, lowfgcolor, lowbgcolor);
        colors[NORM] = initcolor(dc, normfgcolor, normbgcolor);
        colors[CRIT] = initcolor(dc, critfgcolor, critbgcolor);

        color_strings[ColFG][LOW] = lowfgcolor;
        color_strings[ColFG][NORM] = normfgcolor;
        color_strings[ColFG][CRIT] = critfgcolor;

        color_strings[ColBG][LOW] = lowbgcolor;
        color_strings[ColBG][NORM] = normbgcolor;
        color_strings[ColBG][CRIT] = critbgcolor;

        framec = getcolor(dc, frame_color);

        if (sep_color == CUSTOM) {
                sep_custom_col = getcolor(dc, sep_custom_color_str);
        } else {
                sep_custom_col = 0;
        }

        /* parse and set geometry and monitor position */
        if (geom[0] == '-') {
                geometry.negative_width = true;
                geom++;
        } else {
                geometry.negative_width = false;
        }

        geometry.mask = XParseGeometry(geom,
                                       &geometry.x, &geometry.y,
                                       &geometry.w, &geometry.h);

        window_dim.x = 0;
        window_dim.y = 0;
        window_dim.w = 0;
        window_dim.h = 0;

        screensaver_info = XScreenSaverAllocInfo();

        scr.scr = monitor;
        if (scr.scr < 0) {
                scr.scr = DefaultScreen(dc->dpy);
        }

        /* initialize window */
        Window root;
        XSetWindowAttributes wa;

        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));
        utf8 = XInternAtom(dc->dpy, "UTF8_STRING", false);
        font_h = dc->font.height + FONT_HEIGHT_BORDER;
        update_screen_info();

        wa.override_redirect = true;
        wa.background_pixmap = ParentRelative;
        wa.event_mask =
            ExposureMask | KeyPressMask | VisibilityChangeMask |
            ButtonPressMask;
        win =
            XCreateWindow(dc->dpy, root, scr.dim.x, scr.dim.y, scr.dim.w,
                          font_h, 0, DefaultDepth(dc->dpy,
                                                  DefaultScreen(dc->dpy)),
                          CopyFromParent, DefaultVisual(dc->dpy,
                                                        DefaultScreen(dc->dpy)),
                          CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        transparency = transparency > 100 ? 100 : transparency;
        setopacity(dc, win,
                   (unsigned long)((100 - transparency) * (0xffffffff / 100)));
        grab_key(&history_ks);
}

void map_win(void)
{
        /* window is already mapped or there's nothing to show */
        if (visible || g_queue_is_empty(displayed)) {
                return;
        }

        grab_key(&close_ks);
        grab_key(&close_all_ks);
        grab_key(&context_ks);
        setup_error_handler();
        XGrabButton(dc->dpy, AnyButton, AnyModifier, win, false,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        if (tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab mouse button(s)\n");
        }

        update_screen_info();
        XMapRaised(dc->dpy, win);
        visible = true;
}

void parse_follow_mode(const char *mode)
{
        if (strcmp(mode, "mouse") == 0)
                f_mode = FOLLOW_MOUSE;
        else if (strcmp(mode, "keyboard") == 0)
                f_mode = FOLLOW_KEYBOARD;
        else if (strcmp(mode, "none") == 0)
                f_mode = FOLLOW_NONE;
        else {
                fprintf(stderr, "Warning: unknown follow mode: \"%s\"\n", mode);
                f_mode = FOLLOW_NONE;
        }

}

void load_options(char *cmdline_config_path)
{

#ifndef STATIC_CONFIG
        xdgHandle xdg;
        FILE *config_file = NULL;

        xdgInitHandle(&xdg);

        if (cmdline_config_path != NULL) {
                config_file = fopen(cmdline_config_path, "r");
        }
        if (config_file == NULL) {
                config_file = xdgConfigOpen("dunst/dunstrc", "r", &xdg);
        }
        if (config_file == NULL) {
                /* Fall back to just "dunstrc", which was used before 2013-06-23
                 * (before v0.2). */
                config_file = xdgConfigOpen("dunstrc", "r", &xdg);
                if (config_file == NULL) {
                        puts("no dunstrc found -> skipping\n");
                        xdgWipeHandle(&xdg);
                        return;
                }
        }

        load_ini_file(config_file);
#endif

        font =
            option_get_string("global", "font", "-fn", font,
                              "The font dunst should use.");
        format =
            option_get_string("global", "format", "-format", format,
                              "The format template for the notifictions");
        sort =
            option_get_bool("global", "sort", "-sort", sort,
                            "Sort notifications by urgency and date?");
        indicate_hidden =
            option_get_bool("global", "indicate_hidden", "-indicate_hidden",
                            indicate_hidden,
                            "Show how many notificaitons are hidden?");
        word_wrap =
            option_get_bool("global", "word_wrap", "-word_wrap", word_wrap,
                            "Truncating long lines or do word wrap");

        ignore_newline =
            option_get_bool("global", "ignore_newline", "-ignore_newline",
                            ignore_newline, "Ignore newline characters in notifications");
        idle_threshold =
            option_get_int("global", "idle_threshold", "-idle_threshold",
                           idle_threshold,
                           "Don't timeout notifications if user is longer idle than threshold");
        monitor =
            option_get_int("global", "monitor", "-mon", monitor,
                           "On which monitor should the notifications be displayed");
        {
                char *c =
                    option_get_string("global", "follow", "-follow", "",
                                      "Follow mouse, keyboard or none?");
                if (strlen(c) > 0) {
                        parse_follow_mode(c);
                        free(c);
                }
        }
        geom =
            option_get_string("global", "geometry", "-geom/-geometry", geom,
                              "Geometry for the window");
        line_height =
            option_get_int("global", "line_height", "-lh/-line_height",
                           line_height,
                           "Add additional padding above and beneath text");
        bounce_freq =
            option_get_double("global", "bounce_freq", "-bounce_freq",
                              bounce_freq,
                              "Make long text bounce from side to side");
        {
                char *c =
                    option_get_string("global", "alignment",
                                      "-align/-alignment", "",
                                      "Align notifications left/center/right");
                if (strlen(c) > 0) {
                        if (strcmp(c, "left") == 0)
                                align = left;
                        else if (strcmp(c, "center") == 0)
                                align = center;
                        else if (strcmp(c, "right") == 0)
                                align = right;
                        else
                                fprintf(stderr,
                                        "Warning: unknown allignment\n");
                        free(c);
                }
        }
        show_age_threshold =
            option_get_int("global", "show_age_threshold",
                           "-show_age_threshold", show_age_threshold,
                           "When should the age of the notification be displayed?");
        sticky_history =
            option_get_bool("global", "sticky_history", "-sticky_history",
                            sticky_history,
                            "Don't timeout notifications popped up from history");
        separator_height =
            option_get_int("global", "separator_height",
                           "-sep_height/-separator_height", separator_height,
                           "height of the separator line");
        padding =
            option_get_int("global", "padding", "-padding", padding,
                            "Padding between text and separator");
        h_padding =
            option_get_int("global", "horizontal_padding", "-horizontal_padding",
                            h_padding, "horizontal padding");
        transparency =
            option_get_int("global", "transparency", "-transparency",
                           transparency, "Transparency. range 0-100");
        {
                char *c =
                    option_get_string("global", "separator_color",
                                      "-sep_color/-separator_color", "",
                                      "Color of the separator line (or 'auto')");
                if (strlen(c) > 0) {
                        if (strcmp(c, "auto") == 0)
                                sep_color = AUTO;
                        else if (strcmp(c, "foreground") == 0)
                                sep_color = FOREGROUND;
                        else if (strcmp(c, "frame") == 0)
                                sep_color = FRAME;
                        else {
                                sep_color = CUSTOM;
                                sep_custom_color_str = g_strdup(c);
                        }
                        free(c);
                }
        }

        startup_notification = option_get_bool("global", "startup_notification",
                        "-startup_notification", false, "print notification on startup");


        dmenu = option_get_string("global", "dmenu", "-dmenu", dmenu, "path to dmenu");
        dmenu_cmd = g_strsplit(dmenu, " ", 0);

        browser = option_get_string("global", "browser", "-browser", browser, "path to browser");

        frame_width = option_get_int("frame", "width", "-frame_width", frame_width,
                        "Width of frame around window");

        frame_color = option_get_string("frame", "color", "-frame_color",
                        frame_color, "Color of the frame around window");

        lowbgcolor =
            option_get_string("urgency_low", "background", "-lb", lowbgcolor,
                              "Background color for notifcations with low urgency");
        lowfgcolor =
            option_get_string("urgency_low", "foreground", "-lf", lowfgcolor,
                              "Foreground color for notifications with low urgency");
        timeouts[LOW] =
            option_get_int("urgency_low", "timeout", "-lto", timeouts[LOW],
                           "Timeout for notifications with low urgency");
        normbgcolor =
            option_get_string("urgency_normal", "background", "-nb",
                              normbgcolor,
                              "Background color for notifications with normal urgency");
        normfgcolor =
            option_get_string("urgency_normal", "foreground", "-nf",
                              normfgcolor,
                              "Foreground color for notifications with normal urgency");
        timeouts[NORM] =
            option_get_int("urgency_normal", "timeout", "-nto", timeouts[NORM],
                           "Timeout for notifications with normal urgency");
        critbgcolor =
            option_get_string("urgency_critical", "background", "-cb",
                              critbgcolor,
                              "Background color for notifications with critical urgency");
        critfgcolor =
            option_get_string("urgency_critical", "foreground", "-cf",
                              critfgcolor,
                              "Foreground color for notifications with ciritical urgency");
        timeouts[CRIT] =
            option_get_int("urgency_critical", "timeout", "-cto",
                           timeouts[CRIT],
                           "Timeout for notifications with critical urgency");

        close_ks.str =
            option_get_string("shortcuts", "close", "-key", close_ks.str,
                              "Shortcut for closing one notification");
        close_all_ks.str =
            option_get_string("shortcuts", "close_all", "-all_key",
                              close_all_ks.str,
                              "Shortcut for closing all notifications");
        history_ks.str =
            option_get_string("shortcuts", "history", "-history_key",
                              history_ks.str,
                              "Shortcut to pop the last notification from history");

        context_ks.str =
                option_get_string("shortcuts", "context", "-context_key",
                                context_ks.str,
                                "Shortcut for context menu");

        print_notifications =
            cmdline_get_bool("-print", false,
                             "Print notifications to cmdline (DEBUG)");

        char *cur_section = NULL;
        for (;;) {
                cur_section = next_section(cur_section);
                if (!cur_section)
                        break;
                if (strcmp(cur_section, "global") == 0
                    || strcmp(cur_section, "shortcuts") == 0
                    || strcmp(cur_section, "urgency_low") == 0
                    || strcmp(cur_section, "urgency_normal") == 0
                    || strcmp(cur_section, "urgency_critical") == 0)
                        continue;

                /* check for existing rule with same name */
                rule_t *r = NULL;
                for (GSList *iter = rules; iter; iter = iter->next) {
                        rule_t *match = iter->data;
                        if (match->name &&
                            strcmp(match->name, cur_section) == 0)
                                r = match;
                }

                if (r == NULL) {
                        r = g_malloc(sizeof(rule_t));
                        rule_init(r);
                        rules = g_slist_insert(rules, r, 0);
                }

                r->name = g_strdup(cur_section);
                r->appname = ini_get_string(cur_section, "appname", r->appname);
                r->summary = ini_get_string(cur_section, "summary", r->summary);
                r->body = ini_get_string(cur_section, "body", r->body);
                r->icon = ini_get_string(cur_section, "icon", r->icon);
                r->timeout = ini_get_int(cur_section, "timeout", r->timeout);
                {
                        char *urg = ini_get_string(cur_section, "urgency", "");
                        if (strlen(urg) > 0) {
                                if (strcmp(urg, "low") == 0)
                                        r->urgency = LOW;
                                else if (strcmp(urg, "normal") == 0)
                                        r->urgency = NORM;
                                else if (strcmp(urg, "critical") == 0)
                                        r->urgency = CRIT;
                                else
                                        fprintf(stderr,
                                                "unknown urgency: %s, ignoring\n",
                                                urg);
                                free(urg);
                        }
                }
                r->fg = ini_get_string(cur_section, "foreground", r->fg);
                r->bg = ini_get_string(cur_section, "background", r->bg);
                r->format = ini_get_string(cur_section, "format", r->format);
                r->script = ini_get_string(cur_section, "script", NULL);
        }

#ifndef STATIC_CONFIG
        fclose(config_file);
        free_ini();
        xdgWipeHandle(&xdg);
#endif
}

int main(int argc, char *argv[])
{

        history = g_queue_new();
        displayed = g_queue_new();
        queue = g_queue_new();

        for (int i = 0; i < LENGTH(default_rules); i++) {
                rules = g_slist_insert(rules, &(default_rules[i]), 0);
        }

        cmdline_load(argc, argv);

        if (cmdline_get_bool("-v/-version", false, "Print version")
            || cmdline_get_bool("--version", false, "Print version")) {
                print_version();
        }

        char *cmdline_config_path;
        cmdline_config_path =
            cmdline_get_string("-conf/-config", NULL,
                               "Path to configuration file");
        load_options(cmdline_config_path);

        if (cmdline_get_bool("-h/-help", false, "Print help")
            || cmdline_get_bool("--help", false, "Print help")) {
                usage(EXIT_SUCCESS);
        }

        int owner_id = initdbus();
        setup();
        signal (SIGUSR1, pause_signal_handler);
        signal (SIGUSR2, pause_signal_handler);

        if (startup_notification) {
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
                init_notification(n, 0);
        }

        mainloop = g_main_loop_new(NULL, FALSE);

        GPollFD dpy_pollfd = {dc->dpy->fd,
                G_IO_IN | G_IO_HUP | G_IO_ERR, 0 };

        GSourceFuncs x11_source_funcs = {
                x11_fd_prepare,
                x11_fd_check,
                x11_fd_dispatch,
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

void usage(int exit_status)
{
        fputs("usage:\n", stderr);
        char *us = cmdline_create_usage();
        fputs(us, stderr);
        fputs("\n", stderr);
        exit(exit_status);
}

void print_version(void)
{
        printf("Dunst - A customizable and lightweight notification-daemon %s\n",
               VERSION);
        exit(EXIT_SUCCESS);
}

/* vim: set ts=8 sw=8 tw=0: */

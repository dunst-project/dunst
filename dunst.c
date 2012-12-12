/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE
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
#include <signal.h>
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
#include "dunst_dbus.h"
#include "container.h"
#include "utils.h"

#include "options.h"

#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MIN(a,b)                ((a) < (b) ? (a) : (b))
#define MAX(a,b)                ((a) > (b) ? (a) : (b))
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define FONT_HEIGHT_BORDER 2

#define DEFFONT "Monospace-11"

#ifndef VERSION
#define VERSION 0
#endif

#define MSG 1
#define INFO 2
#define DEBUG 3

/* global variables */

#include "config.h"

int height_limit;

rule_array rules;
/* index of colors fit to urgency level */
static ColorSet *colors[3];
static const char *color_strings[2][3];
static Atom utf8;
static DC *dc;
static Window win;
static time_t now;
static bool visible = false;
static screen_info scr;
static dimension_t geometry;
static XScreenSaverInfo *screensaver_info;
static int font_h;
static bool print_notifications = false;
static dimension_t window_dim;
static bool pause_display = false;

static r_line_cache line_cache;

bool dunst_grab_errored = false;

int next_notification_id = 1;

/* notification lists */
n_queue *queue = NULL;             /* all new notifications get into here */
list *displayed_notifications = NULL;   /* currently displayed notifications */
n_stack *history = NULL;      /* history of displayed notifications */

/* misc funtions */
void apply_rules(notification * n);
void check_timeouts(void);
char *fix_markup(char *str);
void handle_mouse_click(XEvent ev);
void handleXEvents(void);
void history_pop(void);
void initrule(rule_t *r);
bool is_idle(void);
void run(void);
void setup(void);
void update_screen_info();
void usage(int exit_status);
l_node *most_important(list * l);
void draw_win(void);
void hide_win(void);
void move_all_to_history(void);
void print_version(void);

void r_line_cache_init(r_line_cache *c);
void r_line_cache_append(r_line_cache *c, const char *s, ColorSet *col, bool continues);
void r_line_cache_reset(r_line_cache *c);

void init_shortcut(keyboard_shortcut * shortcut);
KeySym string_to_mask(char *str);

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
        printf("\tappname: %s\n", n->appname);
        printf("\tsummary: %s\n", n->summary);
        printf("\tbody: %s\n", n->body);
        printf("\ticon: %s\n", n->icon);
        printf("\turgency: %d\n", n->urgency);
        printf("\tformatted: %s\n", n->msg);
        printf("\tid: %d\n", n->id);
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

void apply_rules(notification * n)
{

        for (int i = 0; i < rules.count; i++) {
                rule_t *r = &(rules.rules[i]);
                if ((!r->appname || !fnmatch(r->appname, n->appname, 0))
                    && (!r->summary || !fnmatch(r->summary, n->summary, 0))
                    && (!r->body || !fnmatch(r->body, n->body, 0))
                    && (!r->icon || !fnmatch(r->icon, n->icon, 0))) {
                        n->timeout = r->timeout != -1 ? r->timeout : n->timeout;
                        n->urgency = r->urgency != -1 ? r->urgency : n->urgency;
                        n->color_strings[ColFG] =
                            r->fg ? r->fg : n->color_strings[ColFG];
                        n->color_strings[ColBG] =
                            r->bg ? r->bg : n->color_strings[ColBG];
                        n->format = r->format ? r->format : n->format;
                }
        }
}

void check_timeouts(void)
{
        l_node *iter;
        notification *current;
        l_node *next;

        /* nothing to do */
        if (l_is_empty(displayed_notifications)) {
                return;
        }

        iter = displayed_notifications->head;
        while (iter != NULL) {
                current = (notification *) iter->data;

                /* don't timeout when user is idle */
                if (is_idle()) {
                        current->start = now;
                        iter = iter->next;
                        continue;
                }

                /* skip hidden and sticky messages */
                if (current->start == 0 || current->timeout == 0) {
                        iter = iter->next;
                        continue;
                }

                /* remove old message */
                if (difftime(now, current->start) > current->timeout) {
                        /* l_move changes iter->next, so we need to store it beforehand */
                        next = iter->next;
                        close_notification(current, 1);
                        iter = next;
                        continue;

                } else {
                        iter = iter->next;
                }
        }
}

void update_lists()
{
        int limit;

        check_timeouts();

        if (pause_display) {
                while (!l_is_empty(displayed_notifications)) {
                        notification *n = (notification *) l_remove(displayed_notifications, displayed_notifications->head);
                        n_queue_enqueue(&queue, n);
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
        while (queue) {

                if (limit > 0 && l_length(displayed_notifications) >= limit) {
                        /* the list is full */
                        break;
                }

                notification *n = n_queue_dequeue(&queue);

                if (!n)
                        return;
                n->start = now;

                l_push(displayed_notifications, n);

                l_sort(displayed_notifications, cmp_notification);

        }
}

void r_line_cache_init(r_line_cache *c)
{
    c->count = 0;
    c->size = 0;
    c->lines = NULL;
}

void r_line_cache_append(r_line_cache *c, const char *s, ColorSet *col, bool continues)
{
    if (!c || !s)
        return;

    /* resize cache if it's too small */
    if (c->count >= c->size) {
        c->size++;
        c->lines = realloc(c->lines, c->size * sizeof(r_line));
    }

    c->count++;
    c->lines[c->count-1].colors = col;
    c->lines[c->count-1].str = strdup(s);
    c->lines[c->count-1].continues = continues;
}

void r_line_cache_reset(r_line_cache *c)
{
    for (int i = 0; i < c->count; i++) {
        if (c->lines[i].str)
            free(c->lines[i].str);
        c->lines[i].str = NULL;
        c->lines[i].colors = NULL;
    }
    c->count = 0;
}

int do_word_wrap(char *source, int max_width)
{

        rstrip(source);

        if (!source || strlen(source) == 0)
                return 0;

        char *eol = source;

        while (true) {
                if (*eol == '\0')
                        return 1;
                if (*eol == '\n') {
                        *eol = '\0';
                        return 1 + do_word_wrap(eol + 1, max_width);
                }
                if (*eol == '\\' && *(eol + 1) == 'n') {
                        *eol = ' ';
                        *(eol + 1) = '\0';
                        return 1 + do_word_wrap(eol + 2, max_width);
                }

                if (word_wrap && max_width > 0) {
                        if (textnw(dc, source, (eol - source) + 1) >= max_width) {
                                /* go back to previous space */
                                char *space = eol;
                                while (space > source && !isspace(*space))
                                        space--;

                                if (space <= source) {
                                        /* whe have a word longer than width, so we
                                         * split mid-word. That one letter is
                                         * collateral damage */
                                        space = eol;
                                }
                                *space = '\0';
                                if (*(space + 1) == '\0')
                                        return 1;
                                return 1 + do_word_wrap(space + 1, max_width);
                        }
                }
                eol++;
        }
}

void add_notification_to_line_cache(notification *n, int max_width)
{
        rstrip(n->msg);
        char *msg = n->msg;
        while (isspace(*msg))
                msg++;

        char *buf;

        /* print dup_count */
        if (n->dup_count > 0) {
                asprintf(&buf, "(%d)", n->dup_count);
        } else {
                buf = strdup("");
        }

        /* print msg */
        {
                char *new_buf;
                asprintf(&new_buf, "%s %s", buf, msg);
                free(buf);
                buf = new_buf;
        }

        /* print age */
        int hours, minutes, seconds;
        time_t t_delta = now - n->timestamp;

        if (show_age_threshold >= 0 && t_delta >= show_age_threshold) {
                hours = t_delta / 3600;
                minutes = t_delta / 60 % 60;
                seconds = t_delta % 60;

                char *new_buf;
                if (hours > 0) {
                        asprintf(&new_buf, "%s (%dh %dm %ds old)", buf, hours,
                                 minutes, seconds);
                } else if (minutes > 0) {
                        asprintf(&new_buf, "%s (%dm %ds old)", buf, minutes,
                                 seconds);
                } else {
                        asprintf(&new_buf, "%s (%ds old)", buf, seconds);
                }

                free(buf);
                buf = new_buf;
        }


        int linecnt = do_word_wrap(buf, max_width);
        n->line_count = linecnt;
        char *cur = buf;
        for (int i = 0; i < linecnt; i++) {
                r_line_cache_append(&line_cache, cur, n->colors, i+1 != linecnt);

                while (*cur != '\0')
                        cur++;
                cur++;
        }
        free(buf);
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
                return 0;
        case center:
                return leftover / 2;
        case right:
                return leftover;
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

void fill_line_cache(int width)
{
        /* create cache with all lines */
        for (l_node * iter = displayed_notifications->head; iter;
             iter = iter->next) {
                notification *n = (notification *) iter->data;
                add_notification_to_line_cache(n, width);
        }

        assert(line_cache.count > 0);

        /* add (x more) */
        int queue_cnt = n_queue_len(&queue);
        if (indicate_hidden && queue_cnt > 0) {
                if (geometry.h != 1) {
                        char *tmp;
                        asprintf(&tmp, "(%d more)", queue_cnt);
                        ColorSet *last_colors =
                                line_cache.lines[line_cache.count-1].colors;
                        r_line_cache_append(&line_cache, strdup(tmp), last_colors, false);
                        free(tmp);
                } else {
                        char *old = line_cache.lines[0].str;
                        char *new;
                        asprintf(&new, "%s (%d more)", old, queue_cnt);
                        free(old);
                        line_cache.lines[0].str = new;
                }
        }
}


void draw_win(void)
{

        r_line_cache_reset(&line_cache);
        update_screen_info();
        int width = calculate_width();


        line_height = MAX(line_height, font_h);


        fill_line_cache(width);


        /* if we have a dynamic width, calculate the actual width */
        if (width == 0) {
                for (int i = 0; i < line_cache.count; i++) {
                        char *line = line_cache.lines[i].str;
                        width = MAX(width, textw(dc, line));
                }
        }

        /* resize dc to correct width */

        int height = (line_cache.count * line_height)
                   + ((line_cache.count - 1) * separator_height);


        resizedc(dc, width, height);

        dc->y = 0;

        for (int i = 0; i < line_cache.count; i++) {
                dc->x = 0;

                r_line line = line_cache.lines[i];


                /* draw background */
                drawrect(dc, 0, 0, width, line_height, true, line.colors->BG);

                /* draw text */
                dc->x = calculate_x_offset(width, textw(dc, line.str));
                dc->y += (line_height - font_h) / 2;
                drawtextn(dc, line.str, strlen(line.str), line.colors);
                dc->y += line_height - ((line_height - font_h) / 2);

                /* draw separator */
                if (separator_height > 0 && i < line_cache.count - 1 && !line.continues) {
                        dc->x = 0;
                        double color;
                        if (sep_color == AUTO)
                                color = calculate_foreground_color(line.colors->BG);
                        else
                                color = line.colors->FG;
                        drawrect(dc, 0, 0, width, separator_height, true, color);
                        dc->y += separator_height;
                }

        }

        move_and_map(width, height);
}

char
*fix_markup(char *str)
{
        char *replace_buf, *start, *end;

        if (str == NULL) {
                return NULL;
        }

        str = string_replace("&quot;", "\"", str);
        str = string_replace("&apos;", "'", str);
        str = string_replace("&amp;", "&", str);
        str = string_replace("&lt;", "<", str);
        str = string_replace("&gt;", ">", str);

        str = string_replace("\n", " ", str);
        /* remove tags */
        str = string_replace("<b>", "", str);
        str = string_replace("</b>", "", str);
        str = string_replace("<br>", " ", str);
        str = string_replace("<br/>", " ", str);
        str = string_replace("<br />", " ", str);
        str = string_replace("<i>", "", str);
        str = string_replace("</i>", "", str);
        str = string_replace("<u>", "", str);
        str = string_replace("</u>", "", str);
        str = string_replace("</a>", "", str);

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
                int y = 0;
                notification *n;
                l_node *iter = displayed_notifications->head;
                assert(iter);
                for (; iter; iter = iter->next) {
                        n = (notification *) iter->data;
                        int height = font_h * n->line_count;
                        if (ev.xbutton.y > y && ev.xbutton.y < y + height)
                                break;
                        else
                                y += height;
                }
                close_notification(n, 2);
        }
}

void handleXEvents(void)
{
        XEvent ev;
        while (XPending(dc->dpy) > 0) {
                XNextEvent(dc->dpy, &ev);
                switch (ev.type) {
                case Expose:
                        if (ev.xexpose.count == 0 && visible) {
                                draw_win();
                                mapdc(dc, win, scr.dim.w, font_h);
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
                                if (!l_is_empty(displayed_notifications)) {
                                        notification *n = (notification *)
                                            displayed_notifications->head->data;
                                        close_notification(n, 2);
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
                }
        }
}

void move_all_to_history()
{
        l_node *node;
        notification *n;

        while (!l_is_empty(displayed_notifications)) {
                node = displayed_notifications->head;
                n = (notification *) node->data;
                close_notification(n, 2);
        }

        n = n_queue_dequeue(&queue);
        while (n) {
                n_stack_push(&history, n);
                n = n_queue_dequeue(&queue);
        }
}

void history_pop(void)
{

        if (!history)
                return;

        notification *n = n_stack_pop(&history);
        n->redisplayed = true;
        n->start = 0;
        n->timeout = sticky_history ? 0 : n->timeout;
        n_queue_enqueue(&queue, n);

        if (!visible) {
                map_win();
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

        n->format = format;

        apply_rules(n);

        n->msg = string_replace("%a", n->appname, strdup(n->format));
        n->msg = string_replace("%s", n->summary, n->msg);
        n->msg = string_replace("%i", n->icon, n->msg);
        n->msg = string_replace("%I", basename(n->icon), n->msg);
        n->msg = string_replace("%b", n->body, n->msg);
        if (n->progress) {
                char pg[10];
                sprintf(pg, "[%3d%%]", n->progress - 1);
                n->msg = string_replace("%p", pg, n->msg);
        } else {
                n->msg = string_replace("%p", "", n->msg);
        }

        n->msg = fix_markup(n->msg);

        n->dup_count = 0;

        /* check if n is a duplicate */
        for (n_queue *iter = queue; iter; iter = iter->next) {
                notification *orig = iter->n;
                if (strcmp(orig->appname, n->appname) == 0
                    && strcmp(orig->msg, n->msg) == 0) {
                        orig->dup_count++;
                        free_notification(n);
                        return orig->id;
                }
        }

        for (l_node * iter = displayed_notifications->head; iter;
             iter = iter->next) {
                notification *orig = (notification *) iter->data;
                if (strcmp(orig->appname, n->appname) == 0
                    && strcmp(orig->msg, n->msg) == 0) {
                        orig->dup_count++;
                        orig->start = now;
                        free_notification(n);
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

        n->timestamp = now;

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
                n_queue_enqueue(&queue, n);
        }

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

        for (l_node *iter = displayed_notifications->head; iter; iter = iter->next) {
                notification *n = (notification *) iter->data;
                if (n->id == id) {
                        l_remove(displayed_notifications, iter);
                        n_stack_push(&history, n);
                        target = n;
                        break;
                }
        }

        for (n_queue *iter = queue; iter && iter->next; iter = iter->next) {
                notification *n = iter->next->n;
                if (n->id == id) {
                        n_queue *tmp = iter->next;
                        iter->next = iter->next->next;
                        free(tmp);
                        n_stack_push(&history, n);
                        target = n;
                        break;
                }
        }

        if (reason > 0 && reason < 4 && target != NULL) {
                notificationClosed(target, reason);
        }

        return target == NULL;
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

        char *str = strdup(ks->str);
        char *str_begin = str;

        if (str == NULL)
                die("Unable to allocate memory", EXIT_FAILURE);

        while (strstr(str, "+")) {
                char *mod = str;
                while (*str != '+')
                        str++;
                *str = '\0';
                str++;
                rstrip(mod);
                ks->mask = ks->mask | string_to_mask(mod);
        }
        rstrip(str);

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

void initrule(rule_t *r)
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

bool is_idle(void)
{
        XScreenSaverQueryInfo(dc->dpy, DefaultRootWindow(dc->dpy),
                              screensaver_info);
        if (idle_threshold == 0) {
                return false;
        }
        return screensaver_info->idle / 1000 > idle_threshold;
}

void run(void)
{
        while (true) {
                if (visible) {
                        dbus_poll(50);
                } else {
                        dbus_poll(200);
                }
                now = time(&now);

                /* move messages from notification_queue to displayed_notifications */
                update_lists();
                if (l_length(displayed_notifications) > 0) {
                        if (!visible) {
                                map_win();
                        } else {
                                draw_win();
                        }
                } else {
                        if (visible) {
                                hide_win();
                        }
                }
                handleXEvents();
        }
}

void hide_win()
{
        ungrab_key(&close_ks);
        ungrab_key(&close_all_ks);

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

        grab_key(&close_ks);
        ungrab_key(&close_ks);
        grab_key(&close_all_ks);
        ungrab_key(&close_all_ks);
        grab_key(&history_ks);
        ungrab_key(&history_ks);

        colors[LOW] = initcolor(dc, lowfgcolor, lowbgcolor);
        colors[NORM] = initcolor(dc, normfgcolor, normbgcolor);
        colors[CRIT] = initcolor(dc, critfgcolor, critbgcolor);

        color_strings[ColFG][LOW] = lowfgcolor;
        color_strings[ColFG][NORM] = normfgcolor;
        color_strings[ColFG][CRIT] = critfgcolor;

        color_strings[ColBG][LOW] = lowbgcolor;
        color_strings[ColBG][NORM] = normbgcolor;
        color_strings[ColBG][CRIT] = critbgcolor;

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
        if (visible || l_is_empty(displayed_notifications)) {
                return;
        }

        grab_key(&close_ks);
        grab_key(&close_all_ks);
        setup_error_handler();
        XGrabButton(dc->dpy, AnyButton, AnyModifier, win, false,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        if (tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab mouse button(s)\n");
        }

        update_screen_info();
        XMapRaised(dc->dpy, win);
        draw_win();
        XFlush(dc->dpy);
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
                /* Fall back to just "dunstrc", which was used before 2012-06-23
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
                        else
                                fprintf(stderr,
                                        "Warning: Unknown separator color\n");
                        free(c);
                }
        }

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
                for (int i = 0; i < rules.count; i++)
                        if (rules.rules[i].name &&
                            strcmp(rules.rules[i].name, cur_section) == 0)
                                r = &(rules.rules[i]);

                if (r == NULL) {
                        rules.count++;
                        rules.rules = realloc(rules.rules,
                                        rules.count * sizeof(rule_t));
                        r = &(rules.rules[rules.count-1]);
                        initrule(r);
                }

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
        }

#ifndef STATIC_CONFIG
        fclose(config_file);
        free_ini();
        xdgWipeHandle(&xdg);
#endif
}

int main(int argc, char *argv[])
{
        now = time(&now);

        displayed_notifications = l_init();
        r_line_cache_init(&line_cache);


        rules.count = LENGTH(default_rules);
        rules.rules = calloc(rules.count, sizeof(rule_t));
        memcpy(rules.rules, default_rules, sizeof(rule_t) * rules.count);

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

        initdbus();
        setup();
        signal (SIGUSR1, pause_signal_handler);
        signal (SIGUSR2, pause_signal_handler);

        run();
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
        printf("Dunst - a dmenu-ish notification-daemon, version: %s\n",
               VERSION);
        exit(EXIT_SUCCESS);
}

/* vim: set ts=8 sw=8 tw=0: */

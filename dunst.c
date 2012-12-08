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
#include "list.h"
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

list *rules = NULL;
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

bool dunst_grab_errored = false;

int next_notification_id = 1;

bool deprecated_mod = false;
bool deprecated_dunstrc_shortcuts = false;

/* notification lists */
list *notification_queue = NULL;        /* all new notifications get into here */
list *displayed_notifications = NULL;   /* currently displayed notifications */
list *notification_history = NULL;      /* history of displayed notifications */

/* misc funtions */
void apply_rules(notification * n);
void check_timeouts(void);
char *fix_markup(char *str);
void handle_mouse_click(XEvent ev);
void handleXEvents(void);
void history_pop(void);
rule_t *initrule(void);
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

/* show warning notification */
void warn(const char *text, int urg);

void init_shortcut(keyboard_shortcut * shortcut);
KeySym string_to_mask(char *str);

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

void warn(const char *text, int urg)
{
        notification *n = malloc(sizeof(notification));

        if (n == NULL)
                die("Unable to allocate memory", EXIT_FAILURE);

        n->appname = strdup("dunst");
        n->summary = strdup(text);
        if (n->summary == NULL)
                die("Unable to allocate memory", EXIT_FAILURE);
        n->body = strdup("");
        n->icon = strdup("");
        n->timeout = 0;
        n->urgency = urg;
        n->progress = 0;
        n->dbus_client = NULL;
        n->color_strings[ColFG] = NULL;
        n->color_strings[ColBG] = NULL;
        init_notification(n, 0);
        map_win();
}

int cmp_notification(void *a, void *b)
{
        if (a == NULL && b == NULL)
                return 0;
        else if (a == NULL)
                return -1;
        else if (b == NULL)
                return 1;

        notification *na = (notification *) a;
        notification *nb = (notification *) b;
        if (na->urgency != nb->urgency) {
                return na->urgency - nb->urgency;
        } else {
                return nb->timestamp - na->timestamp;
        }
}

l_node *most_important(list * l)
{

        if (l == NULL || l_is_empty(l)) {
                return NULL;
        }

        if (sort) {
                notification *max;
                l_node *node_max;
                notification *data;

                max = l->head->data;
                node_max = l->head;
                for (l_node * iter = l->head; iter; iter = iter->next) {
                        data = (notification *) iter->data;
                        if (cmp_notification(max, data) < 0) {
                                max = data;
                                node_max = iter;
                        }
                }
                return node_max;
        } else {
                return l->head;
        }
}

void apply_rules(notification * n)
{
        if (l_is_empty(rules) || n == NULL) {
                return;
        }

        for (l_node * iter = rules->head; iter; iter = iter->next) {
                rule_t *r = (rule_t *) iter->data;

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
        l_node *to_move;
        notification *n;
        int limit;

        check_timeouts();

        if (pause_display) {
                while (!l_is_empty(displayed_notifications)) {
                        l_move(displayed_notifications, notification_queue, displayed_notifications->head);
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
        while (!l_is_empty(notification_queue)) {

                if (limit > 0 && l_length(displayed_notifications) >= limit) {
                        /* the list is full */
                        break;
                }

                to_move = most_important(notification_queue);
                if (!to_move) {
                        return;
                }
                n = (notification *) to_move->data;
                if (!n)
                        return;
                n->start = now;

                /* TODO get notifications pushed back into
                 * notification_queue if there's a more important
                 * message waiting there
                 */

                l_move(notification_queue, displayed_notifications, to_move);

                l_sort(displayed_notifications, cmp_notification);

        }
}

/* TODO get draw_txt_buf as argument */
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

void update_draw_txt_buf(notification * n, int max_width)
{
        rstrip(n->msg);
        char *msg = n->msg;
        while (isspace(*msg))
                msg++;

        if (n->draw_txt_buf.txt)
                free(n->draw_txt_buf.txt);

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

        n->draw_txt_buf.line_count = do_word_wrap(buf, max_width);
        n->draw_txt_buf.txt = buf;
}

char *draw_txt_get_line(draw_txt * dt, int line)
{
        if (line > dt->line_count) {
                return NULL;
        }

        char *begin = dt->txt;
        for (int i = 1; i < line; i++) {
                begin += strlen(begin) + 1;
        }

        return begin;
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

void draw_win(void)
{
        int width, x, y, height;

        line_height = MAX(line_height, font_h);

        update_screen_info();

        /* calculate width */
        if (geometry.mask & WidthValue && geometry.w == 0) {
                /* dynamic width */
                width = 0;
        } else if (geometry.mask & WidthValue) {
                /* fixed width */
                if (geometry.negative_width) {
                        width = scr.dim.w - geometry.w;
                } else {
                        width = geometry.w;
                }
        } else {
                /* across the screen */
                width = scr.dim.w;
        }

        /* update draw_txt_bufs and line_cnt */
        int line_cnt = 0;
        for (l_node * iter = displayed_notifications->head; iter;
             iter = iter->next) {
                notification *n = (notification *) iter->data;
                update_draw_txt_buf(n, width);
                line_cnt += n->draw_txt_buf.line_count;
        }

        /* calculate height */
        if (geometry.h == 0) {
                height = line_cnt * line_height;
        } else {
                height = MAX(geometry.h, (line_cnt * line_height));
        }

        height += (l_length(displayed_notifications) - 1) * separator_height;

        /* add "(x more)" */
        draw_txt x_more;
        x_more.txt = NULL;

        char *print_to;
        int more = l_length(notification_queue);
        if (indicate_hidden && more > 0) {

                int x_more_len = strlen(" ( more) ") + digit_count(more);

                if (geometry.h != 1) {
                        /* add additional line */
                        x_more.txt = calloc(x_more_len, sizeof(char));
                        height += line_height;
                        line_cnt++;

                        print_to = x_more.txt;

                } else {
                        /* append "(x more)" message to notification text */
                        notification *n =
                            (notification *) displayed_notifications->head->
                            data;
                        print_to =
                            draw_txt_get_line(&n->draw_txt_buf,
                                              n->draw_txt_buf.line_count);
                        for (; *print_to != '\0'; print_to++) ;
                }
                snprintf(print_to, x_more_len, "(%d more)", more);
        }

        /* if we have a dynamic width, calculate the actual width */
        if (width == 0) {
                for (l_node * iter = displayed_notifications->head; iter;
                     iter = iter->next) {
                        notification *n = (notification *) iter->data;
                        for (int i = 0; i < n->draw_txt_buf.line_count; i++) {
                                char *line =
                                    draw_txt_get_line(&n->draw_txt_buf, i + 1);
                                assert(line != NULL);
                                width = MAX(width, textw(dc, line));
                        }
                }
        }

        assert(line_height > 0);
        assert(font_h > 0);
        assert(width > 0);
        assert(height > 0);
        assert(line_cnt > 0);

        resizedc(dc, width, height);

        /* draw buffers */
        dc->y = 0;
        ColorSet *last_color;
        assert(displayed_notifications->head != NULL);
        for (l_node * iter = displayed_notifications->head; iter;
             iter = iter->next) {

                notification *n = (notification *) iter->data;
                last_color = n->colors;

                for (int i = 0; i < n->draw_txt_buf.line_count; i++) {

                        char *line = draw_txt_get_line(&n->draw_txt_buf, i + 1);
                        dc->x = 0;
                        drawrect(dc, 0, 0, width, line_height, true,
                                 n->colors->BG);

                        dc->x = calculate_x_offset(width, textw(dc, line));
                        dc->y += (line_height - font_h) / 2;
                        drawtextn(dc, line, strlen(line), n->colors);
                        dc->y += line_height - ((line_height - font_h) / 2);
                }

                /* draw separator */
                if (separator_height > 0) {
                        dc->x = 0;
                        double color;
                        if (sep_color == AUTO)
                                color =
                                    calculate_foreground_color(n->colors->BG);
                        else
                                color = n->colors->FG;

                        drawrect(dc, 0, 0, width, separator_height, true,
                                 color);
                        dc->y += separator_height;
                }
        }

        /* draw x_more */
        if (x_more.txt) {
                dc->x = 0;
                drawrect(dc, 0, 0, width, line_height, true, last_color->BG);
                dc->x = calculate_x_offset(width, textw(dc, x_more.txt));
                drawtext(dc, x_more.txt, last_color);
        }

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

        free(x_more.txt);
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
                        int height = font_h * n->draw_txt_buf.line_count;
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
        while (!l_is_empty(notification_queue)) {
                node = notification_queue->head;
                n = (notification *) node->data;
                close_notification(n, 2);
        }
}

void history_pop(void)
{
        l_node *iter;
        notification *data;

        /* nothing to do */
        if (l_is_empty(notification_history)) {
                return;
        }

        for (iter = notification_history->head; iter->next; iter = iter->next) ;
        data = (notification *) iter->data;
        data->redisplayed = true;
        data->start = 0;
        if (sticky_history) {
                data->timeout = 0;
        }
        l_move(notification_history, notification_queue, iter);

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
        n->draw_txt_buf.txt = NULL;

        /* check if n is a duplicate */
        for (l_node * iter = notification_queue->head; iter; iter = iter->next) {
                notification *orig = (notification *) iter->data;
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
                l_push(notification_queue, n);
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
        l_node *iter;
        notification *target = NULL;

        for (iter = displayed_notifications->head; iter; iter = iter->next) {
                notification *n = (notification *) iter->data;
                if (n->id == id) {
                        l_move(displayed_notifications, notification_history,
                               iter);
                        target = n;
                        break;
                }
        }

        for (iter = notification_queue->head; iter; iter = iter->next) {
                notification *n = (notification *) iter->data;
                if (n->id == id) {
                        l_move(notification_queue, notification_history, iter);
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

rule_t *initrule(void)
{
        rule_t *r = malloc(sizeof(rule_t));
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

        return r;
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

static rule_t *dunst_rules_find_or_create(const char *section)
{
        l_node *iter;
        rule_t *rule;

        /* find rule */
        for (iter = rules->head; iter; iter = iter->next) {
                rule_t *r = (rule_t *) iter->data;
                if (strcmp(r->name, section) == 0) {
                        return r;
                }
        }

        rule = initrule();
        rule->name = strdup(section);

        l_push(rules, rule);

        return rule;
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
        {
                char *c = option_get_string("global", "modifier", NULL, "", "");
                if (strlen(c) > 0) {
                        deprecated_dunstrc_shortcuts = true;
                        KeySym mod = string_to_mask(c);
                        close_ks.mask = mod;
                        close_all_ks.mask = mod;
                        close_all_ks.mask = mod;
                        free(c);
                }
        }
        close_ks.str =
            option_get_string("global", "key", NULL, close_ks.str, "");
        close_all_ks.str =
            option_get_string("global", "all_key", NULL, close_all_ks.str, "");
        history_ks.str =
            option_get_string("global", "history_key", NULL, history_ks.str,
                              "");
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

                rule_t *current_rule = dunst_rules_find_or_create(cur_section);
                current_rule->appname =
                    ini_get_string(cur_section, "appname",
                                   current_rule->appname);
                current_rule->summary =
                    ini_get_string(cur_section, "summary",
                                   current_rule->summary);
                current_rule->body =
                    ini_get_string(cur_section, "body", current_rule->body);
                current_rule->icon =
                    ini_get_string(cur_section, "icon", current_rule->icon);
                current_rule->timeout =
                    ini_get_int(cur_section, "timeout", current_rule->timeout);
                {
                        char *urg = ini_get_string(cur_section, "urgency", "");
                        if (strlen(urg) > 0) {
                                if (strcmp(urg, "low") == 0)
                                        current_rule->urgency = LOW;
                                else if (strcmp(urg, "normal") == 0)
                                        current_rule->urgency = NORM;
                                else if (strcmp(urg, "critical") == 0)
                                        current_rule->urgency = CRIT;
                                else
                                        fprintf(stderr,
                                                "unknown urgency: %s, ignoring\n",
                                                urg);
                                free(urg);
                        }
                }
                current_rule->fg =
                    ini_get_string(cur_section, "foreground", current_rule->fg);
                current_rule->bg =
                    ini_get_string(cur_section, "background", current_rule->bg);
                current_rule->format =
                    ini_get_string(cur_section, "format", current_rule->format);
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

        notification_queue = l_init();
        notification_history = l_init();
        displayed_notifications = l_init();
        rules = l_init();

        for (int i = 0; i < LENGTH(default_rules); i++) {
                l_push(rules, &default_rules[i]);
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

        initdbus();
        setup();

        if (deprecated_mod)
                warn("-mod is deprecated. Use \"-key mod+key\" instead\n",
                     CRIT);
        if (deprecated_dunstrc_shortcuts)
                warn("You are using deprecated settings. Please update your dunstrc. SEE [shortcuts]", CRIT);
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

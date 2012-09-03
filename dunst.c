/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <getopt.h>
#include <X11/Xlib.h>
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
#include "ini.h"
#include "utils.h"

#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MIN(a,b)                ((a) < (b) ? (a) : (b))
#define MAX(a,b)                ((a) > (b) ? (a) : (b))
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define FONT_HEIGHT_BORDER 2

#define DEFFONT "Monospace-11"

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
static int visible = False;
static screen_info scr;
static dimension_t geometry;
static XScreenSaverInfo *screensaver_info;
static int font_h;

int dunst_grab_errored = False;

int next_notification_id = 1;

int deprecated_mod = False;
int deprecated_dunstrc_shortcuts = False;

/* notification lists */
list *notification_queue = NULL;        /* all new notifications get into here */
list *displayed_notifications = NULL;   /* currently displayed notifications */
list *notification_history = NULL;      /* history of displayed notifications */

/* misc funtions */
void apply_rules(notification * n);
void check_timeouts(void);
void draw_notifications(void);
char *fix_markup(char *str);
void handle_mouse_click(XEvent ev);
void handleXEvents(void);
void history_pop(void);
rule_t *initrule(void);
int is_idle(void);
void run(void);
void setup(void);
void update_screen_info();
void usage(int exit_status);
void hide_window();
l_node *most_important(list * l);
void draw_win(void);
void hide_win(void);
void move_all_to_history(void);
void print_version(void);

/* show warning notification */
void warn(const char *text, int urg);

void init_shortcut(keyboard_shortcut * shortcut);
KeySym string_to_mask(char *str);

static int GrabXErrorHandler(Display * display, XErrorEvent * e)
{
        dunst_grab_errored = True;
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
        dunst_grab_errored = False;

        XFlush(dc->dpy);
        XSetErrorHandler(GrabXErrorHandler);
}

static int tear_down_error_handler(void)
{
        XFlush(dc->dpy);
        XSync(dc->dpy, False);
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
                         True, GrabModeAsync, GrabModeAsync);

        if (tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab key \"%s\"\n", ks->str);
                ks->is_valid = False;
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

        n->appname = "dunst";
        n->summary = strdup(text);
        if (n->summary == NULL)
                die("Unable to allocate memory", EXIT_FAILURE);
        n->body = "";
        n->icon = "";
        n->timeout = 0;
        n->urgency = urg;
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

void print_rule(rule_t * r)
{
        if (r == NULL)
                return;

        dunst_printf(DEBUG, "%s %s %s %s %s %d %d %s %s %s\n",
                     r->name,
                     r->appname,
                     r->summary,
                     r->body,
                     r->icon, r->timeout, r->urgency, r->fg, r->bg, r->format);
}

void print_rules(void)
{
        dunst_printf(DEBUG, "current rules:\n");
        if (l_is_empty(rules)) {
                dunst_printf(DEBUG, "no rules present\n");
                return;
        }
        for (l_node * iter = rules->head; iter; iter = iter->next) {
                print_rule((rule_t *) iter->data);
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
                        dunst_printf(DEBUG, "matched rule: %s\n", r->name);
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
                n = (notification *) to_move->data;
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

        strtrim_end(source);

        if (!source || strlen(source) == 0)
                return 0;

        char *eol = source;

        while (True) {
                if (*eol == '\0')
                        return 1;
                if (*eol == '\n') {
                        *eol = '\0';
                        return 1 + do_word_wrap(eol + 1, max_width);
                }
                if (*eol == '\\' && *(eol+1) == 'n') {
                        *eol = ' ';
                        *(eol+1) = '\0';
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
        strtrim_end(n->msg);
        char *msg = n->msg;
        while(isspace(*msg))
                msg++;

        if (!n->draw_txt_buf.txt) {
                int dup_len = strlen(" () ") + digit_count(INT_MAX);
                int msg_len = strlen(msg) + 2;       /* 2 == surrounding spaces */
                int age_len = strlen(" ( h 60m 60s old ) ")
                    + digit_count(INT_MAX);     /* could be INT_MAX hours */
                int line_length = dup_len + msg_len + age_len;

                line_length += sizeof(" ( more ) ") + digit_count(INT_MAX);

                n->draw_txt_buf.txt = calloc(line_length, sizeof(char));
                n->draw_txt_buf.bufsize = line_length;
        }

        char *buf = n->draw_txt_buf.txt;
        int bufsize = n->draw_txt_buf.bufsize;
        char *next = buf;

        memset(buf, '\0', bufsize);

        /* print dup_count */
        if (n->dup_count > 0) {
                snprintf(next, bufsize - strlen(buf), "(%d) ", n->dup_count);
                next = buf + strlen(buf);
        }

        /* print msg */
        strncat(buf, msg, bufsize - strlen(buf));
        next = buf + strlen(buf);

        /* print age */
        int hours, minutes, seconds;
        time_t t_delta = now - n->timestamp;

        if (show_age_threshold >= 0 && t_delta >= show_age_threshold) {
                hours = t_delta / 3600;
                minutes = t_delta / 60 % 60;
                seconds = t_delta % 60;

                if (hours > 0) {
                        snprintf(next, bufsize - strlen(buf),
                                 " (%dh %dm %ds old)", hours, minutes, seconds);
                } else if (minutes > 0) {
                        snprintf(next, bufsize - strlen(buf), " (%dm %ds old)",
                                 minutes, seconds);
                } else {
                        snprintf(next, bufsize - strlen(buf), " (%ds old)",
                                 seconds);
                }
        }

        n->draw_txt_buf.line_count =
            do_word_wrap(n->draw_txt_buf.txt, max_width);
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
        switch (align) {
        case left:
                return 0;
        case center:
                return (line_width - text_width) / 2;
        case right:
                return line_width - text_width;
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
                width = geometry.w;
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

        if (separator_enabled) {
                line_cnt += l_length(displayed_notifications) - 1;
                if (indicate_hidden && !l_is_empty(notification_queue))
                        line_cnt++;
        }

        /* if we have a dynamic width, calculate the actual width */
        if (width == 0) {
                for (l_node * iter = displayed_notifications->head; iter;
                     iter = iter->next) {
                        notification *n = (notification *) iter->data;
                        for (int i = 0; i < n->draw_txt_buf.line_count; i++) {
                                char *line =
                                    draw_txt_get_line(&n->draw_txt_buf, i+1);
                                assert(line != NULL);
                                width = MAX(width, textw(dc, line));
                        }
                }
        }


        /* calculate height */
        if (geometry.h == 0) {
                height = line_cnt * line_height;
        } else {
                height = MAX(geometry.h, (line_cnt * line_height));
        }

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
                            (notification *) displayed_notifications->head;
                        print_to =
                            draw_txt_get_line(&n->draw_txt_buf,
                                              n->draw_txt_buf.line_count);
                        for (; *print_to != '\0'; print_to++) ;
                }

                snprintf(print_to, x_more_len, "(%d more)", more);
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
                        drawrect(dc, 0, 0, width, line_height, True,
                                 n->colors->BG);

                        dc->x = calculate_x_offset(width, textw(dc, line));
                        dc->y += (line_height - font_h) / 2;
                        drawtextn(dc, line, strlen(line), n->colors);
                        dc->y += line_height - ((line_height - font_h) / 2);
                }

                /* draw separator */
                if (separator_enabled && line_cnt > 1) {
                        dc->x = 0;
                        drawrect(dc, 0, 0, width, line_height, True, n->colors->BG);

                        double color;
                        if (sep_color == AUTO)
                                color = calculate_foreground_color(n->colors->BG);
                        else
                                color = n->colors->FG;

                        int new_y = dc->y + line_height;
                        int sep_height = line_height * separator_height;
                        sep_height = sep_height < 1 ? 1 : sep_height;
                        int sep_width = width * separator_width;
                        dc->y = dc->y + (line_height - sep_height) / 2;
                        dc->x = (width - sep_width) / 2;
                        drawrect(dc, 0, 0, sep_width, sep_height, True, color);
                        dc->y = new_y;
                }
        }

        /* draw x_more */
        if (x_more.txt) {
                dc->x = 0;
                drawrect(dc, 0, 0, width, line_height, True, last_color->BG);
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
        XResizeWindow(dc->dpy, win, width, height);
        XMoveWindow(dc->dpy, win, x, y);
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
                        if (ev.xexpose.count == 0)
                                draw_win();
                        mapdc(dc, win, scr.dim.w, font_h);
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
        data->redisplayed = True;
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
        n->format = format;

        apply_rules(n);

        n->msg = string_replace("%a", n->appname, strdup(n->format));
        n->msg = string_replace("%s", n->summary, n->msg);
        n->msg = string_replace("%i", n->icon, n->msg);
        n->msg = string_replace("%I", basename(n->icon), n->msg);
        n->msg = string_replace("%b", n->body, n->msg);

        n->msg = fix_markup(n->msg);

        n->dup_count = 0;
        n->draw_txt_buf.txt = NULL;

        /* check if n is a duplicate */
        for (l_node * iter = notification_queue->head; iter; iter = iter->next) {
                notification *orig = (notification *) iter->data;
                if (strcmp(orig->msg, n->msg) == 0) {
                        orig->dup_count++;
                        free_notification(n);
                        return orig->id;
                }
        }

        for (l_node * iter = displayed_notifications->head; iter;
             iter = iter->next) {
                notification *orig = (notification *) iter->data;
                if (strcmp(orig->msg, n->msg) == 0) {
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

        n->redisplayed = False;

        dunst_printf(MSG, "%s\n", n->msg);
        dunst_printf(INFO,
                     "{\n  appname: %s\n  summary: %s\n  body: %s\n  icon: %s\n  urgency: %d\n  timeout: %d\n}",
                     n->appname, n->summary, n->body, n->icon,
                     n->urgency, n->timeout);

        if (id == 0) {
                n->id = ++next_notification_id;
        } else {
                close_notification_by_id(id, -1);
        }

        if(strlen(n->msg) == 0) {
                close_notification(n, 2);
                printf("skipping notification: %s %s\n", n->body, n->summary);
        } else {
                l_push(notification_queue, n);
        }

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
                ks->is_valid = False;
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
                strtrim_end(mod);
                ks->mask = ks->mask | string_to_mask(mod);
        }
        strtrim_end(str);

        ks->sym = XStringToKeysym(str);
        ks->code = XKeysymToKeycode(dc->dpy, ks->sym);

        if (ks->sym == NoSymbol || ks->code == NoSymbol) {
                fprintf(stderr, "Warning: Unknown keyboard shortcut: %s\n",
                        ks->str);
                ks->is_valid = False;
        } else {
                ks->is_valid = True;
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

int is_idle(void)
{
        XScreenSaverQueryInfo(dc->dpy, DefaultRootWindow(dc->dpy),
                              screensaver_info);
        if (idle_threshold == 0) {
                return False;
        }
        return screensaver_info->idle / 1000 > idle_threshold;
}

void run(void)
{
        while (True) {
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
        visible = False;
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
            XInternAtom(dc->dpy, "_NET_ACTIVE_WINDOW", False);

        XGetWindowProperty(dc->dpy, root, netactivewindow, 0L,
                           sizeof(Window), False, XA_WINDOW,
                           &type, &format, &nitems, &bytes_after, &prop_return);
        if (prop_return) {
                focused = *(Window *) prop_return;
                XFree(prop_return);
        }

        return focused;
}

int select_screen(XineramaScreenInfo * info, int info_len)
{
        if (f_mode == FOLLOW_NONE) {
                return scr.scr;

        } else {
                int x, y;
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

void update_screen_info()
{
#ifdef XINERAMA
        int n;
        int screen = scr.scr;
        XineramaScreenInfo *info;
        if ((info = XineramaQueryScreens(dc->dpy, &n))) {
                screen = select_screen(info, n);
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
        Window root;
        XSetWindowAttributes wa;

        notification_queue = l_init();
        notification_history = l_init();
        displayed_notifications = l_init();
        if (scr.scr < 0) {
                scr.scr = DefaultScreen(dc->dpy);
        }
        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));

        utf8 = XInternAtom(dc->dpy, "UTF8_STRING", False);

        /* menu geometry */
        font_h = dc->font.height + FONT_HEIGHT_BORDER;

        update_screen_info();

        /* menu window */
        wa.override_redirect = True;
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

        update_screen_info();

        XMapRaised(dc->dpy, win);

        setup_error_handler();
        XGrabButton(dc->dpy, AnyButton, AnyModifier, win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        if (tear_down_error_handler()) {
                fprintf(stderr, "Unable to grab mouse button(s)\n");
        }

        XFlush(dc->dpy);
        draw_win();
        visible = True;
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

void parse_cmdline(int argc, char *argv[])
{
        int c;
        while (1) {
                static struct option long_options[] = {
                        {"help", no_argument, NULL, 'h'},
                        {"fn", required_argument, NULL, 'F'},
                        {"nb", required_argument, NULL, 'n'},
                        {"nf", required_argument, NULL, 'N'},
                        {"lb", required_argument, NULL, 'l'},
                        {"lf", required_argument, NULL, 'L'},
                        {"cb", required_argument, NULL, 'c'},
                        {"cf", required_argument, NULL, 'C'},
                        {"to", required_argument, NULL, 't'},
                        {"lto", required_argument, NULL, '0'},
                        {"nto", required_argument, NULL, '1'},
                        {"cto", required_argument, NULL, '2'},
                        {"format", required_argument, NULL, 'f'},
                        {"key", required_argument, NULL, 'k'},
                        {"history_key", required_argument, NULL, 'K'},
                        {"all_key", required_argument, NULL, 'A'},
                        {"geometry", required_argument, NULL, 'g'},
                        {"config", required_argument, NULL, 'r'},
                        {"mod", required_argument, NULL, 'M'},
                        {"mon", required_argument, NULL, 'm'},
                        {"ns", no_argument, NULL, 'x'},
                        {"follow", required_argument, NULL, 'o'},
                        {"line_height", required_argument, NULL, 'H'},
                        {"lh", required_argument, NULL, 'H'},
                        {"version", no_argument, NULL, 'v'},
                        {0, 0, 0, 0}
                };

                int option_index = 0;

                c = getopt_long_only(argc, argv, "bhsv", long_options,
                                     &option_index);

                if (c == -1) {
                        break;
                }

                KeySym mod = 0;
                switch (c) {
                case 0:
                        break;
                case 'h':
                        usage(EXIT_SUCCESS);
                        break;
                case 'F':
                        font = optarg;
                        break;
                case 'n':
                        normbgcolor = optarg;
                        break;
                case 'N':
                        normfgcolor = optarg;
                        break;
                case 'l':
                        lowbgcolor = optarg;
                        break;
                case 'L':
                        lowfgcolor = optarg;
                        break;
                case 'c':
                        critbgcolor = optarg;
                        break;
                case 'C':
                        critfgcolor = optarg;
                        break;
                case 't':
                        timeouts[0] = atoi(optarg);
                        timeouts[1] = timeouts[0];
                        break;
                case '0':
                        timeouts[0] = atoi(optarg);
                        break;
                case '1':
                        timeouts[1] = atoi(optarg);
                        break;
                case '2':
                        timeouts[2] = atoi(optarg);
                        break;
                case 'm':
                        scr.scr = atoi(optarg);
                        break;
                case 'f':
                        format = optarg;
                        break;
                case 'M':
                        deprecated_mod = True;
                        mod = string_to_mask(optarg);
                        close_ks.mask = mod;
                        close_all_ks.mask = mod;
                        history_ks.mask = mod;
                        break;
                case 'k':
                        close_ks.str = optarg;
                        break;
                case 'K':
                        history_ks.str = optarg;
                        break;
                case 'A':
                        close_all_ks.str = optarg;
                        break;
                case 'g':
                        geom = optarg;
                        break;
                case 's':
                        sort = True;
                        break;
                case 'r':
                        /* this option is parsed elsewhere. This is just to supress
                         * error message */
                        break;
                case 'x':
                        sort = False;
                        break;
                case 'o':
                        parse_follow_mode(optarg);
                        break;
                case 'H':
                        line_height = atoi(optarg);
                        break;
                case 'v':
                        print_version();
                        break;
                default:
                        usage(EXIT_FAILURE);
                        break;
                }
        }
}

#ifndef STATIC_CONFIG
static int dunst_ini_get_boolean(const char *value)
{
        switch (value[0]) {
        case 'y':
        case 'Y':
        case 't':
        case 'T':
        case '1':
                return True;
        case 'n':
        case 'N':
        case 'f':
        case 'F':
        case '0':
                return False;
        default:
                return False;
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

        /* rule not found in rules, create new one */
        dunst_printf(DEBUG, "adding rule %s\n", section);

        rule = initrule();
        rule->name = strdup(section);

        l_push(rules, rule);

        return rule;
}

static char *dunst_ini_get_string(const char *value)
{
        char *s;

        if (value[0] == '"')
                s = strdup(value + 1);
        else
                s = strdup(value);

        if (s[strlen(s) - 1] == '"')
                s[strlen(s) - 1] = '\0';

        return s;
}

static int
dunst_ini_handle(void *user_data, const char *section,
                 const char *name, const char *value)
{
        if (strcmp(section, "global") == 0) {
                if (strcmp(name, "font") == 0)
                        font = dunst_ini_get_string(value);
                else if (strcmp(name, "format") == 0)
                        format = dunst_ini_get_string(value);
                else if (strcmp(name, "sort") == 0)
                        sort = dunst_ini_get_boolean(value);
                else if (strcmp(name, "indicate_hidden") == 0)
                        indicate_hidden = dunst_ini_get_boolean(value);
                else if (strcmp(name, "word_wrap") == 0)
                        word_wrap = dunst_ini_get_boolean(value);
                else if (strcmp(name, "idle_threshold") == 0)
                        idle_threshold = atoi(value);
                else if (strcmp(name, "monitor") == 0)
                        scr.scr = atoi(value);
                else if (strcmp(name, "follow") == 0) {
                        char *c = dunst_ini_get_string(value);
                        parse_follow_mode(c);
                        free(c);
                } else if (strcmp(name, "geometry") == 0)
                        geom = dunst_ini_get_string(value);
                else if (strcmp(name, "line_height") == 0)
                        line_height = atoi(value);
                else if (strcmp(name, "modifier") == 0) {
                        deprecated_dunstrc_shortcuts = True;
                        char *c = dunst_ini_get_string(value);
                        KeySym mod = string_to_mask(c);
                        free(c);
                        close_ks.mask = mod;
                        close_all_ks.mask = mod;
                        history_ks.mask = mod;
                } else if (strcmp(name, "key") == 0) {
                        close_ks.str = dunst_ini_get_string(value);
                } else if (strcmp(name, "all_key") == 0) {
                        close_all_ks.str = dunst_ini_get_string(value);
                } else if (strcmp(name, "history_key") == 0) {
                        history_ks.str = dunst_ini_get_string(value);
                } else if (strcmp(name, "alignment") == 0) {
                        if (strcmp(value, "left") == 0)
                                align = left;
                        else if (strcmp(value, "center") == 0)
                                align = center;
                        else if (strcmp(value, "right") == 0)
                                align = right;
                        /* FIXME warning on unknown alignment */
                } else if (strcmp(name, "show_age_threshold") == 0)
                        show_age_threshold = atoi(value);
                else if (strcmp(name, "sticky_history") == 0)
                        sticky_history = dunst_ini_get_boolean(value);


        } else if (strcmp(section, "separator") == 0) {
                if (strcmp(name, "enable") == 0)
                        separator_enabled = dunst_ini_get_boolean(value);
                if (strcmp(name, "width") == 0)
                        separator_width = strtod(value, NULL);
                if (strcmp(name, "height") == 0)
                        separator_height = strtod(value, NULL);
                if (strcmp(name, "color") == 0) {
                        char *str = dunst_ini_get_string(value);
                        if (strcmp(str, "auto") == 0)
                                sep_color = AUTO;
                        else if (strcmp(str, "foreground") == 0)
                                sep_color = FOREGROUND;
                        else
                                fprintf(stderr, "Warning: Unknown separator color\n");
                        free(str);
                }
        } else if (strcmp(section, "urgency_low") == 0) {
                if (strcmp(name, "background") == 0)
                        lowbgcolor = dunst_ini_get_string(value);
                else if (strcmp(name, "foreground") == 0)
                        lowfgcolor = dunst_ini_get_string(value);
                else if (strcmp(name, "timeout") == 0)
                        timeouts[LOW] = atoi(value);

        } else if (strcmp(section, "urgency_normal") == 0) {
                if (strcmp(name, "background") == 0)
                        normbgcolor = dunst_ini_get_string(value);
                else if (strcmp(name, "foreground") == 0)
                        normfgcolor = dunst_ini_get_string(value);
                else if (strcmp(name, "timeout") == 0)
                        timeouts[NORM] = atoi(value);

        } else if (strcmp(section, "urgency_critical") == 0) {
                if (strcmp(name, "background") == 0)
                        critbgcolor = dunst_ini_get_string(value);
                else if (strcmp(name, "foreground") == 0)
                        critfgcolor = dunst_ini_get_string(value);
                else if (strcmp(name, "timeout") == 0)
                        timeouts[CRIT] = atoi(value);

        } else if (strcmp(section, "shortcuts") == 0) {
                if (strcmp(name, "close") == 0)
                        close_ks.str = dunst_ini_get_string(value);
                else if (strcmp(name, "close_all") == 0)
                        close_all_ks.str = dunst_ini_get_string(value);
                else if (strcmp(name, "history") == 0)
                        history_ks.str = dunst_ini_get_string(value);
        } else {

                rule_t *current_rule = dunst_rules_find_or_create(section);

                if (strcmp(name, "appname") == 0)
                        current_rule->appname = dunst_ini_get_string(value);
                else if (strcmp(name, "summary") == 0)
                        current_rule->summary = dunst_ini_get_string(value);
                else if (strcmp(name, "body") == 0)
                        current_rule->body = dunst_ini_get_string(value);
                else if (strcmp(name, "icon") == 0)
                        current_rule->icon = dunst_ini_get_string(value);
                else if (strcmp(name, "timeout") == 0)
                        current_rule->timeout = atoi(value);
                else if (strcmp(name, "urgency") == 0) {
                        const char *urg = value;

                        if (strcmp(urg, "low") == 0)
                                current_rule->urgency = LOW;
                        else if (strcmp(urg, "normal") == 0)
                                current_rule->urgency = NORM;
                        else if (strcmp(urg, "critical") == 0)
                                current_rule->urgency = CRIT;
                        else
                                fprintf(stderr,
                                        "unknown urgency: %s, ignoring\n", urg);
                } else if (strcmp(name, "foreground") == 0)
                        current_rule->fg = dunst_ini_get_string(value);
                else if (strcmp(name, "background") == 0)
                        current_rule->bg = dunst_ini_get_string(value);
                else if (strcmp(name, "format") == 0)
                        current_rule->format = dunst_ini_get_string(value);
        }

        return 1;
}

void parse_dunstrc(char *cmdline_config_path)
{

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

        if (ini_parse_file(config_file, dunst_ini_handle, NULL) < 0) {
                puts("dunstrc could not be parsed -> skipping\n");
        }

        fclose(config_file);
        xdgWipeHandle(&xdg);

        print_rules();
}
#endif                          /* STATIC_CONFIG */

char *parse_cmdline_for_config_file(int argc, char *argv[])
{
        for (int i = 0; i < argc; i++) {
                if (strstr(argv[i], "-config") != 0) {
                        if (i + 1 == argc) {
                                printf
                                    ("Invalid commandline: -config needs argument\n");
                        }
                        return argv[++i];
                }
        }
        return NULL;
}

int main(int argc, char *argv[])
{
        now = time(&now);

        rules = l_init();
        for (int i = 0; i < LENGTH(default_rules); i++) {
                l_push(rules, &default_rules[i]);
        }
        scr.scr = monitor;
#ifndef STATIC_CONFIG
        char *cmdline_config_path;
        cmdline_config_path = parse_cmdline_for_config_file(argc, argv);
        parse_dunstrc(cmdline_config_path);
#endif
        parse_cmdline(argc, argv);
        dc = initdc();

        init_shortcut(&close_ks);
        init_shortcut(&close_all_ks);
        init_shortcut(&history_ks);

        grab_key(&close_ks);
        ungrab_key(&close_ks);
        grab_key(&close_all_ks);
        ungrab_key(&close_all_ks);
        grab_key(&history_ks);
        ungrab_key(&history_ks);

        geometry.mask = XParseGeometry(geom,
                                       &geometry.x, &geometry.y,
                                       &geometry.w, &geometry.h);

        screensaver_info = XScreenSaverAllocInfo();

        initdbus();
        initfont(dc, font);
        colors[LOW] = initcolor(dc, lowfgcolor, lowbgcolor);
        colors[NORM] = initcolor(dc, normfgcolor, normbgcolor);
        colors[CRIT] = initcolor(dc, critfgcolor, critbgcolor);

        color_strings[ColFG][LOW] = lowfgcolor;
        color_strings[ColFG][NORM] = normfgcolor;
        color_strings[ColFG][CRIT] = critfgcolor;

        color_strings[ColBG][LOW] = lowbgcolor;
        color_strings[ColBG][NORM] = normbgcolor;
        color_strings[ColBG][CRIT] = critbgcolor;
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
        fputs
            ("usage: dunst [-h/--help] [-v] [-geometry geom] [-lh height] [-fn font] [-format fmt]\n[-nb color] [-nf color] [-lb color] [-lf color] [-cb color] [ -cf color]\n[-to secs] [-lto secs] [-cto secs] [-nto secs] [-key key] [-history_key key] [-all_key key] [-mon n]  [-follow none/mouse/keyboard] [-config dunstrc]\n",
             stderr);
        exit(exit_status);
}

void print_version(void)
{
        printf("Dunst - Dmenuish notification daemon version: %s\n", VERSION);
        exit(EXIT_SUCCESS);
}

/* vim: set ts=8 sw=8 tw=0: */

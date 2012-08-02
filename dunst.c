/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE
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

enum follow_mode { FOLLOW_NONE, FOLLOW_MOUSE, FOLLOW_KEYBOARD };

/* structs */
typedef struct _screen_info {
        int scr;
        dimension_t dim;
} screen_info;

typedef struct _notification_buffer {
        char txt[BUFSIZ];
        notification *n;
        int x_offset;
} notification_buffer;

/* global variables */
char *font = "-*-terminus-medium-r-*-*-16-*-*-*-*-*-*-*";
char *normbgcolor = "#1793D1";
char *normfgcolor = "#DDDDDD";
char *critbgcolor = "#ffaaaa";
char *critfgcolor = "#000000";
char *lowbgcolor = "#aaaaff";
char *lowfgcolor = "#000000";
char *format = "%s %b";         /* default format */
int timeouts[] = { 10, 10, 0 }; /* low, normal, critical */

char *geom = "0x0";             /* geometry */
int height_limit;
int sort = True;                /* sort messages by urgency */
int indicate_hidden = True;     /* show count of hidden messages */
int idle_threshold = 0;
int show_age_threshold = -1;
enum alignment align = left;
int sticky_history = True;

int verbosity = 0;

list *rules = NULL;
/* index of colors fit to urgency level */
static ColorSet *colors[3];
static const char *color_strings[2][3];
static Atom utf8;
static DC *dc;
static Window win;
static time_t now;
static int visible = False;
static keyboard_shortcut close_ks = {.str = NULL,.code = 0,.sym =
            NoSymbol,.mask = 0,.is_valid = False
};

static keyboard_shortcut close_all_ks = {.str = NULL,.code = 0,.sym =
            NoSymbol,.mask = 0,.is_valid = False
};

static keyboard_shortcut history_ks = {.str = NULL,.code = 0,.sym =
            NoSymbol,.mask = 0,.is_valid = False
};

static screen_info scr;
static dimension_t geometry;
static XScreenSaverInfo *screensaver_info;
static int font_h;

static enum follow_mode f_mode = FOLLOW_NONE;

int next_notification_id = 1;

int depricated_mod = False;
int depricated_dunstrc_shortcuts = False;

/* notification lists */
list *notification_queue = NULL;        /* all new notifications get into here */
list *displayed_notifications = NULL;   /* currently displayed notifications */
list *notification_history = NULL;      /* history of displayed notifications */

/* misc funtions */
void apply_rules(notification * n);
void check_timeouts(void);
void draw_notifications(void);
void dunst_printf(int level, const char *fmt, ...);
char *fix_markup(char *str);
void handle_mouse_click(XEvent ev);
void handleXEvents(void);
void history_pop(void);
rule_t *initrule(void);
int is_idle(void);
char *string_replace(const char *needle, const char *replacement,
                     char *haystack);
char *strtrim(char *str);
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
void warn(const char * text, int urg);

void init_shortcut(keyboard_shortcut * shortcut);
KeySym string_to_mask(char *str);

void die(char *text, int exit_value)
{
        fputs(text, stderr);
        exit(exit_value);
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

void draw_win(void)
{
        int width, x, y, height;
        unsigned int len = l_length(displayed_notifications);
        notification_buffer *n_buf;
        dc->x = 0;
        dc->y = 0;
        dc->h = 0;

        update_screen_info();

        /* calculate height */
        if (geometry.h == 0) {
                height = len;
        } else {
                height = MIN(geometry.h, len);
        }

        if (indicate_hidden && geometry.h != 1
            && !l_is_empty(notification_queue)) {
                height++;
        }

        /* initialize and fill buffers */
        n_buf = calloc(height, sizeof(notification_buffer));

        l_node *iter;
        int i;
        for (i = 0, iter = displayed_notifications->head; i < height; i++) {
                memset(n_buf[i].txt, '\0', BUFSIZ);
                n_buf[i].x_offset = 0;
                if (iter) {
                        n_buf[i].n = (notification *) iter->data;
                        if (n_buf[i].n->dup_count > 0) {
                                snprintf(n_buf[i].txt, BUFSIZ, "(%d) %s",
                                         n_buf[i].n->dup_count,
                                         n_buf[i].n->msg);
                        } else {
                                strncpy(n_buf[i].txt, n_buf[i].n->msg, BUFSIZ);
                        }
                        iter = iter->next;
                } else {
                        n_buf[i].n = NULL;
                }
        }

        /* add message age to text */
        if (show_age_threshold >= 0) {
                for (i = 0; i < height; i++) {

                        time_t t_delta;
                        int hours, minutes, seconds;
                        char *end;

                        if (strlen(n_buf[i].txt) < 1)
                                continue;

                        t_delta = now - n_buf[i].n->timestamp;

                        if (t_delta < show_age_threshold)
                                continue;

                        hours = t_delta / 3600;
                        minutes = (t_delta / 60);
                        seconds = (t_delta % 60);

                        for (end = n_buf[i].txt; *end != '\0'; end++) ;
                        if (hours > 0) {
                                snprintf(end, BUFSIZ - strlen(n_buf[i].txt),
                                         " (%dh %dm %ds old)",
                                         hours, minutes, seconds);
                        } else if (minutes > 0) {
                                snprintf(end, BUFSIZ - strlen(n_buf[i].txt),
                                         " (%dm %ds old)", minutes, seconds);
                        } else {
                                snprintf(end, BUFSIZ - strlen(n_buf[i].txt),
                                         " (%ds old)", seconds);
                        }

                }
        }

        /* add "(x more)" */
        if (indicate_hidden && geometry.h != 1
            && !l_is_empty(notification_queue)) {
                snprintf(n_buf[height - 1].txt, BUFSIZ, "(%d more)",
                         l_length(notification_queue));
                /* give this buffer the most important notification in order
                 * to set the colors to it
                 */
                n_buf[height - 1].n = most_important(notification_queue)->data;

        } else if (indicate_hidden && !l_is_empty(notification_queue)) {
                /* append "(x more)" message to notification text */
                char *begin;
                for (begin = n_buf[0].txt; *begin != '\0'; begin++) ;
                snprintf(begin, BUFSIZ - strlen(n_buf[0].txt),
                         " (%d more)", l_length(notification_queue));

        }

        /* calculate width */
        if (geometry.mask & WidthValue && geometry.w == 0) {
                /* dynamic width */
                width = 0;
                for (i = 0; i < height; i++) {
                        width = MAX(width, textw(dc, n_buf[i].txt));
                }
        } else if (geometry.mask & WidthValue) {
                /* fixed width */
                width = geometry.w;
        } else {
                /* across the screen */
                width = scr.dim.w;
        }

        /* calculate offsets for alignment */
        for (int i = 0; i < height; i++) {
                if (strlen(n_buf[i].txt) < 1)
                        continue;

                if (align == right) {
                        n_buf[i].x_offset = width - textw(dc, n_buf[i].txt);
                } else if (align == center) {
                        n_buf[i].x_offset = (width - textw(dc,
                                                           n_buf[i].txt)) / 2;
                }
        }

        /* calculate window position */
        if (geometry.mask & XNegative) {
                x = (scr.dim.x + (scr.dim.w - width)) + geometry.x;
        } else {
                x = scr.dim.x + geometry.x;
        }

        if (geometry.mask & YNegative) {
                y = (scr.dim.h + geometry.y) - height * font_h;
        } else {
                y = 0 + geometry.y;
        }

        /* resize window and draw background */
        if (width == 0) {
                printf("Warning: width == 0\n");
                goto draw_win_cleanup;
        }

        if (height == 0) {
                printf("Warning: height == 0\n");
                goto draw_win_cleanup;
        }

        if (font_h == 0) {
                printf("Warning: font_h == 0\n");
                goto draw_win_cleanup;
        }
        resizedc(dc, width, height * font_h);
        XResizeWindow(dc->dpy, win, width, height * font_h);
        drawrect(dc, 0, 0, width, height * font_h, True, colors[NORM]->BG);

        /* draw buffers */
        for (int i = 0; i < height; i++) {
                if (strlen(n_buf[i].txt) > 0) {
                        notification *n;
                        n = n_buf[i].n;
                        dc->x = 0;
                        drawrect(dc, 0, 0, width, font_h, True, n->colors->BG);
                        dc->x = n_buf[i].x_offset;
                        drawtext(dc, n_buf[i].txt, n->colors);
                        dc->y += font_h;
                }
        }

        /* move and map window */
        XMoveWindow(dc->dpy, win, x, y);
        mapdc(dc, win, width, height * font_h);

 draw_win_cleanup:
        /* cleanup */
        free(n_buf);
}

void dunst_printf(int level, const char *fmt, ...)
{
        va_list ap;

        if (level > verbosity) {
                return;
        }
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
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
        l_node *iter = displayed_notifications->head;
        notification *n;
        if (ev.xbutton.button == Button3) {
                move_all_to_history();
        } else if (ev.xbutton.button == Button1) {
                int i = ev.xbutton.y / font_h;
                for (; i > 0; i--) {
                        /* if the user clicks on the "(x more)" message,
                         * keep iter at the last displayed message and
                         * remove that instead
                         */
                        if (iter->next) {
                                iter = iter->next;
                        }
                }
                n = (notification *) iter->data;
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
        n->msg = strtrim(n->msg);

        n->dup_count = 0;

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

        l_push(notification_queue, n);

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
                strtrim(mod);
                ks->mask = ks->mask | string_to_mask(mod);
        }
        strtrim(str);

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

char *string_replace(const char *needle, const char *replacement,
                     char *haystack)
{
        char *tmp, *start;
        int size;
        start = strstr(haystack, needle);
        if (start == NULL) {
                return haystack;
        }

        size = (strlen(haystack) - strlen(needle)) + strlen(replacement) + 1;
        tmp = calloc(sizeof(char), size);
        memset(tmp, '\0', size);

        strncpy(tmp, haystack, start - haystack);
        tmp[start - haystack] = '\0';

        sprintf(tmp + strlen(tmp), "%s%s", replacement, start + strlen(needle));
        free(haystack);

        if (strstr(tmp, needle)) {
                return string_replace(needle, replacement, tmp);
        } else {
                return tmp;
        }
}

char *strtrim(char *str)
{
        char *end;
        while (isspace(*str))
                str++;

        end = str + strlen(str) - 1;
        while (isspace(*end)) {
                *end = '\0';
                end--;
        }

        return str;

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
        Window root;
        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));

        if (close_ks.is_valid)
                XUngrabKey(dc->dpy, close_ks.code, close_ks.mask, root);

        if (close_all_ks.is_valid)
                XUngrabKey(dc->dpy, close_all_ks.code, close_all_ks.mask, root);

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
        int screen = 0;
        XineramaScreenInfo *info;
#endif
#ifdef XINERAMA
        if ((info = XineramaQueryScreens(dc->dpy, &n))) {
                screen = select_screen(info, n);
                if (screen >= n) {
                        fprintf(stderr, "Monitor %d not found\n", screen);
                        exit(EXIT_FAILURE);
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

        /* grab keys */
        if (history_ks.is_valid)
                XGrabKey(dc->dpy, history_ks.code, history_ks.mask, root,
                         True, GrabModeAsync, GrabModeAsync);
}

void map_win(void)
{
        /* window is already mapped or there's nothing to show */
        if (visible || l_is_empty(displayed_notifications)) {
                return;
        }

        Window root;
        root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));

        if (close_ks.is_valid)
                XGrabKey(dc->dpy, close_ks.code, close_ks.mask, root, True,
                         GrabModeAsync, GrabModeAsync);

        if (close_all_ks.is_valid)
                XGrabKey(dc->dpy, close_all_ks.code, close_all_ks.mask, root,
                         True, GrabModeAsync, GrabModeAsync);

        update_screen_info();

        XMapRaised(dc->dpy, win);
        XGrabButton(dc->dpy, AnyButton, AnyModifier, win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
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
                        depricated_mod = True;
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
                case 'A':
                        close_all_ks.str = optarg;
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
                case 'v':
                        print_version();
                        break;
                default:
                        usage(EXIT_FAILURE);
                        break;
                }
        }
}

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
                else if (strcmp(name, "idle_threshold") == 0)
                        idle_threshold = atoi(value);
                else if (strcmp(name, "monitor") == 0)
                        scr.scr = atoi(value);
                else if (strcmp(name, "follow") == 0)
                        parse_follow_mode(dunst_ini_get_string(value));
                else if (strcmp(name, "geometry") == 0)
                        geom = dunst_ini_get_string(value);
                else if (strcmp(name, "modifier") == 0) {
                        depricated_dunstrc_shortcuts = True;
                        KeySym mod =
                            string_to_mask(dunst_ini_get_string(value));

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
        rules = l_init();

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
        char *cmdline_config_path;
        now = time(&now);

        cmdline_config_path = parse_cmdline_for_config_file(argc, argv);

        parse_dunstrc(cmdline_config_path);
        parse_cmdline(argc, argv);
        dc = initdc();

        init_shortcut(&close_ks);
        init_shortcut(&close_all_ks);
        init_shortcut(&history_ks);

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


        if (depricated_mod)
                warn("-mod is depricated. Use \"-key mod+key\" instead\n", CRIT);
        if (depricated_dunstrc_shortcuts)
                warn("You are using depricated settings. Please update your dunstrc. SEE [shortcuts]",CRIT);
        run();
        return 0;
}

void usage(int exit_status)
{
        fputs
            ("usage: dunst [-h/--help] [-v] [-geometry geom] [-fn font] [-format fmt]\n[-nb color] [-nf color] [-lb color] [-lf color] [-cb color] [ -cf color]\n[-to secs] [-lto secs] [-cto secs] [-nto secs] [-key key] [-history_key key] [-all_key key] [-mon n]  [-follow none/mouse/keyboard] [-config dunstrc]\n",
             stderr);
        exit(exit_status);
}

void print_version(void)
{
        printf("Dunst - Dmenuish notification daemon version: %s\n", VERSION);
        exit(EXIT_SUCCESS);
}

/* vim: set ts=8 sw=8 tw=0: */

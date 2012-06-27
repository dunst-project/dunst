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

/* structs */
typedef struct _screen_info {
        int scr;
        dimension_t dim;
} screen_info;

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
char *key_string = NULL;
char *history_key_string = NULL;
KeySym mask = 0;
int idle_threshold = 0;

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
static KeySym key = NoSymbol;
static KeySym history_key = NoSymbol;
static screen_info scr;
static dimension_t geometry;
static XScreenSaverInfo *screensaver_info;
static int font_h;
static char *config_file;

int next_notification_id = 1;

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
void usage(int exit_status);
void hide_window();
l_node *most_important(list * l);
void draw_win(void);
void hide_win(void);
void move_all_to_history(void);

int cmp_notification(notification * a, notification * b)
{
        if (a->urgency != b->urgency) {
                return a->urgency - b->urgency;
        } else {
                return b->timestamp - a->timestamp;
        }
}

l_node *most_important(list * l)
{

        if (l == NULL || l_is_empty(l)) {
                return NULL;
        }

        if (sort) {
                l_node *iter;
                notification *max;
                l_node *node_max;
                notification *data;

                max = l->head->data;
                node_max = l->head;
                for (iter = l->head; iter; iter = iter->next) {
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
        dunst_printf(DEBUG, "%s %s %s %s %s %d %d %s %s %s\n",
                     r->name,
                     r->appname,
                     r->summary,
                     r->body,
                     r->icon, r->timeout, r->urgency, r->fg, r->bg, r->format);
}

void print_rules(void)
{
        l_node *iter;
        dunst_printf(DEBUG, "current rules:\n");
        if (l_is_empty(rules)) {
                dunst_printf(DEBUG, "no rules present\n");
                return;
        }
        for (iter = rules->head; iter; iter = iter->next) {
                print_rule((rule_t *) iter->data);
        }
}

void apply_rules(notification * n)
{
        l_node *iter;
        if (l_is_empty(rules)) {
                return;
        }

        for (iter = rules->head; iter; iter = iter->next) {
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
                        l_move(displayed_notifications, notification_history,
                               iter);
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
        l_node *iter;
        notification *data;
        char hidden[128];
        ColorSet *hidden_colors = colors[NORM];
        dc->x = 0;
        dc->y = 0;
        dc->h = 0;

        if (geometry.h == 0) {
                height = len;
        } else {
                height = MIN(geometry.h, len);
        }

        if (indicate_hidden && !l_is_empty(notification_queue)) {
                sprintf(hidden, "(%d more)", l_length(notification_queue));
                height++;
        }

        /*
         * calculate the width
         */

        if (geometry.mask & HeightValue) {
                if (geometry.h) {
                        /* code */
                }
        }
        /* defined width */
        if (geometry.mask & WidthValue) {

                /* dynamic width */
                if (geometry.w == 0) {
                        width = 0;

                        for (iter = displayed_notifications->head;
                             iter; iter = iter->next) {
                                data = (notification *) iter->data;
                                width = MAX(width, textw(dc, data->msg));
                        }

                        /* we also have the "(x more)" message into account */
                        if (indicate_hidden
                            && !l_is_empty(displayed_notifications)) {
                                width = MAX(width, textw(dc, hidden));
                        }

                } else {
                        width = geometry.w;
                }
        } else {
                width = scr.dim.w;
        }

        /*
         * calculate window position
         */

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
        resizedc(dc, width, height * font_h);
        XResizeWindow(dc->dpy, win, width, height * font_h);
        drawrect(dc, 0, 0, width, height * font_h, True, colors[NORM]->BG);

        /* draw notifications */
        for (iter = displayed_notifications->head; iter; iter = iter->next) {
                data = (notification *) iter->data;

                drawrect(dc, 0, dc->y, width, font_h, True, data->colors->BG);
                drawtext(dc, data->msg, data->colors);
                dc->y += font_h;

                /* the "(x more)" message should have the same BG-color as
                 * the last displayed_notifications
                 */
                hidden_colors = data->colors;
        }

        /* actually draw the "(x more)" message */
        if (indicate_hidden && !l_is_empty(notification_queue)) {
                drawrect(dc, 0, dc->y, width, font_h, True, hidden_colors->BG);

                drawtext(dc, hidden, hidden_colors);
                dc->y += font_h;
        }

        /* move and map window */
        XMoveWindow(dc->dpy, win, x, y);
        mapdc(dc, win, width, height * font_h);
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
        int i;
        if (ev.xbutton.button == Button3) {
                move_all_to_history();
        } else if (ev.xbutton.button == Button1) {
                i = ev.xbutton.y / font_h;
                for (i = i; i > 0; i--) {
                        /* if the user clicks on the "(x more)" message,
                         * keep iter at the last displayed message and
                         * remove that instead
                         */
                        if (iter->next) {
                                iter = iter->next;
                        }
                }
                l_move(displayed_notifications, notification_history, iter);
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
                        if (XLookupKeysym(&ev.xkey, 0) == key) {
                                if (!l_is_empty(displayed_notifications)) {
                                        l_move(displayed_notifications,
                                               notification_history,
                                               displayed_notifications->head);
                                }
                        }
                        if (XLookupKeysym(&ev.xkey, 0) == history_key) {
                                history_pop();
                        }
                }
        }
}

void move_all_to_history()
{
        l_node *node;

        while (!l_is_empty(displayed_notifications)) {
                node = displayed_notifications->head;
                l_move(displayed_notifications, notification_history, node);
        }
        while (!l_is_empty(notification_queue)) {
                node = notification_queue->head;
                l_move(notification_queue, notification_history, node);
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
        l_move(notification_history, notification_queue, iter);

        if (!visible) {
                map_win();
        }
}

int init_notification(notification * n, int id)
{
        const char *fg = NULL;
        const char *bg = NULL;
        n->format = format;
        apply_rules(n);

        n->msg = string_replace("%a", n->appname, strdup(n->format));
        n->msg = string_replace("%s", n->summary, n->msg);
        n->msg = string_replace("%i", n->icon, n->msg);
        n->msg = string_replace("%I", basename(n->icon), n->msg);
        n->msg = string_replace("%b", n->body, n->msg);

        n->msg = fix_markup(n->msg);
        n->msg = strtrim(n->msg);
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
                close_notification(id);
        }

        l_push(notification_queue, n);

        return n->id;
}

int close_notification(int id)
{
        l_node *iter;

        for (iter = displayed_notifications->head; iter; iter = iter->next) {
                notification *n = (notification *) iter->data;
                if (n->id == id) {
                        l_move(displayed_notifications, notification_history,
                               iter);
                        return True;
                }
        }

        for (iter = notification_queue->head; iter; iter = iter->next) {
                notification *n = (notification *) iter->data;
                if (n->id == id) {
                        l_move(displayed_notifications, notification_history,
                               iter);
                        return True;
                }
        }

        return False;
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
        XUngrabButton(dc->dpy, AnyButton, AnyModifier, win);
        XUnmapWindow(dc->dpy, win);
        XFlush(dc->dpy);
        visible = False;
}

void setup(void)
{
        Window root;
        XSetWindowAttributes wa;
        KeyCode code;
#ifdef XINERAMA
        int n;
        XineramaScreenInfo *info;
#endif

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
#ifdef XINERAMA
        if ((info = XineramaQueryScreens(dc->dpy, &n))) {
                if (scr.scr >= n) {
                        fprintf(stderr, "Monitor %d not found\n", scr.scr);
                        exit(EXIT_FAILURE);
                }
                scr.dim.x = info[scr.scr].x_org;
                scr.dim.y = info[scr.scr].y_org;
                scr.dim.h = info[scr.scr].height;
                scr.dim.w = info[scr.scr].width;
                XFree(info);
        } else
#endif
        {
                scr.dim.x = 0;
                scr.dim.y = 0;
                scr.dim.w = DisplayWidth(dc->dpy, scr.scr);
                scr.dim.h = DisplayHeight(dc->dpy, scr.scr);
        }
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
        if (key != NoSymbol) {
                code = XKeysymToKeycode(dc->dpy, key);
                XGrabKey(dc->dpy, code, mask, root, True, GrabModeAsync,
                         GrabModeAsync);
        }

        if (history_key != NoSymbol) {
                code = XKeysymToKeycode(dc->dpy, history_key);
                XGrabKey(dc->dpy, code, mask, root, True, GrabModeAsync,
                         GrabModeAsync);
        }
}

void map_win(void)
{
        /* window is already mapped or there's nothing to show */
        if (visible || l_is_empty(displayed_notifications)) {
                return;
        }

        XMapRaised(dc->dpy, win);
        XGrabButton(dc->dpy, AnyButton, AnyModifier, win, False,
                    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
        XFlush(dc->dpy);
        draw_win();
        visible = True;
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
                        {"mon", required_argument, NULL, 'm'},
                        {"format", required_argument, NULL, 'f'},
                        {"key", required_argument, NULL, 'k'},
                        {"history_key", required_argument, NULL, 'K'},
                        {"geometry", required_argument, NULL, 'g'},
                        {"config", required_argument, NULL, 'r'},
                        {"mod", required_argument, NULL, 'M'},
                        {"ns", no_argument, NULL, 'x'},
                        {0, 0, 0, 0}
                };

                int option_index = 0;

                c = getopt_long_only(argc, argv, "bhsv", long_options,
                                     &option_index);

                if (c == -1) {
                        break;
                }

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
                case 'k':
                        key = XStringToKeysym(optarg);
                        break;
                case 'K':
                        history_key = XStringToKeysym(optarg);
                        break;
                case 'g':
                        geometry.mask = XParseGeometry(optarg,
                                                       &geometry.x, &geometry.y,
                                                       &geometry.w,
                                                       &geometry.h);
                        break;
                case 'M':
                        if (!strcmp(optarg, "ctrl")) {
                                mask |= ControlMask;
                        } else if (!strcmp(optarg, "mod1")) {
                                mask |= Mod1Mask;
                        } else if (!strcmp(optarg, "mod2")) {
                                mask |= Mod2Mask;
                        } else if (!strcmp(optarg, "mod3")) {
                                mask |= Mod3Mask;
                        } else if (!strcmp(optarg, "mod4")) {
                                mask |= Mod4Mask;
                        } else {
                                fprintf(stderr, "Unable to find mask: %s\n",
                                        optarg);
                                fprintf(stderr,
                                        "See manpage for list of available masks\n");
                        }
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
                case 'v':
                        verbosity++;
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
                else if (strcmp(name, "key") == 0)
                        key_string = dunst_ini_get_string(value);
                else if (strcmp(name, "history_key") == 0)
                        history_key_string = dunst_ini_get_string(value);
                else if (strcmp(name, "idle_threshold") == 0)
                        idle_threshold = atoi(value);
                else if (strcmp(name, "monitor") == 0)
                        scr.scr = atoi(value);
                else if (strcmp(name, "geometry") == 0) {
                        geom = dunst_ini_get_string(value);
                        geometry.mask = XParseGeometry(geom,
                                                       &geometry.x, &geometry.y,
                                                       &geometry.w,
                                                       &geometry.h);
                } else if (strcmp(name, "modifier") == 0) {
                        char *mod = dunst_ini_get_string(value);

                        if (mod == NULL) {
                                mask = 0;
                        } else if (!strcmp(mod, "ctrl")) {
                                mask = ControlMask;
                        } else if (!strcmp(mod, "mod4")) {
                                mask = Mod4Mask;
                        } else if (!strcmp(mod, "mod3")) {
                                mask = Mod3Mask;
                        } else if (!strcmp(mod, "mod2")) {
                                mask = Mod2Mask;
                        } else if (!strcmp(mod, "mod1")) {
                                mask = Mod1Mask;
                        } else {
                                mask = 0;
                        }
                        free(mod);
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

void parse_dunstrc(void)
{

        xdgHandle xdg;
        FILE *config_file;

        xdgInitHandle(&xdg);
        rules = l_init();

        config_file = xdgConfigOpen("dunst/dunstrc", "r", &xdg);
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

void parse_cmdline_for_config_file(int argc, char *argv[])
{
        int i;
        for (i = 0; i < argc; i++) {
                if (strstr(argv[i], "-config") != 0) {
                        if (i + 1 == argc) {
                                printf
                                    ("Invalid commandline: -config needs argument\n");
                        }
                        config_file = argv[++i];
                        return;
                }
        }
}

int main(int argc, char *argv[])
{
        now = time(&now);
        geometry.mask = XParseGeometry(geom,
                                       &geometry.x, &geometry.y,
                                       &geometry.w, &geometry.h);

        parse_cmdline_for_config_file(argc, argv);
        parse_dunstrc();
        parse_cmdline(argc, argv);
        dc = initdc();
        key = key_string ? XStringToKeysym(key_string) : NoSymbol;
        history_key =
            history_key_string ? XStringToKeysym(history_key_string) : NoSymbol;
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

        run();
        return 0;
}

void usage(int exit_status)
{
        fputs
            ("usage: dunst [-h/--help] [-v] [-geometry geom] [-fn font] [-format fmt]\n[-nb color] [-nf color] [-lb color] [-lf color] [-cb color] [ -cf color]\n[-to secs] [-lto secs] [-cto secs] [-nto secs] [-key key] [-history_key key] [-mod modifier] [-mon n] [-config dunstrc]\n",
             stderr);
        exit(exit_status);
}

/* vim: set ts=8 sw=8 tw=0: */

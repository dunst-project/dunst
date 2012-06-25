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
char *lowbgcolor =  "#aaaaff";
char *lowfgcolor = "#000000";
char *format = "%s %b"; /* default format */
int timeouts[] = { 10, 10, 0 }; /* low, normal, critical */
char *geom = "0x0"; /* geometry */
int sort = True; /* sort messages by urgency */
int indicate_hidden = True; /* show count of hidden messages */
char *key_string = NULL;
char *history_key_string = NULL;
KeySym mask = 0;
int idle_threshold = 0;

int verbosity = 0;


rule_t *rules = NULL;
/* index of colors fit to urgency level */
static ColorSet *colors[3];
static const char *color_strings[2][3];
static Atom utf8;
static DC *dc;
static Window win;
static msg_queue_t *msgqueue = NULL;
static msg_queue_t *history = NULL;
static time_t now;
static int visible = False;
static KeySym key = NoSymbol;
static KeySym history_key = NoSymbol;
static screen_info scr;
static dimension_t geometry;
static XScreenSaverInfo *screensaver_info;
static int font_h;
static char *config_file;

/* list functions */
msg_queue_t *add(msg_queue_t *queue, msg_queue_t *msg);
msg_queue_t *delete(msg_queue_t *elem);
msg_queue_t *pop(msg_queue_t *list, msg_queue_t **target);
int list_len(msg_queue_t *list);


/* misc funtions */
void apply_rules(msg_queue_t *msg);
void check_timeouts(void);
void delete_all_msg(void);
void delete_msg(msg_queue_t *elem);
void drawmsg(void);
void dunst_printf(int level, const char *fmt, ...);
char *fix_markup(char *str);
void handle_mouse_click(XEvent ev);
void handleXEvents(void);
void history_pop(void);
void initmsg(msg_queue_t *msg);
rule_t *initrule(void);
int is_idle(void);
char *string_replace(const char *needle, const char *replacement, char *haystack);
char *strtrim(char *str);
void run(void);
void setup(void);
void show_win(void);
void usage(int exit_status);

#include "dunst_dbus.h"


void
print_rule(rule_t *r) {
    dunst_printf(DEBUG, "%s %s %s %s %s %d %d %s %s %s\n",
            r->name,
            r->appname,
            r->summary,
            r->body,
            r->icon,
            r->timeout,
            r->urgency,
            r->fg,
            r->bg,
            r->format);
}

void
print_rules(void) {
    rule_t *cur = rules;
    dunst_printf(DEBUG, "current rules:\n");
    if (cur == NULL) {
        dunst_printf(DEBUG, "no rules present\n");
        return;
    }
    while(cur->next) {
        print_rule(cur);
        cur = cur->next;
    }
}

msg_queue_t*
add(msg_queue_t *queue, msg_queue_t *new) {
    msg_queue_t *i;
    msg_queue_t *prev = NULL;

    new->next = NULL;
    if(queue == NULL) {
        return new;
    }

    if(sort) {
        for(i = queue; i->next; i = i->next) {
            if(new->urgency > i->urgency) {
                if (!prev) {
                    /* we are at the first element */
                    queue = new;
                    new->next = i;
                    return queue;
                } else {
                    prev->next = new;
                    new->next = i;
                    return queue;
                }
            }
            prev = i;
        }
        /* we've reached the end of the queue */
        i->next = new;
        new->next = NULL;
    } else {
        for(i = queue; i->next; i = i->next);
        i->next = new;
    }
    return queue;

}

void
apply_rules(msg_queue_t *msg) {
    rule_t *cur = rules;
    while(cur != NULL) {
        if((!cur->appname || !fnmatch(cur->appname, msg->appname, 0))
        && (!cur->summary || !fnmatch(cur->summary, msg->summary, 0))
        && (!cur->body || !fnmatch(cur->body, msg->body, 0))
        && (!cur->icon || !fnmatch(cur->icon, msg->icon, 0))) {
            dunst_printf(DEBUG, "matched rule: %s\n", cur->name);
            msg->timeout = cur->timeout != -1 ? cur->timeout : msg->timeout;
            msg->urgency = cur->urgency != -1 ? cur->urgency : msg->urgency;
            msg->color_strings[ColFG] = cur->fg ? cur->fg : msg->color_strings[ColFG];
            msg->color_strings[ColBG] = cur->bg ? cur->bg : msg->color_strings[ColBG];
            msg->format = cur->format ? cur->format : msg->format;
        }

        cur = cur->next;
    }
}

msg_queue_t*
delete(msg_queue_t *elem) {
    msg_queue_t *prev;
    msg_queue_t *next;
    if(msgqueue == NULL || elem == NULL) {
        return NULL;
    }

    if(elem == msgqueue) {
        next = elem->next;
        history = add(history, elem);
        return next;
    }

    next = elem->next;
    for(prev = msgqueue; prev != NULL; prev = prev->next) {
        if(prev->next == elem) {
            break;
        }
    }
    if(prev == NULL) {
        /* the element wasn't in the list */
        return msgqueue;
    }

    prev->next = next;
    history = add(history, elem);
    return msgqueue;
}

int list_len(msg_queue_t *list) {
    int count = 0;
    msg_queue_t *i;
    if(list == NULL) {
        return 0;
    }
    for(i = list; i != NULL; i = i->next) {
        count++;
    }
    return count;
}

msg_queue_t*
pop(msg_queue_t *list, msg_queue_t **target) {
    msg_queue_t *prev = NULL;
    msg_queue_t *cur = NULL;

    if (!list) {
        *target = NULL;
        return list;
    }

    if (!list->next) {
        *target = list;
        return NULL;
    }

    cur = list->next;
    prev = list;
    while(cur->next) {
        cur = cur->next;
        prev = prev->next;
    }

    prev->next = NULL;

    *target = cur;
    return list;
}

void
check_timeouts(void) {
    msg_queue_t *cur, *next;

    cur = msgqueue;
    while(cur != NULL) {
        if(is_idle()) {
            cur->start = now;
            cur = cur->next;
            continue;
        }
        if(cur->start == 0 || cur->timeout == 0) {
            cur = cur->next;
            continue;
        }
        if(difftime(now, cur->start) > cur->timeout) {
            next = cur->next;
            delete_msg(cur);
            cur = next;
        } else {
            cur = cur->next;
        }
    }
}

void
delete_all_msg(void) {
    while (msgqueue != NULL) {
        delete_msg(NULL);
    }
}

void
delete_msg(msg_queue_t *elem) {
    msg_queue_t *cur;
    msg_queue_t *min;
    int visible_count = 0;
    if(msgqueue == NULL) {
        return;
    }
    if(elem == NULL) {
        /* delete the oldest element */
        min = msgqueue;
        for(elem = msgqueue; elem->next != NULL; elem = elem->next) {
            if(elem->start > 0 && min->start > elem->start) {
                min = elem;
            }
        }
        elem = min;
    }
    msgqueue = delete(elem);
    for(cur = msgqueue; cur != NULL; cur = cur->next) {
        if(cur->start != 0) {
            visible_count++;
        }
    }
    if(visible_count == 0) {
        /* hide window */
        if(!visible) {
            /* window is already hidden */
            return;
        }
        XUngrabButton(dc->dpy, AnyButton, AnyModifier, win);
        XUnmapWindow(dc->dpy, win);
        XFlush(dc->dpy);
        visible = False;
    } else {
        drawmsg();
    }
}

void
drawmsg(void) {
    int width, x, y, height, drawn_msg_count, i;
    unsigned int len = list_len(msgqueue);
    msg_queue_t *cur_msg = msgqueue;
    char hidden[128];
    int hidden_count = 0;
    int hidden_color_idx = NORM;
    dc->x = 0;
    dc->y = 0;
    dc->h = 0;
    /* a height of 0 doesn't make sense, so we define it as 1 */
    if(geometry.h == 0) {
        geometry.h = 1;
    }

    height = MIN(geometry.h, len);
    drawn_msg_count = height;
    hidden_count = len - height;
    hidden_count = indicate_hidden && hidden_count > 0 ? hidden_count : 0;
    sprintf(hidden, "(%d more)", hidden_count);
    if(hidden_count)
        height++;

    if(geometry.mask & WidthValue) {
        if(geometry.w == 0) {
            width = 0;
            for(i = 0; i < height; i++){
                width = MAX(width, textw(dc, cur_msg->msg));
                if(hidden_count)
                    width = MAX(width, textw(dc,hidden));
                cur_msg = cur_msg->next;
            }
        } else {
            width = geometry.w;
        }
    } else {
        width = scr.dim.w;
    }

    cur_msg = msgqueue;

    if(geometry.mask & XNegative) {
        x = (scr.dim.x + (scr.dim.w - width)) + geometry.x;
    } else {
        x = scr.dim.x + geometry.x;
    }

    if(geometry.mask & YNegative) {
        y = (scr.dim.h + geometry.y) - height*font_h;
    } else {
       y = 0 + geometry.y;
    }

    resizedc(dc, width, height*font_h);
    XResizeWindow(dc->dpy, win, width, height*font_h);
    drawrect(dc, 0, 0, width, height*font_h, True, colors[NORM]->BG);

    for(i = 0; i < drawn_msg_count; i++) {
        if(cur_msg->start == 0)
            cur_msg->start = now;

        drawrect(dc, 0, dc->y, width, font_h, True, cur_msg->colors->BG);
        drawtext(dc, cur_msg->msg, cur_msg->colors);

        dc->y += font_h;
        hidden_color_idx = cur_msg->urgency;
        cur_msg = cur_msg->next;
    }

    if(hidden_count) {
        drawrect(dc, 0, dc->y, width, font_h, True, colors[NORM]->BG);

        drawtext(dc, hidden, colors[hidden_color_idx]);
        dc->y += font_h;
    }


    XMoveWindow(dc->dpy, win, x, y);

    mapdc(dc, win, width, height*font_h);
}

void
dunst_printf(int level, const char *fmt, ...) {
    va_list ap;

    if(level > verbosity) {
        return;
    }
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

char
*fix_markup(char *str) {
    char *replace_buf, *start, *end;

    if(str == NULL) {
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
    if(start != NULL) {
        end = strstr(str, ">");
        if(end != NULL) {
            replace_buf = strndup(start, end-start+1);
            str = string_replace(replace_buf, "", str);
            free(replace_buf);
        }
    }
    start = strstr(str, "<img src");
    if(start != NULL) {
        end = strstr(str, "/>");
        if(end != NULL) {
            replace_buf = strndup(start, end-start+2);
            str = string_replace(replace_buf, "", str);
            free(replace_buf);
        }
    }
    return str;

}


void
handle_mouse_click(XEvent ev) {
    msg_queue_t *cur_msg = msgqueue;
    int i;
    if(ev.xbutton.button == Button3) {
        delete_all_msg();
    } else if(ev.xbutton.button == Button1) {
        i = ev.xbutton.y / font_h;
        for(i = i; i > 0; i--) {
            cur_msg = cur_msg->next;
        }
        delete_msg(cur_msg);
    }
}

void
handleXEvents(void) {
    XEvent ev;
    while(XPending(dc->dpy) > 0) {
        XNextEvent(dc->dpy, &ev);
        switch(ev.type) {
        case Expose:
            if(ev.xexpose.count == 0)
                mapdc(dc, win, scr.dim.w, font_h);
            break;
        case SelectionNotify:
            if(ev.xselection.property == utf8)
            break;
        case VisibilityNotify:
            if(ev.xvisibility.state != VisibilityUnobscured)
                XRaiseWindow(dc->dpy, win);
            break;
        case ButtonPress:
            if(ev.xbutton.window == win) {
                handle_mouse_click(ev);
            }
            break;
        case KeyPress:
            if(XLookupKeysym(&ev.xkey, 0) == key) {
                delete_msg(NULL);
            }
            if(XLookupKeysym(&ev.xkey, 0) == history_key) {
                history_pop();
            }
        }
    }
}

void
history_pop(void) {
    msg_queue_t *elem = NULL;
    history = pop(history, &elem);
    if(!elem) {
        return;
    }
    msgqueue = add(msgqueue, elem);
    elem->timeout = elem->timeout == -1 ? timeouts[elem->urgency] : elem->timeout;

    elem->start = 0;
    drawmsg();
}

void
initmsg(msg_queue_t *msg) {
    const char *fg = NULL;
    const char *bg = NULL;
    msg->format = format;
    apply_rules(msg);

    msg->msg = string_replace("%a", msg->appname, strdup(msg->format));
    msg->msg = string_replace("%s", msg->summary, msg->msg);
    msg->msg = string_replace("%i", msg->icon, msg->msg);
    msg->msg = string_replace("%I", basename(msg->icon), msg->msg);
    msg->msg = string_replace("%b", msg->body, msg->msg);

    msg->msg = fix_markup(msg->msg);
    msg->msg = strtrim(msg->msg);
    /* urgency > CRIT -> array out of range */
    msg->urgency = msg->urgency > CRIT ? CRIT : msg->urgency;



    if (msg->color_strings[ColFG]) {
        fg = msg->color_strings[ColFG];
    } else {
        fg = color_strings[ColFG][msg->urgency];
    }

    if (msg->color_strings[ColBG]) {
        bg = msg->color_strings[ColBG];
    } else {
        bg = color_strings[ColBG][msg->urgency];
    }

    msg->colors = initcolor(dc, fg, bg);

    msg->timeout = msg->timeout == -1 ? timeouts[msg->urgency] : msg->timeout;
    msg->start = 0;

    dunst_printf(MSG, "%s\n", msg->msg);
    dunst_printf(INFO, "{\n  appname: %s\n  summary: %s\n  body: %s\n  icon: %s\n  urgency: %d\n  timeout: %d\n}",
            msg->appname, msg->summary, msg->body, msg->icon, msg->urgency, msg->timeout);

}

rule_t *
initrule(void) {
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
    r->next = NULL;

    return r;
}

int
is_idle(void)
{
    XScreenSaverQueryInfo(dc->dpy, DefaultRootWindow(dc->dpy),
            screensaver_info);
    if(idle_threshold == 0) {
        return False;
    }
    return screensaver_info->idle / 1000 > idle_threshold;
}

char *
string_replace(const char *needle, const char *replacement, char *haystack) {
    char *tmp, *start;
    int size;
    start = strstr(haystack, needle);
    if(start == NULL) {
        return haystack;
    }

    size = (strlen(haystack) - strlen(needle)) + strlen(replacement) + 1;
    tmp = calloc(sizeof(char), size);
    memset(tmp, '\0', size);

    strncpy(tmp, haystack, start-haystack);
    tmp[start-haystack] = '\0';

    sprintf(tmp+strlen(tmp), "%s%s", replacement, start+strlen(needle));
    free(haystack);

    if(strstr(tmp, needle)) {
        return string_replace(needle, replacement, tmp);
    } else {
        return tmp;
    }
}

char *
strtrim(char *str) {
    char *end;
    while(isspace(*str)) str++;

    end = str + strlen(str) - 1;
    while(isspace(*end)) {
        *end = '\0';
        end--;
    }

    return str;

}

void
run(void) {

    while(True) {
        /* dbus_poll blocks for max 2 seconds, if no events are present */
        dbus_poll();
        now = time(&now);
        if(msgqueue != NULL) {
            show_win();
            check_timeouts();
        }
        handleXEvents();
    }
}

void
setup(void) {
    Window root;
    XSetWindowAttributes wa;
    KeyCode code;
#ifdef XINERAMA
    int n;
    XineramaScreenInfo *info;
#endif
    if(scr.scr < 0) {
        scr.scr = DefaultScreen(dc->dpy);
    }
    root = RootWindow(dc->dpy, DefaultScreen(dc->dpy));

    utf8 = XInternAtom(dc->dpy, "UTF8_STRING", False);

    /* menu geometry */
    font_h = dc->font.height + FONT_HEIGHT_BORDER;
#ifdef XINERAMA
    if((info = XineramaQueryScreens(dc->dpy, &n))) {
        if(scr.scr >= n) {
            fprintf(stderr, "Monitor %d not found\n", scr.scr);
            exit(EXIT_FAILURE);
        }
        scr.dim.x = info[scr.scr].x_org;
        scr.dim.y = info[scr.scr].y_org;
        scr.dim.h = info[scr.scr].height;
        scr.dim.w = info[scr.scr].width;
        XFree(info);
    }
    else
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
    wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask | ButtonPressMask;
    win = XCreateWindow(dc->dpy, root, scr.dim.x, scr.dim.y, scr.dim.w, font_h, 0,
                        DefaultDepth(dc->dpy, DefaultScreen(dc->dpy)), CopyFromParent,
                        DefaultVisual(dc->dpy, DefaultScreen(dc->dpy)),
                        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

    /* grab keys */
    if(key != NoSymbol) {
        code = XKeysymToKeycode(dc->dpy, key);
        XGrabKey(dc->dpy, code, mask, root, True, GrabModeAsync, GrabModeAsync);
    }

    if (history_key != NoSymbol) {
        code = XKeysymToKeycode(dc->dpy, history_key);
        XGrabKey(dc->dpy, code, mask, root, True, GrabModeAsync, GrabModeAsync);
    }
}

void
show_win(void) {
    if(visible || msgqueue == NULL) {
        return;
    }

    XMapRaised(dc->dpy, win);
    XGrabButton(dc->dpy, AnyButton, AnyModifier, win, False,
        BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
    XFlush(dc->dpy);
    drawmsg();
    visible = True;
}

void
parse_cmdline(int argc, char *argv[]) {

    int c;
    while(1) {
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
        {0,0,0,0}
    };


        int option_index = 0;

        c = getopt_long_only(argc, argv, "bhsv", long_options, &option_index);

        if(c == -1) {
            break;
        }

        switch(c)
        {
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
                        &geometry.w, &geometry.h);
                break;
            case 'M':
                if(!strcmp(optarg, "ctrl")) {
                    mask |= ControlMask;
                }
                else if(!strcmp(optarg, "mod1")) {
                    mask |= Mod1Mask;
                }
                else if(!strcmp(optarg, "mod2")) {
                    mask |= Mod2Mask;
                }
                else if(!strcmp(optarg, "mod3")) {
                    mask |= Mod3Mask;
                }
                else if(!strcmp(optarg, "mod4")) {
                    mask |= Mod4Mask;
                } else {
                    fprintf(stderr, "Unable to find mask: %s\n", optarg);
                    fprintf(stderr, "See manpage for list of available masks\n");
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

static int
dunst_ini_get_boolean(const char *value) {

    switch (value[0])
    {
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

static rule_t *
dunst_rules_find_or_create(const char *section) {

    rule_t *current_rule = rules, *last_rule;

    while (current_rule && strcmp(current_rule->name, section) != 0) {
    current_rule = current_rule->next;
    }

    if (current_rule) {
    return current_rule;
    }

    dunst_printf(DEBUG, "adding rule %s\n", section);

    current_rule = initrule();
    current_rule->name = strdup(section);

    last_rule = rules;
    while (last_rule && last_rule->next) {
    last_rule = last_rule->next;
    }

    if (last_rule == NULL) {
    last_rule = current_rule;
    rules = last_rule;
    } else {
    last_rule->next = current_rule;
    }

    return current_rule;
}

static char *
dunst_ini_get_string(const char *value) {

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
         const char *name, const char *value) {

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
    else if (strcmp(name, "geometry") == 0) {
        geom = dunst_ini_get_string(value);
        geometry.mask = XParseGeometry(geom,
                       &geometry.x, &geometry.y,
                       &geometry.w, &geometry.h);
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
        free (mod);
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
        fprintf(stderr, "unknown urgency: %s, ignoring\n", urg);
    } else if (strcmp(name, "foreground") == 0)
        current_rule->fg = dunst_ini_get_string(value);
    else if (strcmp(name, "background") == 0)
        current_rule->bg = dunst_ini_get_string(value);
    else if (strcmp(name, "format") == 0)
        current_rule->format = dunst_ini_get_string(value);
    }

    return 1;
}

void
parse_dunstrc(void) {

    xdgHandle xdg;
    FILE *config_file;

    dunst_printf(DEBUG, "Begin parsing of dunstrc\n");

    xdgInitHandle(&xdg);

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


void
parse_cmdline_for_config_file(int argc, char *argv[]) {
    int i;
    for (i = 0; i < argc; i++) {
        if(strstr(argv[i], "-config") != 0) {
            if (i + 1 == argc) {
                printf("Invalid commandline: -config needs argument\n");
            }
            config_file = argv[++i];
            return;
        }
    }
}

int
main(int argc, char *argv[]) {

    now = time(&now);
    dc = initdc();
    geometry.mask = XParseGeometry(geom,
            &geometry.x, &geometry.y,
            &geometry.w, &geometry.h);

    parse_cmdline_for_config_file(argc, argv);
    parse_dunstrc();
    parse_cmdline(argc, argv);
    key = key_string ? XStringToKeysym(key_string) : NoSymbol;
    history_key = history_key_string ? XStringToKeysym(history_key_string) : NoSymbol;
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
    if(msgqueue != NULL) {
        show_win();
    }
    run();
    return 0;
}

void
usage(int exit_status) {
    fputs("usage: dunst [-h/--help] [-v] [-geometry geom] [-fn font] [-format fmt]\n[-nb color] [-nf color] [-lb color] [-lf color] [-cb color] [ -cf color]\n[-to secs] [-lto secs] [-cto secs] [-nto secs] [-key key] [-history_key key] [-mod modifier] [-mon n] [-config dunstrc]\n", stderr);
    exit(exit_status);
}

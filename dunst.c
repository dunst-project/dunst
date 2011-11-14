#define _GNU_SOURCE
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "dunst.h"
#include "draw.h"

#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))
#define MIN(a,b)                ((a) < (b) ? (a) : (b))
#define MAX(a,b)                ((a) > (b) ? (a) : (b))
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define FONT_HEIGHT_BORDER 2
#define LOW 0
#define NORM 1
#define CRIT 2

/* structs */

typedef struct _dimension_t {
    int x;
    int y;
    unsigned int h;
    unsigned int w;
    int mask;
} dimension_t;

typedef struct _screen_info {
    int scr;
    dimension_t dim;
} screen_info;


/* global variables */
static const char *font = NULL;
static const char *normbgcolor = "#cccccc";
static const char *normfgcolor = "#000000";
static const char *critbgcolor = "#ffaaaa";
static const char *critfgcolor = "#000000";
static const char *lowbgcolor =  "#aaaaff";
static const char *lowfgcolor = "#000000";
/* index of colors fit to urgency level */
static unsigned long colors[3][ColLast];
static Atom utf8;
static DC *dc;
static Window win;
static double timeouts[] = { 10, 10, 0 };
static msg_queue_t *msgqueue = NULL;
static time_t now;
static int visible = False;
static KeySym key = NoSymbol;
static KeySym mask = 0;
static screen_info scr;
static dimension_t geometry;
static int font_h;
static const char *format = "%s %b";
static int verbose = False;

/* list functions */
msg_queue_t *append(msg_queue_t *queue, msg_queue_t *msg);
msg_queue_t *delete(msg_queue_t *elem);
msg_queue_t *pop(msg_queue_t *queue);
int list_len(msg_queue_t *list);


/* misc funtions */
void check_timeouts(void);
void delete_all_msg(void);
void delete_msg(msg_queue_t *elem);
void drawmsg(void);
void dunst_printf(const char *fmt, ...);
char *fix_markup(char *str);
void free_msgqueue_t(msg_queue_t *elem);
void handleXEvents(void);
char *string_replace(const char *needle, const char *replacement, char *haystack);
void run(void);
void setup(void);
void show_win(void);
void usage(int exit_status);

#include "dunst_dbus.h"

static void
_set_color(msg_queue_t *msg, int color_idx) {
    Colormap cmap = DefaultColormap(dc->dpy, DefaultScreen(dc->dpy));
    XColor color;
    if(msg->color_strings[color_idx] == NULL
       || !XAllocNamedColor(dc->dpy, cmap,
           msg->color_strings[color_idx], &color, &color)) {
        msg->colors[color_idx] = colors[msg->urgency][color_idx];
    } else {
        msg->colors[color_idx] = color.pixel;
    }
}

msg_queue_t*
append(msg_queue_t *queue, msg_queue_t *new) {
    msg_queue_t *last;

    new->msg = string_replace("%a", new->appname, strdup(format));
    new->msg = string_replace("%s", new->summary, new->msg);
    new->msg = string_replace("%i", new->icon, new->msg);
    new->msg = string_replace("%I", basename(new->icon), new->msg);
    new->msg = string_replace("%b", new->body, new->msg);

    new->msg = fix_markup(new->msg);

    /* urgency > CRIT -> array out of range */
    new->urgency = new->urgency > CRIT ? CRIT : new->urgency;

    _set_color(new, ColFG);
    _set_color(new, ColBG);

    new->timeout = new->timeout == -1 ? timeouts[new->urgency] : new->timeout;
    new->start = 0;

    dunst_printf("%s (timeout: %d, urgency: %d)\n", new->msg, new->timeout, new->urgency);
    new->next = NULL;
    if(queue == NULL) {
        return new;
    }
    for(last = queue; last->next; last = last->next);
    last->next = new;
    return queue;

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
        free_msgqueue_t(elem);
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
    free_msgqueue_t(elem);
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

void
check_timeouts(void) {
    msg_queue_t *cur, *next;

    cur = msgqueue;
    while(cur != NULL) {
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
    int width, x, y, height, i;
    unsigned int len = list_len(msgqueue);
    msg_queue_t *cur_msg = msgqueue;
    dc->x = 0;
    dc->y = 0;
    dc->h = 0;
    /* a height of 0 doesn't make sense, so we define it as 1 */
    if(geometry.h == 0) {
        geometry.h = 1;
    }

    height = MIN(geometry.h, len);

    if(geometry.mask & WidthValue) {
        if(geometry.w == 0) {
            width = 0;
            for(i = 0; i < height; i++){
                width = MAX(width, textw(dc, cur_msg->msg));
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
    drawrect(dc, 0, 0, width, height*font_h, True, BG(dc, colors[NORM]));

    for(i = 0; i < height; i++) {
        if(cur_msg->start == 0)
            cur_msg->start = now;

        drawrect(dc, 0, dc->y, width, font_h, True, BG(dc, cur_msg->colors));
        drawtext(dc, cur_msg->msg, cur_msg->colors);

        dc->y += font_h;
        cur_msg = cur_msg->next;
    }

    XMoveWindow(dc->dpy, win, x, y);

    mapdc(dc, win, width, height*font_h);
}

void
dunst_printf(const char *fmt, ...) {
    va_list ap;

    if(!verbose) {
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
free_msgqueue_t(msg_queue_t *elem) {
    int i;
    free(elem->appname);
    free(elem->summary);
    free(elem->body);
    free(elem->msg);
    for(i = 0; i < ColLast; i++) {
        if(elem->color_strings[i] != NULL) {
            free(elem->color_strings[i]);
        }
    }
    free(elem);
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
                if(ev.xbutton.button == Button1) {
                    delete_msg(NULL);
                } else {
                    delete_all_msg();
                }
            }
            break;
        case KeyPress:
            if(XLookupKeysym(&ev.xkey, 0) == key) {
                delete_msg(NULL);
            }
        }
    }
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

    colors[0][ColBG] = getcolor(dc, lowbgcolor);
    colors[0][ColFG] = getcolor(dc, lowfgcolor);
    colors[1][ColBG] = getcolor(dc, normbgcolor);
    colors[1][ColFG] = getcolor(dc, normfgcolor);
    colors[2][ColBG] = getcolor(dc, critbgcolor);
    colors[2][ColFG] = getcolor(dc, critfgcolor);

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


int
main(int argc, char *argv[]) {

    int c;

    now = time(&now);
    dc = initdc();

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
        {"geometry", required_argument, NULL, 'g'},
        {"mod", required_argument, NULL, 'M'},
        {0,0,0,0}
    };


        int option_index = 0;

        c = getopt_long_only(argc, argv, "bhv", long_options, &option_index);

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
            case 'v':
                verbose = True;
                break;
            default:
                usage(EXIT_FAILURE);
                break;
        }
    }

    initdbus();
    initfont(dc, font);
    setup();
    if(msgqueue != NULL) {
        show_win();
    }
    run();
    return 0;
}

void
usage(int exit_status) {
    fputs("usage: dunst [-h/--help] [-v] [-geometry geom] [-fn font] [-format fmt]\n[-nb color] [-nf color] [-lb color] [-lf color] [-cb color] [ -cf color]\n[-to secs] [-lto secs] [-cto secs] [-nto secs] [-key key] [-mod modifier] [-mon n]\n", stderr);
    exit(exit_status);
}

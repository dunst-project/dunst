#define _GNU_SOURCE
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

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
typedef struct _msg_queue_t {
    char *msg;
    struct _msg_queue_t *next;
    time_t start;
    int timeout;
    int urgency;
    unsigned long colors[ColLast];
} msg_queue_t;

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

/* list functions */
msg_queue_t *append(msg_queue_t *queue, char *msg, int to, int urgency, const char *fg, const char *bg);
msg_queue_t *delete(msg_queue_t *elem);
msg_queue_t *pop(msg_queue_t *queue);
int list_len(msg_queue_t *list);


/* misc funtions */
void check_timeouts(void);
void delete_msg(msg_queue_t *elem);
void drawmsg(void);
char *fix_markup(char *str);
char *format_msg(const char *app, const char *sum, const char *body, const char *icon);
void handleXEvents(void);
char *string_replace(const char *needle, const char *replacement, char *haystack);
void run(void);
void setup(void);
void show_win(void);
void usage(int exit_status);

#include "dunst_dbus.h"

msg_queue_t*
append(msg_queue_t *queue, char *msg, int to, int urgency, const char *fg, const char *bg) {
    msg_queue_t *new = malloc(sizeof(msg_queue_t));
    msg_queue_t *last;
    Colormap cmap = DefaultColormap(dc->dpy, DefaultScreen(dc->dpy));
    XColor color;


    new->msg = fix_markup(msg);
    new->urgency = urgency;
    new->urgency = new->urgency > CRIT ? CRIT : new->urgency;

	if(fg == NULL || !XAllocNamedColor(dc->dpy, cmap, fg, &color, &color)) {
        new->colors[ColFG] = colors[new->urgency][ColFG];
    } else {
        new->colors[ColFG] = color.pixel;
    }
	if(bg == NULL || !XAllocNamedColor(dc->dpy, cmap, bg, &color, &color)) {
        new->colors[ColBG] = colors[new->urgency][ColBG];
    } else {
        new->colors[ColBG] = color.pixel;
    }

    if(to == -1) {
        new->timeout = timeouts[urgency];
    } else {
        new->timeout = to;
    }

    new->start = 0;
    printf("%s (timeout: %d, urgency: %d)\n", new->msg, new->timeout, urgency);
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
    if(msgqueue == NULL) {
        return NULL;
    }
    if(elem == NULL) {
        return msgqueue;
    }

    if(elem == msgqueue) {
        next = elem->next;
        free(elem->msg);
        free(elem);
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
    free(elem->msg);
    free(elem);
    return msgqueue;
}

msg_queue_t*
pop(msg_queue_t *queue) {
    msg_queue_t *new_head;
    if(queue == NULL) {
        return NULL;
    }
    if(queue->next == NULL) {
        free(queue->msg);
        free(queue);
        return NULL;
    }
    new_head = queue->next;
    free(queue->msg);
    free(queue);
    return new_head;
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

char
*fix_markup(char *str) {
    char *tmpString, *strpos, *tmppos;
    char *replace_buf, *start, *end;

    if(str == NULL) {
        return NULL;
    }

    /* tmpString can never be bigger than str */
    tmpString = (char *) calloc(strlen(str), sizeof(char) + 1);
    memset(tmpString, '\0', strlen(tmpString) * sizeof(char) + 1);
    tmppos = tmpString;
    strpos = str;

    while(*strpos != '\0') {
        if(*strpos != '&') {
            /* not the beginning of an xml-escape */
            *tmppos = *strpos;
            strpos++;
            tmppos++;
            continue;
        }
        else if(!strncmp(strpos, "&quot;", strlen("&quot;"))) {
            *tmppos = '"';
            tmppos++;
            strpos += strlen("&quot;");
        }
        else if(!strncmp(strpos, "&apos;", strlen("apos;"))) {
            *tmppos = '\'';
            tmppos++;
            strpos += strlen("&apos;");
        }
        else if(!strncmp(strpos, "&amp;", strlen("amp;"))) {
            *tmppos = '&';
            tmppos++;
            strpos += strlen("&amp;");
        }
        else if(!strncmp(strpos, "&lt;", strlen("lt;"))) {
            *tmppos = '<';
            tmppos++;
            strpos += strlen("&lt;");
        }
        else if(!strncmp(strpos, "&gt;", strlen("gt;"))) {
            *tmppos = '>';
            tmppos++;
            strpos += strlen("&gt;");
        }
        else {
            *tmppos = *strpos;
            strpos++;
            tmppos++;
        }
    }

    free(str);

    tmpString = string_replace("\n", " ", tmpString);
    /* remove tags */
    tmpString = string_replace("<b>", "", tmpString);
    tmpString = string_replace("</b>", "", tmpString);
    tmpString = string_replace("<i>", "", tmpString);
    tmpString = string_replace("</i>", "", tmpString);
    tmpString = string_replace("<u>", "", tmpString);
    tmpString = string_replace("</u>", "", tmpString);
    tmpString = string_replace("</a>", "", tmpString);

    start = strstr(tmpString, "<a href");
    if(start != NULL) {
        end = strstr(tmpString, ">");
        if(end != NULL) {
            replace_buf = strndup(start, end-start+1);
            printf("replace_buf: '%s'\n", replace_buf);
            tmpString = string_replace(replace_buf, "", tmpString);
            free(replace_buf);
        }
    }
    start = strstr(tmpString, "<img src");
    if(start != NULL) {
        end = strstr(tmpString, "/>");
        if(end != NULL) {
            replace_buf = strndup(start, end-start+2);
            printf("replace_buf: '%s'\n", replace_buf);
            tmpString = string_replace(replace_buf, "", tmpString);
            free(replace_buf);
        }
    }
    return tmpString;

}

char *
_do_replace(char *buf, char *replace_buf, const char *to_replace, const char *replacement) {
    char *replace_buf_old = strdup(replace_buf);
    if(strstr(replace_buf, to_replace)) {
        if(strlen(replacement) > 0) {
            replace_buf = string_replace("%{", "", replace_buf);

            replace_buf = string_replace(to_replace, replacement, replace_buf);
            replace_buf[strlen(replace_buf)-1] = '\0';
            buf = string_replace(replace_buf_old, replace_buf, buf);
        } else {
            buf = string_replace(replace_buf, "", buf);
        }
    }
    return buf;
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
                delete_msg(NULL);
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
    if(visible == True) {
        /* window is already visible */
        return;
    }
    if(msgqueue == NULL) {
        /* there's nothing to show */
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

    int i;

    now = time(&now);
    dc = initdc();

    for(i = 1; i < argc; i++) {
        /* switches */
        if(!strcmp(argv[i], "-b")) {
            geometry.mask |= YNegative;
        }
        else if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
            usage(EXIT_SUCCESS);

        /* options */
        else if(i == argc-1) {
            printf("Option needs an argument\n");
            usage(1);
        }
        else if(!strcmp(argv[i], "-fn"))
            font = argv[++i];
        else if(!strcmp(argv[i], "-nb"))
            normbgcolor = argv[++i];
        else if(!strcmp(argv[i], "-nf"))
            normfgcolor = argv[++i];
        else if(!strcmp(argv[i], "-lb"))
            lowbgcolor = argv[++i];
        else if(!strcmp(argv[i], "-lf"))
            lowfgcolor = argv[++i];
        else if(!strcmp(argv[i], "-cb"))
            critbgcolor = argv[++i];
        else if(!strcmp(argv[i], "-cf"))
            critfgcolor = argv[++i];
        else if(!strcmp(argv[i], "-to")) {
            timeouts[0] = atoi(argv[++i]);
            timeouts[1] = timeouts[0];
        }
        else if(!strcmp(argv[i], "-lto"))
            timeouts[0] = atoi(argv[++i]);
        else if(!strcmp(argv[i], "-nto"))
            timeouts[1] = atoi(argv[++i]);
        else if(!strcmp(argv[i], "-cto"))
            timeouts[2] = atoi(argv[++i]);
        else if(!strcmp(argv[i], "-mon")) {
            scr.scr = atoi(argv[++i]);
        }
        else if(!strcmp(argv[i], "-format")) {
            format = argv[++i];
        }
        else if(!strcmp(argv[i], "-key")) {
            key = XStringToKeysym(argv[i+1]);
            if(key == NoSymbol) {
                fprintf(stderr, "Unable to grab key: %s.\n", argv[i+1]);
                exit(EXIT_FAILURE);
            }
            i++;
        }
        else if(!strcmp(argv[i], "-geometry")) {
            geometry.mask = XParseGeometry(argv[++i], &geometry.x, &geometry.y, &geometry.w, &geometry.h);
        }
        else if(!strcmp(argv[i], "-mod")) {
            if(!strcmp(argv[i+1], "ctrl")) {
                mask |= ControlMask;
            }
            else if(!strcmp(argv[i+1], "shift")) {
                mask |= ShiftMask;
            }
            else if(!strcmp(argv[i+1], "mod1")) {
                mask |= Mod1Mask;
            }
            else if(!strcmp(argv[i+1], "mod2")) {
                mask |= Mod2Mask;
            }
            else if(!strcmp(argv[i+1], "mod3")) {
                mask |= Mod3Mask;
            }
            else if(!strcmp(argv[i+1], "mod4")) {
                mask |= Mod4Mask;
            } else {
                fprintf(stderr, "Unable to find mask: %s\n", argv[i+1]);
                fprintf(stderr, "See manpage for list of available masks\n");
            }
            i++;
        }
        else
            usage(EXIT_FAILURE);
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
    fputs("usage: dunst [-h/--help] [-geometry geom] [-fn font] [-format fmt]\n[-nb color] [-nf color] [-lb color] [-lf color] [-cb color] [ -cf color]\n[-to secs] [-lto secs] [-cto secs] [-nto secs] [-key key] [-mod modifier] [-mon n]\n", stderr);
    exit(exit_status);
}

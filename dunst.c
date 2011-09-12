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

/* structs */
typedef struct _msg_queue_t {
    char *msg;
    struct _msg_queue_t *next;
    time_t start;
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
static unsigned long normcol[ColLast];
static Atom utf8;
static DC *dc;
static Window win;
static double global_timeout = 10;
static msg_queue_t *msgqueue = NULL;
static time_t now;
static int listen_to_dbus = True;
static int visible = False;
static KeySym key = NoSymbol;
static KeySym mask = 0;
static screen_info scr;
static dimension_t geometry;
static int font_h;

/* list functions */
msg_queue_t *append(msg_queue_t *queue, char *msg);
msg_queue_t *pop(msg_queue_t *queue);
int list_len(msg_queue_t *list);


/* misc funtions */
void delete_msg(void);
void drawmsg(void);
void handleXEvents(void);
void run(void);
void setup(void);
void show_win(void);
void usage(int exit_status);

#include "dunst_dbus.h"

msg_queue_t*
append(msg_queue_t *queue, char *msg) {
    msg_queue_t *new = malloc(sizeof(msg_queue_t));
    msg_queue_t *last;
    new->msg = msg;
    new->start = 0;
    printf("%s\n", new->msg);
    new->next = NULL;
    if(queue == NULL) {
        return new;
    }
    for(last = queue; last->next; last = last->next);
    last->next = new;
    return queue;
}

msg_queue_t*
pop(msg_queue_t *queue) {
    msg_queue_t *new_head;
    if(queue == NULL) {
        return NULL;
    }
    if(queue->next == NULL) {
        free(queue);
        return NULL;
    }
    new_head = queue->next;
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
delete_msg(void) {
    if(msgqueue == NULL) {
        return;
    }
    msgqueue = pop(msgqueue);
    if(msgqueue == NULL) {
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
    int len = list_len(msgqueue);
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
    drawrect(dc, 0, 0, width, height*font_h, True, BG(dc, normcol));

    for(i = 0; i < height; i++) {
        if(cur_msg->start == 0)
            cur_msg->start = now;

        drawtext(dc, cur_msg->msg, normcol);
        dc->y += font_h;
        cur_msg = cur_msg->next;
    }

    XMoveWindow(dc->dpy, win, x, y);

    mapdc(dc, win, width, height*font_h);
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
                delete_msg();
            }
            break;
        case KeyPress:
            if(XLookupKeysym(&ev.xkey, 0) == key) {
                delete_msg();
            }
        }
    }
}

void
run(void) {

    while(True) {
        /* dbus_poll blocks for max 2 seconds, if no events are present */
        if(listen_to_dbus) {
            dbus_poll();
        }
        now = time(&now);
        if(msgqueue != NULL) {
            show_win();
            if(difftime(now, msgqueue->start) > global_timeout) {
                delete_msg();
            }
            handleXEvents();
        } else if (!listen_to_dbus) {
            break;
        }
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

	normcol[ColBG] = getcolor(dc, normbgcolor);
	normcol[ColFG] = getcolor(dc, normfgcolor);

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
        else if(!strcmp(argv[i], "-nb") || !strcmp(argv[i], "-bg"))
            normbgcolor = argv[++i];
        else if(!strcmp(argv[i], "-nf") || !strcmp(argv[i], "-fg"))
            normfgcolor = argv[++i];
        else if(!strcmp(argv[i], "-to"))
            global_timeout = atoi(argv[++i]);
        else if(!strcmp(argv[i], "-msg")) {
             msgqueue = append(msgqueue, argv[++i]);
             listen_to_dbus = False;
        }
        else if(!strcmp(argv[i], "-mon")) {
            scr.scr = atoi(argv[++i]);
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

    if(listen_to_dbus) {
        initdbus();
    }
    dc = initdc();
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
    fputs("usage: dunst [-h/--help] [-geometry geom] [-fn font]\n[-nb/-bg color] [-nf/-fg color] [-to secs] [-key key] [-mod modifier] [-mon n] [-msg msg]\n", stderr);
    exit(exit_status);
}

/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

#pragma once

#include "draw.h"

#define LOW 0
#define NORM 1
#define CRIT 2

#define ColLast 2
#define ColFG 1
#define ColBG 0


enum alignment { left, center, right };
enum separator_color { FOREGROUND, AUTO };
enum follow_mode { FOLLOW_NONE, FOLLOW_MOUSE, FOLLOW_KEYBOARD };

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

typedef struct _draw_txt {
        char *txt;
        int line_count;
        int bufsize;
} draw_txt;

typedef struct _notification {
        char *appname;
        char *summary;
        char *body;
        char *icon;
        char *msg;
        draw_txt draw_txt_buf;
        const char *format;
        char *dbus_client;
        time_t start;
        time_t timestamp;
        int timeout;
        int urgency;
        int redisplayed;        /* has been displayed before? */
        int id;
        int dup_count;
        ColorSet *colors;
        char *color_strings[2];
        int progress;           /* percentage + 1, 0 to hide */
} notification;

typedef struct _notification_buffer {
        char txt[BUFSIZ];
        notification *n;
} notification_buffer;

typedef struct _rule_t {
        char *name;
        /* filters */
        char *appname;
        char *summary;
        char *body;
        char *icon;

        /* actions */
        int timeout;
        int urgency;
        char *fg;
        char *bg;
        const char *format;

} rule_t;

typedef struct _keyboard_shortcut {
        const char *str;
        KeyCode code;
        KeySym sym;
        KeySym mask;
        int is_valid;
} keyboard_shortcut;

extern int verbosity;

/* return id of notification */
int init_notification(notification * n, int id);
int close_notification(notification * n, int reason);
int close_notification_by_id(int id, int reason);
void map_win(void);

/* vim: set ts=8 sw=8 tw=0: */

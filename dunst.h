/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

#ifndef DUNST_H
#define DUNST_H

#include "draw.h"

#define LOW 0
#define NORM 1
#define CRIT 2

#define ColLast 2
#define ColFG 1
#define ColBG 0

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

typedef struct _notification {
        char *appname;
        char *summary;
        char *body;
        char *icon;
        char *msg;
        const char *format;
        time_t start;
        time_t timestamp;
        int timeout;
        int urgency;
        int redisplayed;        /* has been displayed before? */
        int id;
        ColorSet *colors;
        char *color_strings[2];
} notification;

typedef struct _dimension_t {
        int x;
        int y;
        unsigned int h;
        unsigned int w;
        int mask;
} dimension_t;
#endif
/* vim: set ts=8 sw=8 tw=0: */

/* return id of notification */
int init_notification(notification * n, int id);
int close_notification(int id);
void map_win(void);

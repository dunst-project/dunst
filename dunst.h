#ifndef DUNST_H
#define DUNST_H

#include "draw.h"

#define LOW 0
#define NORM 1
#define CRIT 2

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

    struct _rule_t *next;
} rule_t;

typedef struct _msg_queue_t {
    char *appname;
    char *summary;
    char *body;
    char *icon;
    char *msg;
    const char *format;
    struct _msg_queue_t *next;
    time_t start;
    int timeout;
    int urgency;
    unsigned long colors[ColLast];
    char *color_strings[ColLast];
} msg_queue_t;

typedef struct _dimension_t {
    int x;
    int y;
    unsigned int h;
    unsigned int w;
    int mask;
} dimension_t;
#endif

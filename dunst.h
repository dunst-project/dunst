#ifndef DUNST_H
#define DUNST_H

#include "draw.h"

typedef struct _msg_queue_t {
    char *appname;
    char *summary;
    char *body;
    char *icon;
    char *msg;
    struct _msg_queue_t *next;
    time_t start;
    int timeout;
    int urgency;
    unsigned long colors[ColLast];
    char *color_strings[ColLast];
} msg_queue_t;
#endif


#pragma once

#include "draw.h"

#define LOW 0
#define NORM 1
#define CRIT 2

typedef struct _actions {
        char **actions;
        char *dmenu_str;
        gsize count;
} Actions;

typedef struct _notification {
        char *appname;
        char *summary;
        char *body;
        char *icon;
        char *msg;
        const char *format;
        char *dbus_client;
        time_t start;
        time_t timestamp;
        int timeout;
        int urgency;
        bool redisplayed;       /* has been displayed before? */
        int id;
        int dup_count;
        ColorSet *colors;
        char *color_strings[2];

        int progress;           /* percentage + 1, 0 to hide */
        int line_count;
        const char *script;
        char *urls;
        Actions *actions;
} notification;


int notification_init(notification * n, int id);
int notification_close_by_id(int id, int reason);
int notification_cmp(const void *a, const void *b);
int notification_cmp_data(const void *a, const void *b, void *data);
void notification_run_script(notification *n);
int notification_close(notification * n, int reason);
void notification_print(notification *n);
char *notification_fix_markup(char *str);

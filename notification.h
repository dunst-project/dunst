/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#pragma once

#include "x.h"

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
        char *msg;            /* formatted message */
        char *category;
        char *text_to_render;
        const char *format;
        char *dbus_client;
        time_t start;
        time_t timestamp;
        int timeout;
        int urgency;
        bool redisplayed;       /* has been displayed before? */
        int id;
        int dup_count;
        int displayed_height;
        const char *color_strings[2];
        bool first_render;

        int progress;           /* percentage + 1, 0 to hide */
        int line_count;
        const char *script;
        char *urls;
        Actions *actions;
} notification;

int notification_init(notification * n, int id);
void notification_free(notification * n);
int notification_close_by_id(int id, int reason);
int notification_cmp(const void *a, const void *b);
int notification_cmp_data(const void *a, const void *b, void *data);
void notification_run_script(notification * n);
int notification_close(notification * n, int reason);
void notification_print(notification * n);
char *notification_fix_markup(char *str);
void notification_update_text_to_render(notification *n);
int notification_get_ttl(notification *n);
int notification_get_age(notification *n);

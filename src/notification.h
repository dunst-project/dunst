/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_NOTIFICATION_H
#define DUNST_NOTIFICATION_H

#include <glib.h>
#include <stdbool.h>

#include "settings.h"

#define LOW 0
#define NORM 1
#define CRIT 2

#define DUNST_NOTIF_MAX_CHARS 5000

typedef struct _raw_image {
        int width;
        int height;
        int rowstride;
        int has_alpha;
        int bits_per_sample;
        int n_channels;
        unsigned char *data;
} RawImage;

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
        RawImage *raw_icon;
        char *msg;            /* formatted message */
        char *category;
        char *text_to_render;
        const char *format;
        char *dbus_client;
        gint64 start;
        gint64 timestamp;
        gint64 timeout;
        int urgency;
        enum markup_mode markup;
        bool redisplayed;       /* has been displayed before? */
        int id;
        int dup_count;
        int displayed_height;
        const char *color_strings[3];
        bool first_render;
        bool transient;

        int progress;           /* percentage + 1, 0 to hide */
        int history_ignore;
        const char *script;
        char *urls;
        Actions *actions;
} notification;

notification *notification_create(void);
void notification_init(notification *n);
void rawimage_free(RawImage *i);
void notification_free(notification *n);
int notification_cmp(const void *a, const void *b);
int notification_cmp_data(const void *a, const void *b, void *data);
int notification_is_duplicate(const notification *a, const notification *b);
void notification_run_script(notification *n);
void notification_print(notification *n);
void notification_replace_single_field(char **haystack, char **needle, const char *replacement, enum markup_mode markup_mode);
void notification_update_text_to_render(notification *n);
void notification_do_action(notification *n);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

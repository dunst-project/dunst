/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_NOTIFICATION_H
#define DUNST_NOTIFICATION_H

#include <glib.h>
#include <stdbool.h>

#include "settings.h"

#define DUNST_NOTIF_MAX_CHARS 5000

enum behavior_fullscreen {
        FS_NULL,      //!< Invalid value
        FS_DELAY,     //!< Delay the notification until leaving fullscreen mode
        FS_PUSHBACK,  //!< When entering fullscreen mode, push the notification back to waiting
        FS_SHOW,      //!< Show the message when in fullscreen mode
};

/// Representing the urgencies according to the notification spec
enum urgency {
        URG_NONE = -1, /**< Urgency not set (invalid) */
        URG_MIN = 0,   /**< Minimum value, useful for boundary checking */
        URG_LOW = 0,   /**< Low urgency */
        URG_NORM = 1,  /**< Normal urgency */
        URG_CRIT = 2,  /**< Critical urgency */
        URG_MAX = 2,   /**< Maximum value, useful for boundary checking */
};

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
        int id;
        char *dbus_client;

        char *appname;
        char *summary;
        char *body;
        char *category;
        enum urgency urgency;

        char *icon;          /**< plain icon information (may be a path or just a name) */
        RawImage *raw_icon;  /**< passed icon data of notification, takes precedence over icon */

        gint64 start;      /**< begin of current display */
        gint64 timestamp;  /**< arrival time */
        gint64 timeout;    /**< time to display */

        Actions *actions;

        enum markup_mode markup;
        const char *format;
        const char *script;
        char *colors[3];

        /* Hints */
        bool transient;     /**< timeout albeit user is idle */
        int progress;       /**< percentage (-1: undefined) */
        int history_ignore; /**< push to history or free directly */

        /* internal */
        bool redisplayed;       /**< has been displayed before? */
        bool first_render;      /**< markup has been rendered before? */
        int dup_count;          /**< amount of duplicate notifications stacked onto this */
        int displayed_height;
        enum behavior_fullscreen fullscreen; //!< The instruction what to do with it, when desktop enters fullscreen

        /* derived fields */
        char *msg;            /**< formatted message */
        char *text_to_render; /**< formatted message (with age and action indicators) */
        char *urls;           /**< urllist delimited by '\\n' */
} notification;

notification *notification_create(void);
void notification_init(notification *n);
void actions_free(Actions *a);
void rawimage_free(RawImage *i);
void notification_free(notification *n);
int notification_cmp(const notification *a, const notification *b);
int notification_cmp_data(const void *va, const void *vb, void *data);
int notification_is_duplicate(const notification *a, const notification *b);
void notification_run_script(notification *n);
void notification_print(notification *n);
void notification_replace_single_field(char **haystack,
                                       char **needle,
                                       const char *replacement,
                                       enum markup_mode markup_mode);
void notification_update_text_to_render(notification *n);
void notification_do_action(notification *n);

const char *notification_urgency_to_string(enum urgency urgency);

/**
 * Return the string representation for fullscreen behavior
 *
 * @param in the #behavior_fullscreen enum value to represent
 * @return the string representation for `in`
 */
const char *enum_to_string_fullscreen(enum behavior_fullscreen in);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

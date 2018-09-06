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
        bool script_run;        /**< Has the script been executed already? */

        /* derived fields */
        char *msg;            /**< formatted message */
        char *text_to_render; /**< formatted message (with age and action indicators) */
        char *urls;           /**< urllist delimited by '\\n' */
} notification;

/**
 * Create notification struct and initialise all fields with either
 *  - the default (if it's not needed to be freed later)
 *  - its undefined representation (NULL, -1)
 *
 * This function is guaranteed to return a valid pointer.
 * @returns The generated notification
 */
notification *notification_create(void);

/**
 * Sanitize values of notification, apply all matching rules
 * and generate derived fields.
 *
 * @param n: the notification to sanitize
 */
void notification_init(notification *n);

/**
 * Free the actions structure
 *
 * @param a (nullable): Pointer to #Actions
 */
void actions_free(Actions *a);

/**
 * Free a #RawImage
 *
 * @param i (nullable): pointer to #RawImage
 */
void rawimage_free(RawImage *i);

/**
 * Free the memory used by the given notification.
 *
 * @param n (nullable): pointer to #notification
 */
void notification_free(notification *n);

/**
 * Helper function to compare two given notifications.
 */
int notification_cmp(const notification *a, const notification *b);

/**
 * Wrapper for notification_cmp to match glib's
 * compare functions signature.
 */
int notification_cmp_data(const void *va, const void *vb, void *data);

int notification_is_duplicate(const notification *a, const notification *b);

/**
 * Run the script associated with the
 * given notification.
 *
 * If the script of the notification has been executed already and
 * settings.always_run_script is not set, do nothing.
 */
void notification_run_script(notification *n);
/**
 * print a human readable representation
 * of the given notification to stdout.
 */
void notification_print(notification *n);

/**
 * Replace the two chars where **needle points
 * with a quoted "replacement", according to the markup settings.
 *
 * The needle is a double pointer and gets updated upon return
 * to point to the first char, which occurs after replacement.
 */
void notification_replace_single_field(char **haystack,
                                       char **needle,
                                       const char *replacement,
                                       enum markup_mode markup_mode);
void notification_update_text_to_render(notification *n);

/**
 * If the notification has exactly one action, or one is marked as default,
 * invoke it. If there are multiple and no default, open the context menu. If
 * there are no actions, proceed similarly with urls.
 */
void notification_do_action(notification *n);

const char *notification_urgency_to_string(const enum urgency urgency);

/**
 * Return the string representation for fullscreen behavior
 *
 * @param in the #behavior_fullscreen enum value to represent
 * @return the string representation for `in`
 */
const char *enum_to_string_fullscreen(enum behavior_fullscreen in);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

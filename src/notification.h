/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_NOTIFICATION_H
#define DUNST_NOTIFICATION_H

#include <glib.h>
#include <stdbool.h>
#include <pango/pango-layout.h>
#include <cairo.h>

#include "markup.h"

#define DUNST_NOTIF_MAX_CHARS 50000

enum icon_position {
        ICON_LEFT,
        ICON_RIGHT,
        ICON_TOP,
        ICON_OFF
};

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

typedef struct _notification_private NotificationPrivate;

struct notification_colors {
        char *frame;
        char *bg;
        char *fg;
        char *highlight;
};

struct notification {
        NotificationPrivate *priv;
        int id;
        char *dbus_client;
        bool dbus_valid;

        char *appname;
        char *summary;
        char *body;
        char *category;
        char *desktop_entry;     /**< The desktop entry hint sent via every GApplication */
        enum urgency urgency;

        cairo_surface_t *icon;         /**< The raw cached icon data used to draw */
        char *icon_id;           /**< Plain icon information, which acts as the icon's id.
                                      May be a hash for a raw icon or a name/path for a regular app icon. */
        char *iconname;          /**< plain icon information (may be a path or just a name) as recieved from dbus.
                                   Use this to compare the icon name with rules. May also be modified by rules.*/
        char *icon_path;         /**< Full path to the notification's icon. */
        char *default_icon_name; /**< The icon that is used when no other icon is available. */
        int icon_size;           /**< Size of the icon used for searching the right icon. */
        enum icon_position icon_position;       /**< Icon position (enum left,right,top,off). */

        gint64 start;      /**< begin of current display (in milliseconds) */
        gint64 timestamp;  /**< arrival time (in milliseconds) */
        gint64 timeout;    /**< time to display (in milliseconds) */
        int locked;     /**< If non-zero the notification is locked **/
        PangoAlignment progress_bar_alignment; /**< Horizontal alignment of the progress bar **/

        GHashTable *actions;
        char *default_action_name; /**< The name of the action to be invoked on do_action */

        enum markup_mode markup;
        const char *format;
        const char **scripts;
        int script_count;
        struct notification_colors colors;

        char *stack_tag;    /**< stack notifications by tag */

        /* Hints */
        bool transient;     /**< timeout albeit user is idle */
        int progress;       /**< percentage (-1: undefined) */
        int history_ignore; /**< push to history or free directly */
        int skip_display;   /**< insert notification into history, skipping initial waiting and display */

        /* internal */
        bool redisplayed;       /**< has been displayed before? */
        bool first_render;      /**< markup has been rendered before? */
        int dup_count;          /**< amount of duplicate notifications stacked onto this */
        int displayed_height;
        enum behavior_fullscreen fullscreen; //!< The instruction what to do with it, when desktop enters fullscreen
        bool script_run;        /**< Has the script been executed already? */
        guint8 marked_for_closure;
        bool word_wrap;
        PangoEllipsizeMode ellipsize;
        PangoAlignment alignment;
        bool hide_text;

        /* derived fields */
        char *msg;            /**< formatted message */
        char *text_to_render; /**< formatted message (with age and action indicators) */
        char *urls;           /**< urllist delimited by '\\n' */
};

/**
 * Create notification struct and initialise all fields with either
 *  - the default (if it's not needed to be freed later)
 *  - its undefined representation (NULL, -1)
 *
 * The reference counter is set to 1.
 *
 * This function is guaranteed to return a valid pointer.
 * @returns The generated notification
 */
struct notification *notification_create(void);

/**
 * Retrieve the current reference count of the notification
 */
gint notification_refcount_get(struct notification *n);

/**
 * Increase the reference counter of the notification.
 */
void notification_ref(struct notification *n);

/**
 * Sanitize values of notification, apply all matching rules
 * and generate derived fields.
 *
 * @param n: the notification to sanitize
 */
void notification_init(struct notification *n);

/**
 * Decrease the reference counter of the notification.
 *
 * If the reference count drops to 0, the object gets freed.
 */
void notification_unref(struct notification *n);

/**
 * Helper function to compare two given notifications.
 */
int notification_cmp(const struct notification *a, const struct notification *b);

/**
 * Wrapper for notification_cmp to match glib's
 * compare functions signature.
 */
int notification_cmp_data(const void *va, const void *vb, void *data);

bool notification_is_duplicate(const struct notification *a, const struct notification *b);

bool notification_is_locked(struct notification *n);

struct notification *notification_lock(struct notification *n);

struct notification *notification_unlock(struct notification *n);

/**
 * Transfer the image surface of \p from to \p to. The image surface is
 * transfered only if the icon names match. When the icon is transferred, it is
 * removed from the old notification to make sure it's not freed twice.
 *
 * @param from The notification of which the icon surface is removed.
 * @param to The notification that receives the icon surface.
 */
void notification_transfer_icon(struct notification *from, struct notification *to);

/**Replace the current notification's icon with the icon specified by path.
 *
 * Removes the reference for the previous icon automatically and will also free the
 * iconname field. So passing n->iconname as new_icon is invalid.
 *
 * @param n the notification to replace the icon
 * @param new_icon The path of the new icon. May be an absolute path or an icon name.
 */
void notification_icon_replace_path(struct notification *n, const char *new_icon);

/**Replace the current notification's icon with the raw icon given in the GVariant.
 *
 * Removes the reference for the previous icon automatically.
 *
 * @param n the notification to replace the icon
 * @param new_icon The icon's data. Has to be in the format of the notification spec.
 */
void notification_icon_replace_data(struct notification *n, GVariant *new_icon);

/**
 * Run the script associated with the
 * given notification.
 *
 * If the script of the notification has been executed already and
 * settings.always_run_script is not set, do nothing.
 */
void notification_run_script(struct notification *n);
/**
 * print a human readable representation
 * of the given notification to stdout.
 */
void notification_print(const struct notification *n);

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

void notification_update_text_to_render(struct notification *n);

/**
 * If the notification has an action named n->default_action_name or there is only one
 * action and n->default_action_name is set to "default", invoke it. If there is no
 * such action, open the context menu if threre are other actions. Otherwise, do nothing.
 */
void notification_do_action(struct notification *n);

/**
 * If the notification has exactly one url, invoke it. If there are multiple,
 * open the context menu. If there are no urls, do nothing.
 */
void notification_open_url(struct notification *n);

/**
 * Open the context menu for the notification.
 * 
 * Convenience function that creates the GList and passes it to context_menu_for().
 */
void notification_open_context_menu(struct notification *n);

/**
 * Remove all client action data from the notification.
 *
 * This should be called after a notification is closed to avoid showing
 * actions that will not work anymore since the client has stopped listening
 * for them.
 */
void notification_invalidate_actions(struct notification *n);

const char *notification_urgency_to_string(const enum urgency urgency);

/**
 * Return the string representation for fullscreen behavior
 *
 * @param in the #behavior_fullscreen enum value to represent
 * @return the string representation for `in`
 */
const char *enum_to_string_fullscreen(enum behavior_fullscreen in);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#ifndef DUNST_DBUS_H
#define DUNST_DBUS_H

#include "dunst.h"
#include "notification.h"

/// The reasons according to the notification spec
enum reason {
        REASON_MIN = 1,   /**< Minimum value, useful for boundary checking */
        REASON_TIME = 1,  /**< The notification timed out */
        REASON_USER = 2,  /**< The user closed the notification */
        REASON_SIG = 3,   /**< The daemon received a `NotificationClose` signal */
        REASON_UNDEF = 4, /**< Undefined reason not matching the previous ones */
        REASON_MAX = 4,   /**< Maximum value, useful for boundary checking */
};

int dbus_init(void);
void dbus_teardown(int id);
void signal_notification_closed(struct notification *n, enum reason reason);
void signal_action_invoked(const struct notification *n, const char *identifier);
void signal_length_propertieschanged();

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#ifndef DUNST_QUEUE_H
#define DUNST_QUEUE_H

#include "notification.h"

extern GQueue *queue;
extern GQueue *displayed;
extern GQueue *history;
extern unsigned int displayed_limit;

/*
 * Initialise neccessary queues
 */
void queues_init(void);

/*
 * Set maximum notification count to display
 * and store in displayed queue
 */
void queues_displayed_limit(unsigned int limit);

/*
 * Insert a fully initialized notification into queues
 * Respects stack_duplicates, and notification replacement
 *
 * If replaces_id != 0, n replaces notification with id replaces_id
 * If replaces_id == 0, n gets occupies a new position
 *
 * Returns the assigned notification id
 */
int queues_notification_insert(notification *n, int replaces_id);

/*
 * Replace the notification which matches the id field of
 * the new notification. The given notification is inserted
 * right in the same position as the old notification.
 *
 * Returns true, if a matching notification has been found
 * and is replaced. Else false.
 */
bool notification_replace_by_id(notification *new);

/*
 * Close the notification that has id.
 *
 * After closing, call wake_up to remove the notification from UI
 *
 * reasons:
 * -1 -> notification is a replacement, no NotificationClosed signal emitted
 *  1 -> the notification expired
 *  2 -> the notification was dismissed by the user_data
 *  3 -> The notification was closed by a call to CloseNotification
 */
int notification_close_by_id(int id, int reason);

/* Close the given notification. SEE notification_close_by_id. */
int notification_close(notification *n, int reason);

void history_pop(void);
void history_push(notification *n);
void move_all_to_history(void);

/*
 * Remove all notifications from all lists
 * and free the notifications
 */
void teardown_queues(void);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

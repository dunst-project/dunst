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
bool queues_notification_replace_id(notification *new);

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
int queues_notification_close_id(int id, int reason);

/* Close the given notification. SEE queues_notification_close_id. */
int queues_notification_close(notification *n, int reason);

void history_pop(void);
void history_push(notification *n);
void move_all_to_history(void);

/*
 * Check timeout of each notification and close it, if neccessary
 */
void queues_check_timeouts(bool idle);

/*
 * Move inserted notifications from waiting queue to displayed queue
 * and show them. In displayed queue, the amount of elements is limited
 * to the amount set via queues_displayed_limit
 */
void queues_update();

/*
 * Return the distance to the next event in the queue,
 * which forces an update visible to the user
 *
 * This may be:
 *
 * - notification hits timeout
 * - notification's age second changes
 * - notification's age threshold is hit
 */
gint64 queues_get_next_datachange(gint64 time);

/*
 * Pause queue-management of dunst
 *   pause_on  = paused (no notifications displayed)
 *   pause_off = running
 *
 * Calling update_lists is neccessary
 */
void queues_pause_on(void);
void queues_pause_off(void);
bool queues_pause_status(void);

/*
 * Remove all notifications from all lists
 * and free the notifications
 */
void teardown_queues(void);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

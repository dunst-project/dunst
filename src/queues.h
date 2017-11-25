/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#ifndef DUNST_QUEUE_H
#define DUNST_QUEUE_H

#include "dbus.h"
#include "notification.h"

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
 * Return read only list of notifications
 */
const GList *queues_get_displayed();

/*
 * Returns the current amount of notifications,
 * which are shown, waiting or already in history
 */
unsigned int queues_length_waiting();
unsigned int queues_length_displayed();
unsigned int queues_length_history();

/*
 * Insert a fully initialized notification into queues
 * Respects stack_duplicates, and notification replacement
 *
 * If replaces_id != 0, n replaces notification with id replaces_id
 * If replaces_id == 0, n gets occupies a new position
 *
 * Returns the assigned notification id
 * If returned id == 0, the message was dismissed
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
 * Close the notification that has n->id == id
 *
 * Sends a signal and pushes it automatically to history.
 *
 * After closing, call wake_up to synchronize the queues with the UI
 * (which closes the notification on screen)
 */
int queues_notification_close_id(int id, enum reason reason);

/* Close the given notification. SEE queues_notification_close_id.
 *
 * @n: (transfer full): The notification to close
 * @reason: The reason to close
 * */
int queues_notification_close(notification *n, enum reason reason);

/*
 * Pushed the latest notification of history to the displayed queue
 * and removes it from history
 */
void queues_history_pop(void);

/*
 * Push a single notification to history
 * The given notification has to be removed its queue
 *
 * @n: (transfer full): The notification to push to history
 */
void queues_history_push(notification *n);

/*
 * Push all waiting and displayed notifications to history
 */
void queues_history_push_all(void);

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

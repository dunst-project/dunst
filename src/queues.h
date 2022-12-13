/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

/**
 * @file src/queues.h
 */

#ifndef DUNST_QUEUE_H
#define DUNST_QUEUE_H

#include "dbus.h"
#include "dunst.h"
#include "notification.h"

/**
 * Initialise necessary queues
 *
 * @pre Do not call consecutively to avoid memory leaks
 *      or assure to have queues_teardown() executed before
 */
void queues_init(void);

/**
 * Receive the current list of displayed notifications
 *
 * @return read only list of notifications
 */
GList *queues_get_displayed(void);

/**
 * Recieve the list of all notifications encountered
 *
 * @return read only list of notifications
 */
GList *queues_get_history(void);

/**
 * Get the highest notification in line
 *
 * @returns the first notification in waiting
 * @retval NULL: waiting is empty
 */
struct notification *queues_get_head_waiting(void);

/**
 * Returns the current amount of notifications,
 * which are waiting to get displayed
 */
unsigned int queues_length_waiting(void);

/**
 * Returns the current amount of notifications,
 * which are shown in the UI
 */
unsigned int queues_length_displayed(void);

/**
 * Returns the current amount of notifications,
 * which are already in history
 */
unsigned int queues_length_history(void);

/**
 * Insert a fully initialized notification into queues
 *
 * Respects stack_duplicates, and notification replacement
 *
 * @param n the notification to insert
 *
 * - If n->id != 0, n replaces notification n with id n->id
 * - If n->id == 0, n gets a new id assigned
 *
 * @returns The new value of `n->id`
 * @retval 0: the notification was dismissed and freed
 */
int queues_notification_insert(struct notification *n);

/**
 * Replace the notification which matches the id field of
 * the new notification. The given notification is inserted
 * right in the same position as the old notification.
 *
 * @param new replacement for the old notification
 *
 * @retval true: a matching notification has been found and is replaced
 * @retval false: otherwise
 */
bool queues_notification_replace_id(struct notification *new);

/**
 * Close the notification that has n->id == id
 *
 * Sends a signal and pushes the selected notification automatically to history.
 *
 * @param id The id of the notification to close
 * @param reason The #reason to close
 *
 * @post Call wake_up() to synchronize the queues with the UI
 *       (which closes the notification on screen)
 */
void queues_notification_close_id(int id, enum reason reason);

/**
 * Close the given notification. \see queues_notification_close_id().
 *
 * @param n (transfer full) The notification to close
 * @param reason The #reason to close
 * */
void queues_notification_close(struct notification *n, enum reason reason);

/**
 * Removes all notifications from history
 */
void queues_history_clear(void);

/**
 * Pushes the latest notification of history to the displayed queue
 * and removes it from history
 */
void queues_history_pop(void);

/**
 * Pushes the latest notification found in the history buffer identified by
 * it's assigned id
 */
void queues_history_pop_by_id(unsigned int id);

/**
 * Push a single notification to history
 * The given notification has to be removed its queue
 *
 * @param n (transfer full) The notification to push to history
 */
void queues_history_push(struct notification *n);

/**
 * Push all waiting and displayed notifications to history
 */
void queues_history_push_all(void);

/**
 * Removes an notification identified by the given id from the history 
 */
void queues_history_remove_by_id(unsigned int id);

/**
 * Move inserted notifications from waiting queue to displayed queue
 * and show them. In displayed queue, the amount of elements is limited
 * to the amount set via queues_displayed_limit()
 *
 * @post Call wake_up() to synchronize the queues with the UI
 *       (which closes old and shows new notifications on screen)
 *
 * @param status the current status of dunst
 * @param time the current time
 */
void queues_update(struct dunst_status status, gint64 time);

/**
 * Calculate the distance to the next event, when an element in the
 * queues changes
 *
 * @param time the current time
 *
 * @return the distance to the next event in the queue, which forces
 *         an update visible to the user. This may be:
 *             - notification hits timeout
 *             - notification's age second changes
 *             - notification's age threshold is hit
 */
gint64 queues_get_next_datachange(gint64 time);

/**
 * Get the notification which has the given id in the displayed and waiting queue or
 * NULL if not found
 *
 * @param id the id searched for.
 *
 * @return the `id` notification  or NULL
 */
struct notification* queues_get_by_id(int id);


/**
 * Remove all notifications from all list and free the notifications
 *
 * @pre At least one time queues_init() called
 */
void queues_teardown(void);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

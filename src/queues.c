/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

/**
 * @file queues.c
 * @brief All important functions to handle the notification queues for
 * history, entrance and currently displayed ones.
 *
 * Every method requires to have executed queues_init() at the start.
 *
 * A read only representation of the queue with the current notifications
 * can get acquired by calling queues_get_displayed().
 *
 * When ending the program or resetting the queues, tear down the stack with
 * queues_teardown(). (And reinit with queues_init() if needed.)
 */
#include "queues.h"

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "notification.h"
#include "settings.h"
#include "utils.h"

/* notification lists */
static GQueue *waiting   = NULL; /**< all new notifications get into here */
static GQueue *displayed = NULL; /**< currently displayed notifications */
static GQueue *history   = NULL; /**< history of displayed notifications */

int next_notification_id = 1;
bool pause_displayed = false;

static bool queues_stack_duplicate(notification *n);

/* see queues.h */
void queues_init(void)
{
        history   = g_queue_new();
        displayed = g_queue_new();
        waiting   = g_queue_new();
}

/* see queues.h */
const GList *queues_get_displayed(void)
{
        return g_queue_peek_head_link(displayed);
}

/* see queues.h */
const notification *queues_get_head_waiting(void)
{
        if (waiting->length == 0)
                return NULL;
        return g_queue_peek_head(waiting);
}

/* see queues.h */
unsigned int queues_length_waiting(void)
{
        return waiting->length;
}

/* see queues.h */
unsigned int queues_length_displayed(void)
{
        return displayed->length;
}

/* see queues.h */
unsigned int queues_length_history(void)
{
        return history->length;
}

/**
 * Swap two given queue elements. The element's data has to be a notification.
 *
 * @pre { elemA has to be part of queueA. }
 * @pre { elemB has to be part of queueB. }
 *
 * @param queueA The queue, which elemB's data will get inserted
 * @param elemA  The element, which will get removed from queueA
 * @param queueB The queue, which elemA's data will get inserted
 * @param elemB  The element, which will get removed from queueB
 */
static void queues_swap_notifications(GQueue *queueA,
                                      GList  *elemA,
                                      GQueue *queueB,
                                      GList  *elemB)
{
        notification *toB = elemA->data;
        notification *toA = elemB->data;

        g_queue_delete_link(queueA, elemA);
        g_queue_delete_link(queueB, elemB);

        if (toA)
                g_queue_insert_sorted(queueA, toA, notification_cmp_data, NULL);
        if (toB)
                g_queue_insert_sorted(queueB, toB, notification_cmp_data, NULL);
}

/**
 * Check if a notification is eligible to get shown.
 *
 * @param n          The notification to check
 * @param fullscreen True if a fullscreen window is currently active
 * @param visible    True if the notification is currently displayed
 */
static bool queues_notification_is_ready(const notification *n, bool fullscreen, bool visible)
{
        if (fullscreen && visible)
                return n && n->fullscreen != FS_PUSHBACK;
        else if (fullscreen && !visible)
                return n && n->fullscreen == FS_SHOW;
        else
                return true;
}

/* see queues.h */
int queues_notification_insert(notification *n)
{

        /* do not display the message, if the message is empty */
        if (strlen(n->msg) == 0) {
                if (settings.always_run_script) {
                        notification_run_script(n);
                }
                LOG_M("Skipping notification: '%s' '%s'", n->body, n->summary);
                return 0;
        }
        /* Do not insert the message if it's a command */
        if (strcmp("DUNST_COMMAND_PAUSE", n->summary) == 0) {
                pause_displayed = true;
                return 0;
        }
        if (strcmp("DUNST_COMMAND_RESUME", n->summary) == 0) {
                pause_displayed = false;
                return 0;
        }
        if (strcmp("DUNST_COMMAND_TOGGLE", n->summary) == 0) {
                pause_displayed = !pause_displayed;
                return 0;
        }

        if (n->id == 0) {
                n->id = ++next_notification_id;
                if (!settings.stack_duplicates || !queues_stack_duplicate(n))
                        g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
        } else {
                if (!queues_notification_replace_id(n))
                        g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
        }

        if (settings.print_notifications)
                notification_print(n);

        return n->id;
}

/**
 * Replaces duplicate notification and stacks it
 *
 * @return true, if notification got stacked
 * @return false, if notification did not get stacked
 */
static bool queues_stack_duplicate(notification *n)
{
        for (GList *iter = g_queue_peek_head_link(displayed); iter;
             iter = iter->next) {
                notification *orig = iter->data;
                if (notification_is_duplicate(orig, n)) {
                        /* If the progress differs, probably notify-send was used to update the notification
                         * So only count it as a duplicate, if the progress was not the same.
                         * */
                        if (orig->progress == n->progress) {
                                orig->dup_count++;
                        } else {
                                orig->progress = n->progress;
                        }

                        iter->data = n;

                        n->start = time_monotonic_now();

                        n->dup_count = orig->dup_count;

                        signal_notification_closed(orig, 1);

                        notification_free(orig);
                        return true;
                }
        }

        for (GList *iter = g_queue_peek_head_link(waiting); iter;
             iter = iter->next) {
                notification *orig = iter->data;
                if (notification_is_duplicate(orig, n)) {
                        /* If the progress differs, probably notify-send was used to update the notification
                         * So only count it as a duplicate, if the progress was not the same.
                         * */
                        if (orig->progress == n->progress) {
                                orig->dup_count++;
                        } else {
                                orig->progress = n->progress;
                        }
                        iter->data = n;

                        n->dup_count = orig->dup_count;

                        signal_notification_closed(orig, 1);

                        notification_free(orig);
                        return true;
                }
        }

        return false;
}

/* see queues.h */
bool queues_notification_replace_id(notification *new)
{

        for (GList *iter = g_queue_peek_head_link(displayed);
                    iter;
                    iter = iter->next) {
                notification *old = iter->data;
                if (old->id == new->id) {
                        iter->data = new;
                        new->start = time_monotonic_now();
                        new->dup_count = old->dup_count;
                        notification_run_script(new);
                        notification_free(old);
                        return true;
                }
        }

        for (GList *iter = g_queue_peek_head_link(waiting);
                    iter;
                    iter = iter->next) {
                notification *old = iter->data;
                if (old->id == new->id) {
                        iter->data = new;
                        new->dup_count = old->dup_count;
                        notification_free(old);
                        return true;
                }
        }
        return false;
}

/* see queues.h */
void queues_notification_close_id(int id, enum reason reason)
{
        notification *target = NULL;

        for (GList *iter = g_queue_peek_head_link(displayed); iter;
             iter = iter->next) {
                notification *n = iter->data;
                if (n->id == id) {
                        g_queue_remove(displayed, n);
                        target = n;
                        break;
                }
        }

        for (GList *iter = g_queue_peek_head_link(waiting); iter;
             iter = iter->next) {
                notification *n = iter->data;
                if (n->id == id) {
                        assert(target == NULL);
                        g_queue_remove(waiting, n);
                        target = n;
                        break;
                }
        }

        if (target) {
                //Don't notify clients if notification was pulled from history
                if (!target->redisplayed)
                        signal_notification_closed(target, reason);
                queues_history_push(target);
        }
}

/* see queues.h */
void queues_notification_close(notification *n, enum reason reason)
{
        assert(n != NULL);
        queues_notification_close_id(n->id, reason);
}

/* see queues.h */
void queues_history_pop(void)
{
        if (g_queue_is_empty(history))
                return;

        notification *n = g_queue_pop_tail(history);
        n->redisplayed = true;
        n->start = 0;
        n->timeout = settings.sticky_history ? 0 : n->timeout;
        g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
}

/* see queues.h */
void queues_history_push(notification *n)
{
        if (!n->history_ignore) {
                if (settings.history_length > 0 && history->length >= settings.history_length) {
                        notification *to_free = g_queue_pop_head(history);
                        notification_free(to_free);
                }

                g_queue_push_tail(history, n);
        } else {
                notification_free(n);
        }
}

/* see queues.h */
void queues_history_push_all(void)
{
        while (displayed->length > 0) {
                queues_notification_close(g_queue_peek_head_link(displayed)->data, REASON_USER);
        }

        while (waiting->length > 0) {
                queues_notification_close(g_queue_peek_head_link(waiting)->data, REASON_USER);
        }
}

/* see queues.h */
void queues_check_timeouts(bool idle, bool fullscreen)
{
        /* nothing to do */
        if (displayed->length == 0)
                return;

        bool is_idle = fullscreen ? false : idle;

        GList *iter = g_queue_peek_head_link(displayed);
        while (iter) {
                notification *n = iter->data;

                /*
                 * Update iter to the next item before we either exit the
                 * current iteration of the loop or potentially delete the
                 * notification which would invalidate the pointer.
                 */
                iter = iter->next;

                /* don't timeout when user is idle */
                if (is_idle && !n->transient) {
                        n->start = time_monotonic_now();
                        continue;
                }

                /* skip hidden and sticky messages */
                if (n->start == 0 || n->timeout == 0) {
                        continue;
                }

                /* remove old message */
                if (time_monotonic_now() - n->start > n->timeout) {
                        queues_notification_close(n, REASON_TIME);
                }
        }
}

/* see queues.h */
void queues_update(bool fullscreen)
{
        if (pause_displayed) {
                while (displayed->length > 0) {
                        g_queue_insert_sorted(
                            waiting, g_queue_pop_head(displayed), notification_cmp_data, NULL);
                }
                return;
        }

        /* move notifications back to queue, which are set to pushback */
        if (fullscreen) {
                GList *iter = g_queue_peek_head_link(displayed);
                while (iter) {
                        notification *n = iter->data;
                        GList *nextiter = iter->next;

                        if (n->fullscreen == FS_PUSHBACK){
                                g_queue_delete_link(displayed, iter);
                                g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
                        }

                        iter = nextiter;
                }
        }

        int cur_displayed_limit;
        if (settings.geometry.h == 0)
                cur_displayed_limit = INT_MAX;
        else if (   settings.indicate_hidden
                 && settings.geometry.h > 1
                 && displayed->length + waiting->length > settings.geometry.h)
                cur_displayed_limit = settings.geometry.h-1;
        else
                cur_displayed_limit = settings.geometry.h;

        /* move notifications from queue to displayed */
        GList *iter = g_queue_peek_head_link(waiting);
        while (displayed->length < cur_displayed_limit && iter) {
                notification *n = iter->data;
                GList *nextiter = iter->next;

                if (!n)
                        return;

                if (!queues_notification_is_ready(n, fullscreen, false)) {
                        iter = nextiter;
                        continue;
                }

                n->start = time_monotonic_now();
                notification_run_script(n);

                g_queue_delete_link(waiting, iter);
                g_queue_insert_sorted(displayed, n, notification_cmp_data, NULL);

                iter = nextiter;
        }

        /* if necessary, push the overhanging notifications from displayed to waiting again */
        while (displayed->length > cur_displayed_limit) {
                notification *n = g_queue_pop_tail(displayed);
                g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL); //TODO: actually it should be on the head if unsorted
        }

        /* If displayed is actually full, let the more important notifications
         * from waiting seep into displayed.
         */
        if (settings.sort) {
                GList *i_waiting, *i_displayed;

                while (   (i_waiting   = g_queue_peek_head_link(waiting))
                       && (i_displayed = g_queue_peek_tail_link(displayed))) {

                        while (i_waiting && ! queues_notification_is_ready(i_waiting->data, fullscreen, true)) {
                                i_waiting = i_waiting->prev;
                        }

                        if (i_waiting && notification_cmp(i_displayed->data, i_waiting->data) > 0) {
                                notification *todisp = i_waiting->data;

                                todisp->start = time_monotonic_now();
                                notification_run_script(todisp);

                                queues_swap_notifications(displayed, i_displayed, waiting, i_waiting);
                        } else {
                                break;
                        }
                }
        }
}

/* see queues.h */
gint64 queues_get_next_datachange(gint64 time)
{
        gint64 sleep = G_MAXINT64;

        for (GList *iter = g_queue_peek_head_link(displayed); iter;
                        iter = iter->next) {
                notification *n = iter->data;
                gint64 ttl = n->timeout - (time - n->start);

                if (n->timeout > 0) {
                        if (ttl > 0)
                                sleep = MIN(sleep, ttl);
                        else
                                // while we're processing, the notification already timed out
                                return 0;
                }

                if (settings.show_age_threshold >= 0) {
                        gint64 age = time - n->timestamp;

                        if (age > settings.show_age_threshold)
                                // sleep exactly until the next shift of the second happens
                                sleep = MIN(sleep, ((G_USEC_PER_SEC) - (age % (G_USEC_PER_SEC))));
                        else if (n->timeout == 0 || ttl > settings.show_age_threshold)
                                sleep = MIN(sleep, settings.show_age_threshold);
                }
        }

        return sleep != G_MAXINT64 ? sleep : -1;
}

/* see queues.h */
void queues_pause_on(void)
{
        pause_displayed = true;
}

/* see queues.h */
void queues_pause_off(void)
{
        pause_displayed = false;
}

/* see queues.h */
bool queues_pause_status(void)
{
        return pause_displayed;
}

/**
 * Helper function for teardown_queues() to free a single notification
 *
 * @param data The notification to free
 */
static void teardown_notification(gpointer data)
{
        notification *n = data;
        notification_free(n);
}

/* see queues.h */
void teardown_queues(void)
{
        g_queue_free_full(history, teardown_notification);
        g_queue_free_full(displayed, teardown_notification);
        g_queue_free_full(waiting, teardown_notification);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

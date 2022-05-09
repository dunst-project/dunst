/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

/**
 * @file src/queues.c
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

#include "dunst.h"
#include "log.h"
#include "notification.h"
#include "settings.h"
#include "utils.h"
#include "output.h" // For checking if wayland is active.

/* notification lists */
static GQueue *waiting   = NULL; /**< all new notifications get into here */
static GQueue *displayed = NULL; /**< currently displayed notifications */
static GQueue *history   = NULL; /**< history of displayed notifications */

int next_notification_id = 1;

static bool queues_stack_duplicate(struct notification *n);
static bool queues_stack_by_tag(struct notification *n);

/* see queues.h */
void queues_init(void)
{
        history   = g_queue_new();
        displayed = g_queue_new();
        waiting   = g_queue_new();
}

/* see queues.h */
GList *queues_get_displayed(void)
{
        return g_queue_peek_head_link(displayed);
}

/* see queues.h */
struct notification *queues_get_head_waiting(void)
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

/* see queues.h */
GList *queues_get_history(void)
{
        return g_queue_peek_head_link(history);
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
        struct notification *toB = elemA->data;
        struct notification *toA = elemB->data;

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
 * @param n      The notification to check
 * @param status The current status of dunst
 * @param shown  True if the notification is currently displayed
 */
static bool queues_notification_is_ready(const struct notification *n, struct dunst_status status, bool shown)
{
        ASSERT_OR_RET(status.running, false);
        if (status.fullscreen && shown)
                return n && n->fullscreen != FS_PUSHBACK;
        else if (status.fullscreen && !shown)
                return n && n->fullscreen == FS_SHOW;
        else
                return true;
}

/**
 * Check if a notification has timed out
 *
 * @param n the notification to check
 * @param status the current status of dunst
 * @param time the current time
 * @retval true: the notification is timed out
 * @retval false: otherwise
 */
static bool queues_notification_is_finished(struct notification *n, struct dunst_status status, gint64 time)
{
        assert(n);

        if (n->skip_display && !n->redisplayed)
                return true;

        if (n->timeout == 0) // sticky
                return false;

        bool is_idle = status.fullscreen ? false : status.idle;

        /* don't timeout when user is idle */
        if (is_idle && !n->transient) {
                n->start = time_monotonic_now();
                return false;
        }

        /* remove old message */
        if (time - n->start > n->timeout) {
                return true;
        }

        return false;
}

/* see queues.h */
int queues_notification_insert(struct notification *n)
{
        /* do not display the message, if the message is empty */
        if (STR_EMPTY(n->msg)) {
                if (settings.always_run_script) {
                        notification_run_script(n);
                }
                LOG_M("Skipping notification: '%s' '%s'", n->body, n->summary);
                return 0;
        }

        bool inserted = false;
        if (n->id != 0) {
                if (!queues_notification_replace_id(n)) {
                        // Requested id was not valid, but play nice and assign it anyway
                        g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
                }
                inserted = true;
        } else {
                n->id = ++next_notification_id;
        }

        if (!inserted && STR_FULL(n->stack_tag) && queues_stack_by_tag(n))
                inserted = true;

        if (!inserted && settings.stack_duplicates && queues_stack_duplicate(n))
                inserted = true;

        if (!inserted)
                g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);

        if (!n->icon) {
                notification_icon_replace_path(n, n->iconname);
        }

        if (settings.print_notifications)
                notification_print(n);

        return n->id;
}

/**
 * Replaces duplicate notification and stacks it
 *
 * @retval true: notification got stacked
 * @retval false: notification did not get stacked
 */
static bool queues_stack_duplicate(struct notification *new)
{
        GQueue *allqueues[] = { displayed, waiting };
        for (int i = 0; i < sizeof(allqueues)/sizeof(GQueue*); i++) {
                for (GList *iter = g_queue_peek_head_link(allqueues[i]); iter;
                     iter = iter->next) {
                        struct notification *old = iter->data;
                        if (notification_is_duplicate(old, new)) {
                                /* If the progress differs, probably notify-send was used to update the notification
                                 * So only count it as a duplicate, if the progress was not the same.
                                 * */
                                if (old->progress == new->progress) {
                                        old->dup_count++;
                                } else {
                                        old->progress = new->progress;
                                }
                                iter->data = new;

                                new->dup_count = old->dup_count;
                                signal_notification_closed(old, 1);

                                if (allqueues[i] == displayed)
                                        new->start = time_monotonic_now();

                                notification_transfer_icon(old, new);

                                notification_unref(old);
                                return true;
                        }
                }
        }

        return false;
}

/**
 * Replaces the first notification of the same stack_tag
 *
 * @retval true: notification got stacked
 * @retval false: notification did not get stacked
 */
static bool queues_stack_by_tag(struct notification *new)
{
        GQueue *allqueues[] = { displayed, waiting };
        for (int i = 0; i < sizeof(allqueues)/sizeof(GQueue*); i++) {
                for (GList *iter = g_queue_peek_head_link(allqueues[i]); iter;
                            iter = iter->next) {
                        struct notification *old = iter->data;
                        if (STR_FULL(old->stack_tag) && STR_EQ(old->stack_tag, new->stack_tag)
                                        && STR_EQ(old->appname, new->appname)) {
                                iter->data = new;
                                new->dup_count = old->dup_count;

                                signal_notification_closed(old, 1);

                                if (allqueues[i] == displayed) {
                                        new->start = time_monotonic_now();
                                        notification_run_script(new);
                                }

                                notification_transfer_icon(old, new);

                                notification_unref(old);
                                return true;
                        }
                }
        }
        return false;
}

/* see queues.h */
bool queues_notification_replace_id(struct notification *new)
{
        GQueue *allqueues[] = { displayed, waiting };
        for (int i = 0; i < sizeof(allqueues)/sizeof(GQueue*); i++) {
                for (GList *iter = g_queue_peek_head_link(allqueues[i]);
                            iter;
                            iter = iter->next) {
                        struct notification *old = iter->data;
                        if (old->id == new->id) {
                                iter->data = new;
                                new->dup_count = old->dup_count;

                                if (allqueues[i] == displayed) {
                                        new->start = time_monotonic_now();
                                        notification_run_script(new);
                                }

                                notification_unref(old);
                                return true;
                        }
                }
        }
        return false;
}

/* see queues.h */
void queues_notification_close_id(int id, enum reason reason)
{
        struct notification *target = NULL;

        GQueue *allqueues[] = { displayed, waiting };
        for (int i = 0; i < sizeof(allqueues)/sizeof(GQueue*); i++) {
                for (GList *iter = g_queue_peek_head_link(allqueues[i]); iter;
                     iter = iter->next) {
                        struct notification *n = iter->data;
                        if (n->id == id) {
                                g_queue_remove(allqueues[i], n);
                                target = n;
                                break;
                        }
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
void queues_notification_close(struct notification *n, enum reason reason)
{
        assert(n != NULL);
        queues_notification_close_id(n->id, reason);
}

/* see queues.h */
void queues_history_pop(void)
{
        if (g_queue_is_empty(history))
                return;

        struct notification *n = g_queue_pop_tail(history);
        n->redisplayed = true;
        n->timeout = settings.sticky_history ? 0 : n->timeout;
        g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
}

/* see queues.h */
void queues_history_pop_by_id(unsigned int id)
{
        struct notification *n = NULL;

        if (g_queue_is_empty(history))
                return;

        // search through the history buffer 
        for (GList *iter = g_queue_peek_head_link(history); iter;
                iter = iter->next) {
                struct notification *cur = iter->data;
                if (cur->id == id) {
                        n = cur;
                        break;
                }
        }

        // must be a valid notification
        if (n == NULL)
                return;

        g_queue_remove(history, n);
        n->redisplayed = true;
        n->timeout = settings.sticky_history ? 0 : n->timeout;
        g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
}

/* see queues.h */
void queues_history_push(struct notification *n)
{
        if (!n->history_ignore) {
                if (settings.history_length > 0 && history->length >= settings.history_length) {
                        struct notification *to_free = g_queue_pop_head(history);
                        notification_unref(to_free);
                }

                g_queue_push_tail(history, n);
        } else {
                notification_unref(n);
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
void queues_update(struct dunst_status status, gint64 time)
{
        GList *iter, *nextiter;

        /* Move back all notifications, which aren't eligible to get shown anymore
         * Will move the notifications back to waiting, if dunst isn't running or fullscreen
         * and notifications is not eligible to get shown anymore */
        iter = g_queue_peek_head_link(displayed);
        while (iter) {
                struct notification *n = iter->data;
                nextiter = iter->next;

                if (notification_is_locked(n)) {
                        iter = nextiter;
                        continue;
                }

                if (n->marked_for_closure) {
                        queues_notification_close(n, n->marked_for_closure);
                        n->marked_for_closure = 0;
                        iter = nextiter;
                        continue;
                }


                if (queues_notification_is_finished(n, status, time)){
                        queues_notification_close(n, REASON_TIME);
                        iter = nextiter;
                        continue;
                }

                if (!queues_notification_is_ready(n, status, true)) {
                        g_queue_delete_link(displayed, iter);
                        g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
                        iter = nextiter;
                        continue;
                }

                iter = nextiter;
        }

        int cur_displayed_limit;
        if (settings.notification_limit == 0)
                cur_displayed_limit = INT_MAX;
        else if (   settings.indicate_hidden
                 && settings.notification_limit > 1
                 && displayed->length + waiting->length > settings.notification_limit)
                cur_displayed_limit = settings.notification_limit-1;
        else
                cur_displayed_limit = settings.notification_limit;

        /* move notifications from queue to displayed */
        iter = g_queue_peek_head_link(waiting);
        while (displayed->length < cur_displayed_limit && iter) {
                struct notification *n = iter->data;
                nextiter = iter->next;

                ASSERT_OR_RET(n,);

                if (!queues_notification_is_ready(n, status, false)) {
                        iter = nextiter;
                        continue;
                }

                n->start = time_monotonic_now();
                notification_run_script(n);

                if (n->skip_display && !n->redisplayed) {
                        queues_notification_close(n, REASON_USER);
                } else {
                        g_queue_delete_link(waiting, iter);
                        g_queue_insert_sorted(displayed, n, notification_cmp_data, NULL);
                }

                iter = nextiter;
        }

        /* if necessary, push the overhanging notifications from displayed to waiting again */
        while (displayed->length > cur_displayed_limit) {
                struct notification *n = g_queue_pop_tail(displayed);
                g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL); //TODO: actually it should be on the head if unsorted
        }

        /* If displayed is actually full, let the more important notifications
         * from waiting seep into displayed.
         */
        if (settings.sort && displayed->length == cur_displayed_limit) {
                GList *i_waiting, *i_displayed;

                while (   (i_waiting   = g_queue_peek_head_link(waiting))
                       && (i_displayed = g_queue_peek_tail_link(displayed))) {

                        while (i_waiting && ! queues_notification_is_ready(i_waiting->data, status, false)) {
                                i_waiting = i_waiting->prev;
                        }

                        if (i_waiting && notification_cmp(i_displayed->data, i_waiting->data) > 0) {
                                struct notification *todisp = i_waiting->data;

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
                struct notification *n = iter->data;
                gint64 ttl = n->timeout - (time - n->start);

                if (n->timeout > 0 && n->locked == 0) {
                        if (ttl > 0)
                                sleep = MIN(sleep, ttl);
                        else
                                // while we're processing, the notification already timed out
                                return 0;
                }

                if (settings.show_age_threshold >= 0) {
                        gint64 age = time - n->timestamp;

                        // sleep exactly until the next shift of the second happens
                        if (age > settings.show_age_threshold - S2US(1))
                                sleep = MIN(sleep, (S2US(1) - (age % S2US(1))));
                        else
                                sleep = MIN(sleep, settings.show_age_threshold - age);
                }
        }

        return sleep != G_MAXINT64 ? sleep : -1;
}




/* see queues.h */
struct notification* queues_get_by_id(int id)
{
        assert(id > 0);

        GQueue *recqueues[] = { displayed, waiting, history };
        for (int i = 0; i < sizeof(recqueues)/sizeof(GQueue*); i++) {
                for (GList *iter = g_queue_peek_head_link(recqueues[i]); iter;
                     iter = iter->next) {
                        struct notification *cur = iter->data;
                        if (cur->id == id)
                                return cur;
                }
        }

        return NULL;
}

/**
 * Helper function for queues_teardown() to free a single notification
 *
 * @param data The notification to free
 */
static void teardown_notification(gpointer data)
{
        struct notification *n = data;
        notification_unref(n);
}

/* see queues.h */
void queues_teardown(void)
{
        g_queue_free_full(history, teardown_notification);
        history = NULL;
        g_queue_free_full(displayed, teardown_notification);
        displayed = NULL;
        g_queue_free_full(waiting, teardown_notification);
        waiting = NULL;
}


/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

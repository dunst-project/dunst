/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "queues.h"

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "notification.h"
#include "settings.h"

/* notification lists */
static GQueue *waiting   = NULL; /* all new notifications get into here */
static GQueue *displayed = NULL; /* currently displayed notifications */
static GQueue *history   = NULL; /* history of displayed notifications */

unsigned int displayed_limit = 0;
int next_notification_id = 1;
bool pause_displayed = false;

static int queues_stack_duplicate(notification *n);

void queues_init(void)
{
        history   = g_queue_new();
        displayed = g_queue_new();
        waiting   = g_queue_new();
}

void queues_displayed_limit(unsigned int limit)
{
        displayed_limit = limit;
}

/* misc getter functions */
const GList *queues_get_displayed()
{
        return g_queue_peek_head_link(displayed);
}
unsigned int queues_length_waiting()
{
        return waiting->length;
}
unsigned int queues_length_displayed()
{
        return displayed->length;
}
unsigned int queues_length_history()
{
        return history->length;
}

int queues_notification_insert(notification *n, int replaces_id)
{
        /* do not display the message, if the message is empty */
        if (strlen(n->msg) == 0) {
                if (settings.always_run_script) {
                        notification_run_script(n);
                }
                printf("skipping notification: %s %s\n", n->body, n->summary);
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

        if (replaces_id == 0) {

                if (settings.stack_duplicates) {
                        int stacked = queues_stack_duplicate(n);
                        if (stacked > 0) {
                                // notification got stacked
                                return stacked;
                        }
                }

                n->id = ++next_notification_id;

                g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);

        } else {
                n->id = replaces_id;
                if (!queues_notification_replace_id(n))
                        g_queue_insert_sorted(waiting, n, notification_cmp_data, NULL);
        }

        if (settings.print_notifications)
                notification_print(n);

        return n->id;
}

/*
 * Replaces duplicate notification and stacks it
 *
 * Returns the notification id of the stacked notification
 * Returns -1 if not notification could be stacked
 */
static int queues_stack_duplicate(notification *n)
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
                        orig->start = g_get_monotonic_time();
                        g_free(orig->msg);
                        orig->msg = g_strdup(n->msg);
                        notification_free(n);
                        return orig->id;
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
                        g_free(orig->msg);
                        orig->msg = g_strdup(n->msg);
                        notification_free(n);
                        return orig->id;
                }
        }

        return -1;
}

bool queues_notification_replace_id(notification *new)
{

        for (GList *iter = g_queue_peek_head_link(displayed);
                    iter;
                    iter = iter->next) {
                notification *old = iter->data;
                if (old->id == new->id) {
                        iter->data = new;
                        new->start = g_get_monotonic_time();
                        new->dup_count = old->dup_count;
                        notification_run_script(new);
                        queues_history_push(old);
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
                        queues_history_push(old);
                        return true;
                }
        }
        return false;
}

int queues_notification_close_id(int id, enum reason reason)
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

                if (reason > 0 && reason < 4)
                        notification_closed(target, reason);

                queues_history_push(target);
        }

        return reason;
}

int queues_notification_close(notification *n, enum reason reason)
{
        assert(n != NULL);
        return queues_notification_close_id(n->id, reason);
}

void queues_history_pop(void)
{
        if (g_queue_is_empty(history))
                return;

        notification *n = g_queue_pop_tail(history);
        n->redisplayed = true;
        n->start = 0;
        n->timeout = settings.sticky_history ? 0 : n->timeout;
        g_queue_push_head(waiting, n);
}

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

void queues_history_push_all(void)
{
        while (displayed->length > 0) {
                queues_notification_close(g_queue_peek_head_link(displayed)->data, REASON_USER);
        }

        while (waiting->length > 0) {
                queues_notification_close(g_queue_peek_head_link(waiting)->data, REASON_USER);
        }
}

void queues_check_timeouts(bool idle)
{
        /* nothing to do */
        if (displayed->length == 0)
                return;

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
                if (idle && !n->transient) {
                        n->start = g_get_monotonic_time();
                        continue;
                }

                /* skip hidden and sticky messages */
                if (n->start == 0 || n->timeout == 0) {
                        continue;
                }

                /* remove old message */
                if (g_get_monotonic_time() - n->start > n->timeout) {
                        queues_notification_close(n, REASON_TIME);
                }
        }
}

void queues_update()
{
        if (pause_displayed) {
                while (displayed->length > 0) {
                        g_queue_insert_sorted(
                            waiting, g_queue_pop_head(displayed), notification_cmp_data, NULL);
                }
                return;
        }

        /* move notifications from queue to displayed */
        while (waiting->length > 0) {

                if (displayed_limit > 0 && displayed->length >= displayed_limit) {
                        /* the list is full */
                        break;
                }

                notification *n = g_queue_pop_head(waiting);

                if (!n)
                        return;

                n->start = g_get_monotonic_time();

                if (!n->redisplayed && n->script) {
                        notification_run_script(n);
                }

                g_queue_insert_sorted(displayed, n, notification_cmp_data, NULL);
        }
}

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
                        else if (ttl > settings.show_age_threshold)
                                sleep = MIN(sleep, settings.show_age_threshold);
                }
        }

        return sleep != G_MAXINT64 ? sleep : -1;
}

void queues_pause_on(void)
{
        pause_displayed = true;
}

void queues_pause_off(void)
{
        pause_displayed = false;
}

bool queues_pause_status(void)
{
        return pause_displayed;
}

static void teardown_notification(gpointer data)
{
        notification *n = data;
        notification_free(n);
}

void teardown_queues(void)
{
        g_queue_free_full(history, teardown_notification);
        g_queue_free_full(displayed, teardown_notification);
        g_queue_free_full(waiting, teardown_notification);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

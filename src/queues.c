/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "queues.h"

#include <assert.h>
#include <glib.h>

#include "dbus.h"
#include "notification.h"
#include "settings.h"

/* notification lists */
GQueue *queue     = NULL; /* all new notifications get into here */
GQueue *displayed = NULL; /* currently displayed notifications */
GQueue *history   = NULL; /* history of displayed notifications */

void queues_init(void)
{
        history   = g_queue_new();
        displayed = g_queue_new();
        queue     = g_queue_new();
}

bool notification_replace_by_id(notification *new)
{

        for (GList *iter = g_queue_peek_head_link(displayed);
                    iter;
                    iter = iter->next) {
                notification *old = iter->data;
                if (old->id == new->id) {
                        iter->data = new;
                        new->start = time(NULL);
                        new->dup_count = old->dup_count;
                        notification_run_script(new);
                        history_push(old);
                        return true;
                }
        }

        for (GList *iter = g_queue_peek_head_link(queue);
                    iter;
                    iter = iter->next) {
                notification *old = iter->data;
                if (old->id == new->id) {
                        iter->data = new;
                        new->dup_count = old->dup_count;
                        history_push(old);
                        return true;
                }
        }
        return false;
}

int notification_close_by_id(int id, int reason)
{
        notification *target = NULL;

        for (GList *iter = g_queue_peek_head_link(displayed); iter;
             iter = iter->next) {
                notification *n = iter->data;
                if (n->id == id) {
                        g_queue_remove(displayed, n);
                        history_push(n);
                        target = n;
                        break;
                }
        }

        for (GList *iter = g_queue_peek_head_link(queue); iter;
             iter = iter->next) {
                notification *n = iter->data;
                if (n->id == id) {
                        g_queue_remove(queue, n);
                        history_push(n);
                        target = n;
                        break;
                }
        }

        if (reason > 0 && reason < 4 && target != NULL) {
                notification_closed(target, reason);
        }

        return reason;
}

int notification_close(notification *n, int reason)
{
        assert(n != NULL);
        return notification_close_by_id(n->id, reason);
}

void move_all_to_history()
{
        while (displayed->length > 0) {
                notification_close(g_queue_peek_head_link(displayed)->data, 2);
        }

        while (queue->length > 0) {
                notification_close(g_queue_peek_head_link(queue)->data, 2);
        }
}

void history_pop(void)
{
        if (g_queue_is_empty(history))
                return;

        notification *n = g_queue_pop_tail(history);
        n->redisplayed = true;
        n->start = 0;
        n->timeout = settings.sticky_history ? 0 : n->timeout;
        g_queue_push_head(queue, n);
}

void history_push(notification *n)
{
        if (settings.history_length > 0 && history->length >= settings.history_length) {
                notification *to_free = g_queue_pop_head(history);
                notification_free(to_free);
        }

        if (!n->history_ignore)
                g_queue_push_tail(history, n);
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
        g_queue_free_full(queue, teardown_notification);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

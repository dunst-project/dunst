/* copyright 2020 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "clipboard.h"

#include <errno.h>
#include <glib.h>

#include "log.h"
#include "notification.h"
#include "queues.h"
#include "settings.h"
#include "utils.h"

struct notification_lock {
        struct notification *n;
        gint64 timeout;
};
static gpointer copy_alert_thread(gpointer data);

/** Call xclip with the specified input. Blocks until xclip is finished.
 *
 * @param xclip_input The data to be copied by xclip
 */
void invoke_xclip(const char *xclip_input)
{
        if (!settings.xclip_cmd) {
                LOG_C("Unable to open xclip: No xclip command set.");
                return;
        }

        ASSERT_OR_RET(STR_FULL(xclip_input),);

        gint dunst_to_xclip;
        GError *err = NULL;

        g_spawn_async_with_pipes(NULL,
                                 settings.xclip_cmd,
                                 NULL,
                                 G_SPAWN_DEFAULT
                                   | G_SPAWN_SEARCH_PATH,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &dunst_to_xclip,
                                 NULL,
                                 NULL,
                                 &err);

        if (err) {
                LOG_C("Cannot spawn xclip: %s", err->message);
                g_error_free(err);
        } else {
                size_t wlen = strlen(xclip_input);
                if (write(dunst_to_xclip, xclip_input, wlen) != wlen) {
                        LOG_W("Cannot feed xclip with input: %s", strerror(errno));
                }
                close(dunst_to_xclip);
        }
}

void copy_alert_contents()
{
        GError *err = NULL;
        g_thread_unref(g_thread_try_new("xclip",
                                        copy_alert_thread,
                                        NULL,
                                        &err));

        if (err) {
                LOG_C("Cannot start thread to call xclip: %s", err->message);
                g_error_free(err);
        }
}

static gpointer copy_alert_thread(gpointer data)
{
        char *xclip_input = NULL;
        GList *locked_notifications = NULL;

        for (const GList *iter = queues_get_displayed(); iter;
             iter = iter->next) {
                struct notification *n = iter->data;


                // Reference and lock the notification if we need it
		notification_ref(n);

		struct notification_lock *nl =
			g_malloc(sizeof(struct notification_lock));

		nl->n = n;
		nl->timeout = n->timeout;
		n->timeout = 0;

		locked_notifications = g_list_prepend(locked_notifications, nl);

                xclip_input = string_append(xclip_input, n->clipboard_msg, "\n");
        }

        invoke_xclip(xclip_input);
        g_free(xclip_input);

        // unref all notifications
        for (GList *iter = locked_notifications;
                    iter;
                    iter = iter->next) {

                struct notification_lock *nl = iter->data;
                struct notification *n = nl->n;

                n->timeout = nl->timeout;

                g_free(nl);
                notification_unref(n);
        }
        g_list_free(locked_notifications);

        return NULL;
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

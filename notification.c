/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE

#include <time.h>
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

#include "dbus.h"
#include "x.h"
#include "notification.h"
#include "dunst.h"
#include "utils.h"
#include "settings.h"
#include "rules.h"
#include "menu.h"

int next_notification_id = 1;

        /*
         * print a human readable representation
         * of the given notification to stdout.
         */
void notification_print(notification * n)
{
        printf("{\n");
        printf("\tappname: '%s'\n", n->appname);
        printf("\tsummary: '%s'\n", n->summary);
        printf("\tbody: '%s'\n", n->body);
        printf("\ticon: '%s'\n", n->icon);
        printf("\turgency: %d\n", n->urgency);
        printf("\tformatted: '%s'\n", n->msg);
        printf("\tfg: %s\n", n->color_strings[ColFG]);
        printf("\tbg: %s\n", n->color_strings[ColBG]);
        printf("\tid: %d\n", n->id);
        if (n->urls) {
                printf("\turls\n");
                printf("\t{\n");
                printf("%s\n", n->urls);
                printf("\t}\n");
        }

        if (n->actions) {
                printf("\tactions:\n");
                printf("\t{\n");
                for (int i = 0; i < n->actions->count; i += 2) {
                        printf("\t\t [%s,%s]\n", n->actions->actions[i],
                               n->actions->actions[i + 1]);
                }
                printf("actions_dmenu: %s\n", n->actions->dmenu_str);
                printf("\t]\n");
        }
        printf("\tscript: %s\n", n->script);
        printf("}\n");
}

        /*
         * Run the script associated with the
         * given notification.
         */
void notification_run_script(notification * n)
{
        if (!n->script || strlen(n->script) < 1)
                return;

        char *appname = n->appname ? n->appname : "";
        char *summary = n->summary ? n->summary : "";
        char *body = n->body ? n->body : "";
        char *icon = n->icon ? n->icon : "";

        char *urgency;
        switch (n->urgency) {
        case LOW:
                urgency = "LOW";
                break;
        case NORM:
                urgency = "NORMAL";
                break;
        case CRIT:
                urgency = "CRITICAL";
                break;
        default:
                urgency = "NORMAL";
                break;
        }

        int pid1 = fork();

        if (pid1) {
                int status;
                waitpid(pid1, &status, 0);
        } else {
                int pid2 = fork();
                if (pid2) {
                        exit(0);
                } else {
                        int ret = execlp(n->script, n->script,
                                         appname,
                                         summary,
                                         body,
                                         icon,
                                         urgency,
                                         (char *)NULL);
                        if (ret != 0) {
                                PERR("Unable to run script", errno);
                                exit(EXIT_FAILURE);
                        }
                }
        }
}

        /*
         * Helper function to compare to given
         * notifications.
         */
int notification_cmp(const void *va, const void *vb)
{
        notification *a = (notification *) va;
        notification *b = (notification *) vb;

        if (!settings.sort)
                return 1;

        if (a->urgency != b->urgency) {
                return b->urgency - a->urgency;
        } else {
                return a->id - b->id;
        }
}

        /*
         * Wrapper for notification_cmp to match glib's
         * compare functions signature.
         */
int notification_cmp_data(const void *va, const void *vb, void *data)
{
        return notification_cmp(va, vb);
}

        /*
         * Free the memory used by the given notification.
         */
void notification_free(notification * n)
{
        if (n == NULL)
                return;
        free(n->appname);
        free(n->summary);
        free(n->body);
        free(n->icon);
        free(n->msg);
        free(n->dbus_client);
        free(n);
}

        /*
         * Strip any markup from text
         */

char *notification_fix_markup(char *str)
{
        char *replace_buf, *start, *end;

        if (str == NULL) {
                return NULL;
        }

        str = string_replace_all("&quot;", "\"", str);
        str = string_replace_all("&apos;", "'", str);
        str = string_replace_all("&amp;", "&", str);
        str = string_replace_all("&lt;", "<", str);
        str = string_replace_all("&gt;", ">", str);

        /* remove tags */
        str = string_replace_all("<b>", "", str);
        str = string_replace_all("</b>", "", str);
        str = string_replace_all("<br>", " ", str);
        str = string_replace_all("<br/>", " ", str);
        str = string_replace_all("<br />", " ", str);
        str = string_replace_all("<i>", "", str);
        str = string_replace_all("</i>", "", str);
        str = string_replace_all("<u>", "", str);
        str = string_replace_all("</u>", "", str);
        str = string_replace_all("</a>", "", str);

        start = strstr(str, "<a href");
        if (start != NULL) {
                end = strstr(str, ">");
                if (end != NULL) {
                        replace_buf = strndup(start, end - start + 1);
                        str = string_replace(replace_buf, "", str);
                        free(replace_buf);
                }
        }
        start = strstr(str, "<img src");
        if (start != NULL) {
                end = strstr(str, "/>");
                if (end != NULL) {
                        replace_buf = strndup(start, end - start + 2);
                        str = string_replace(replace_buf, "", str);
                        free(replace_buf);
                }
        }
        return str;

}

        /*
         * Initialize the given notification and add it to
         * the queue. Replace notification with id if id > 0.
         */
int notification_init(notification * n, int id)
{

        if (n == NULL)
                return -1;

        if (strcmp("DUNST_COMMAND_PAUSE", n->summary) == 0) {
                pause_display = true;
                return 0;
        }

        if (strcmp("DUNST_COMMAND_RESUME", n->summary) == 0) {
                pause_display = false;
                return 0;
        }

        n->script = NULL;
        n->text_to_render = NULL;

        n->format = settings.format;

        rule_apply_all(n);

        n->msg = string_replace("%a", n->appname, g_strdup(n->format));
        n->msg = string_replace("%s", n->summary, n->msg);
        if (n->icon) {
                n->msg = string_replace("%I", basename(n->icon), n->msg);
                n->msg = string_replace("%i", n->icon, n->msg);
        }
        n->msg = string_replace("%b", n->body, n->msg);
        if (n->progress) {
                char pg[10];
                sprintf(pg, "[%3d%%]", n->progress - 1);
                n->msg = string_replace("%p", pg, n->msg);
        } else {
                n->msg = string_replace("%p", "", n->msg);
        }

        if (!settings.allow_markup)
                n->msg = notification_fix_markup(n->msg);

        while (strstr(n->msg, "\\n") != NULL)
                n->msg = string_replace("\\n", "\n", n->msg);

        if (settings.ignore_newline)
                while (strstr(n->msg, "\n") != NULL)
                        n->msg = string_replace("\n", " ", n->msg);

        n->msg = g_strstrip(n->msg);

        n->dup_count = 0;

        /* check if n is a duplicate */
        for (GList * iter = g_queue_peek_head_link(queue); iter;
             iter = iter->next) {
                notification *orig = iter->data;
                if (strcmp(orig->appname, n->appname) == 0
                    && strcmp(orig->msg, n->msg) == 0) {
                        orig->dup_count++;
                        notification_free(n);
                        wake_up();
                        return orig->id;
                }
        }

        for (GList * iter = g_queue_peek_head_link(displayed); iter;
             iter = iter->next) {
                notification *orig = iter->data;
                if (strcmp(orig->appname, n->appname) == 0
                    && strcmp(orig->msg, n->msg) == 0) {
                        orig->dup_count++;
                        orig->start = time(NULL);
                        notification_free(n);
                        wake_up();
                        return orig->id;
                }
        }

        /* urgency > CRIT -> array out of range */
        n->urgency = n->urgency > CRIT ? CRIT : n->urgency;

        if (!n->color_strings[ColFG]) {
                n->color_strings[ColFG] = xctx.color_strings[ColFG][n->urgency];
        }

        if (!n->color_strings[ColBG]) {
                n->color_strings[ColBG] = xctx.color_strings[ColBG][n->urgency];
        }

        n->timeout =
            n->timeout == -1 ? settings.timeouts[n->urgency] : n->timeout;
        n->start = 0;

        n->timestamp = time(NULL);

        n->redisplayed = false;

        if (id == 0) {
                n->id = ++next_notification_id;
        } else {
                notification_close_by_id(id, -1);
                n->id = id;
        }

        if (strlen(n->msg) == 0) {
                notification_close(n, 2);
                printf("skipping notification: %s %s\n", n->body, n->summary);
        } else {
                g_queue_insert_sorted(queue, n, notification_cmp_data, NULL);
        }

        char *tmp = g_strconcat(n->summary, " ", n->body, NULL);

        n->urls = extract_urls(tmp);

        if (n->actions) {
                n->actions->dmenu_str = NULL;
                for (int i = 0; i < n->actions->count; i += 2) {
                        char *human_readable = n->actions->actions[i + 1];
                        string_replace_char('[', '(', human_readable); // kill square brackets
                        string_replace_char(']', ')', human_readable);
                        char *tmp =
                            g_strdup_printf("%s %s", n->appname,
                                            human_readable);

                        n->actions->dmenu_str =
                            string_append(n->actions->dmenu_str,
                                          g_strdup_printf("#%s [%s]", human_readable,
                                                                      n->appname),
                                          "\n");
                }
        }

        free(tmp);

        if (settings.print_notifications)
                notification_print(n);

        return n->id;
}

        /*
         * Close the notification that has id.
         *
         * reasons:
         * -1 -> notification is a replacement, no NotificationClosed signal emitted
         *  1 -> the notification expired
         *  2 -> the notification was dismissed by the user_data
         *  3 -> The notification was closed by a call to CloseNotification
         */
int notification_close_by_id(int id, int reason)
{
        notification *target = NULL;

        for (GList * iter = g_queue_peek_head_link(displayed); iter;
             iter = iter->next) {
                notification *n = iter->data;
                if (n->id == id) {
                        g_queue_remove(displayed, n);
                        g_queue_push_tail(history, n);
                        target = n;
                        break;
                }
        }

        for (GList * iter = g_queue_peek_head_link(queue); iter;
             iter = iter->next) {
                notification *n = iter->data;
                if (n->id == id) {
                        g_queue_remove(queue, n);
                        g_queue_push_tail(history, n);
                        target = n;
                        break;
                }
        }

        if (reason > 0 && reason < 4 && target != NULL) {
                notificationClosed(target, reason);
        }

        wake_up();
        return reason;
}

        /*
         * Close the given notification. SEE notification_close_by_id.
         */
int notification_close(notification * n, int reason)
{
        if (n == NULL)
                return -1;
        return notification_close_by_id(n->id, reason);
}

void notification_update_text_to_render(notification *n)
{
        if (n->text_to_render) {
                free(n->text_to_render);
                n->text_to_render = NULL;
        }

        char *buf = NULL;

        char *msg = g_strstrip(n->msg);

        /* print dup_count and msg */
        if (n->dup_count > 0 && (n->actions || n->urls)) {
                buf = g_strdup_printf("(%d%s%s) %s",
                                      n->dup_count,
                                      n->actions ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if (n->actions || n->urls) {
                buf = g_strdup_printf("(%s%s) %s",
                                      n->actions ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if (n->dup_count > 0) {
                buf = g_strdup_printf("(%d) %s", n->dup_count, msg);
        } else {
                buf = g_strdup(msg);
        }

        /* print age */
        int hours, minutes, seconds;
        time_t t_delta = time(NULL) - n->timestamp;

        if (settings.show_age_threshold >= 0
            && t_delta >= settings.show_age_threshold) {
                hours = t_delta / 3600;
                minutes = t_delta / 60 % 60;
                seconds = t_delta % 60;

                char *new_buf;
                if (hours > 0) {
                        new_buf =
                            g_strdup_printf("%s (%dh %dm %ds old)", buf, hours,
                                            minutes, seconds);
                } else if (minutes > 0) {
                        new_buf =
                            g_strdup_printf("%s (%dm %ds old)", buf, minutes,
                                            seconds);
                } else {
                        new_buf = g_strdup_printf("%s (%ds old)", buf, seconds);
                }

                free(buf);
                buf = new_buf;
        }

        n->text_to_render = buf;
}

int notification_get_ttl(notification *n) {
        if (n->timeout == 0) {
                return 0;
        } else {
                return n->timeout - (time(NULL) - n->start);
        }
}

int notification_get_age(notification *n) {
        return time(NULL) - n->timestamp;
}
/* vim: set ts=8 sw=8 tw=0: */

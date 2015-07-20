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
        printf("\tcategory: %s\n", n->category);
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

        if (n->category && *n->category != '\0')
                g_free(n->category);

        if (n->text_to_render)
                g_free(n->text_to_render);

        if (n->urls)
                g_free(n->urls);

        if (n->actions) {
                g_strfreev(n->actions->actions);
                free(n->actions->dmenu_str);
        }

        free(n);
}

        /*
         * Strip any markup from text
         */
char *notification_strip_markup(char *str)
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

        while ((start = strstr(str, "<a href")) != NULL) {
                end = strstr(start, ">");
                if (end != NULL) {
                        replace_buf = strndup(start, end - start + 1);
                        str = string_replace(replace_buf, "", str);
                        free(replace_buf);
                } else {
                    break;
                }
        }

        while ((start = strstr(str, "<img src")) != NULL) {
                end = strstr(start, "/>");
                if (end != NULL) {
                        replace_buf = strndup(start, end - start + 2);
                        str = string_replace(replace_buf, "", str);
                        free(replace_buf);
                } else {
                    break;
                }
        }
        return str;
}

        /*
         * Quote a text string for rendering with pango
         */
char *notification_quote_markup(char *str)
{
        if (str == NULL) {
                return NULL;
        }

        str = string_replace_all("&", "&amp;", str);
        str = string_replace_all("\"", "&quot;", str);
        str = string_replace_all("'", "&apos;", str);
        str = string_replace_all("<", "&lt;", str);
        str = string_replace_all(">", "&gt;", str);

        return str;
}

        /*
         * Replace all occurrences of "needle" with a quoted "replacement",
         * according to the allow_markup/plain_text settings.
         */
char *notification_replace_format(const char *needle, const char *replacement,
                                  char *haystack, bool allow_markup,
                                  bool plain_text) {
        char* tmp;
        char* ret;

        if (plain_text) {
                tmp = strdup(replacement);
                tmp = string_replace_all("\\n", "\n", tmp);
                if (settings.ignore_newline) {
                        tmp = string_replace_all("\n", " ", tmp);
                }
                tmp = notification_quote_markup(tmp);
                ret = string_replace_all(needle, tmp, haystack);
                free(tmp);
        } else if (!allow_markup) {
                tmp = strdup(replacement);
                if (!settings.ignore_newline) {
                        tmp = string_replace_all("<br>", "\n", tmp);
                        tmp = string_replace_all("<br/>", "\n", tmp);
                        tmp = string_replace_all("<br />", "\n", tmp);
                }
                tmp = notification_strip_markup(tmp);
                tmp = notification_quote_markup(tmp);
                ret = string_replace_all(needle, tmp, haystack);
                free(tmp);
        } else {
                ret = string_replace_all(needle, replacement, haystack);
        }

        return ret;
}

char *notification_extract_markup_urls(char **str_ptr) {
    char *start, *end, *replace_buf, *str, *urls = NULL, *url, *index_buf;
    int linkno = 1;

    str = *str_ptr;
    while ((start = strstr(str, "<a href")) != NULL) {
        end = strstr(start, ">");
        if (end != NULL) {
                replace_buf = strndup(start, end - start + 1);
                url = extract_urls(replace_buf);
                if (url != NULL) {
                    str = string_replace(replace_buf, "[", str);

                    index_buf = g_strdup_printf("[#%d]", linkno++);
                    if (urls == NULL) {
                        urls = g_strconcat(index_buf, " ", url, NULL);
                    } else {
                        char *tmp = urls;
                        urls = g_strconcat(tmp, "\n", index_buf, " ", url, NULL);
                        free(tmp);
                    }

                    index_buf[0] = ' ';
                    str = string_replace("</a>", index_buf, str);
                    free(index_buf);
                    free(url);
                } else {
                    str = string_replace(replace_buf, "", str);
                    str = string_replace("</a>", "", str);
                }
                free(replace_buf);
        } else {
            break;
        }
    }
    *str_ptr = str;
    return urls;
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

        n->urls = notification_extract_markup_urls(&(n->body));

        n->msg = string_replace_all("\\n", "\n", g_strdup(n->format));
        n->msg = notification_replace_format("%a", n->appname, n->msg,
                false, true);
        n->msg = notification_replace_format("%s", n->summary, n->msg,
                n->allow_markup, n->plain_text);
        n->msg = notification_replace_format("%b", n->body, n->msg,
                n->allow_markup, n->plain_text);

        if (n->icon) {
                n->msg = notification_replace_format("%I", basename(n->icon),
                        n->msg, false, true);
                n->msg = notification_replace_format("%i", n->icon,
                        n->msg, false, true);
        }

        if (n->progress) {
                char pg[10];
                sprintf(pg, "[%3d%%]", n->progress - 1);
                n->msg = string_replace_all("%p", pg, n->msg);
        } else {
                n->msg = string_replace_all("%p", "", n->msg);
        }

        n->msg = g_strstrip(n->msg);

        if (id == 0) {
                n->id = ++next_notification_id;
        } else {
                notification_close_by_id(id, -1);
                n->id = id;
        }

        n->dup_count = 0;

        /* check if n is a duplicate */
        if (settings.stack_duplicates) {
                for (GList * iter = g_queue_peek_head_link(queue); iter;
                     iter = iter->next) {
                        notification *orig = iter->data;
                        if (strcmp(orig->appname, n->appname) == 0
                            && strcmp(orig->summary, n->summary) == 0
                            && strcmp(orig->body, n->body) == 0) {
                                /* If the progress differs this was probably intended to replace the notification
                                 * but notify-send was used. So don't increment dup_count in this case
                                 */
                                if (orig->progress == n->progress) {
                                        orig->dup_count++;
                                } else {
                                        orig->progress = n->progress;
                                }
                                /* notifications that differ only in progress hints should be expected equal,
                                 * but we want the latest message, with the latest hint value
                                 */
                                free(orig->msg);
                                orig->msg = strdup(n->msg);
                                notification_free(n);
                                wake_up();
                                return orig->id;
                        }
                }

                for (GList * iter = g_queue_peek_head_link(displayed); iter;
                     iter = iter->next) {
                        notification *orig = iter->data;
                        if (strcmp(orig->appname, n->appname) == 0
                            && strcmp(orig->summary, n->summary) == 0
                            && strcmp(orig->body, n->body) == 0) {
                                /* notifications that differ only in progress hints should be expected equal,
                                 * but we want the latest message, with the latest hint value
                                 */
                                free(orig->msg);
                                orig->msg = strdup(n->msg);
                                /* If the progress differs this was probably intended to replace the notification
                                 * but notify-send was used. So don't increment dup_count in this case
                                 */
                                if (orig->progress == n->progress) {
                                        orig->dup_count++;
                                } else {
                                        orig->progress = n->progress;
                                }
                                orig->start = time(NULL);
                                notification_free(n);
                                wake_up();
                                return orig->id;
                        }
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

        if (n->icon == NULL) {
                n->icon = strdup(settings.icons[n->urgency]);
        }
        else if (strlen(n->icon) <= 0) {
                free(n->icon);
                n->icon = strdup(settings.icons[n->urgency]);
        }

        if (n->category == NULL) {
                n->category = "";
        }

        n->timestamp = time(NULL);

        n->redisplayed = false;

        n->first_render = true;

        if (strlen(n->msg) == 0) {
                notification_close(n, 2);
                if (settings.always_run_script) {
                        notification_run_script(n);
                }
                printf("skipping notification: %s %s\n", n->body, n->summary);
        } else {
                g_queue_insert_sorted(queue, n, notification_cmp_data, NULL);
        }

        char *tmp = g_strconcat(n->summary, " ", n->body, NULL);

        char *tmp_urls = extract_urls(tmp);
        if (tmp_urls != NULL) {
            if (n->urls != NULL) {
                n->urls = string_append(n->urls, tmp_urls, "\n");
                free(tmp_urls);
            } else {
                n->urls = tmp_urls;
            }
        }

        if (n->actions) {
                n->actions->dmenu_str = NULL;
                for (int i = 0; i < n->actions->count; i += 2) {
                        char *human_readable = n->actions->actions[i + 1];
                        string_replace_char('[', '(', human_readable); // kill square brackets
                        string_replace_char(']', ')', human_readable);

                        char *act_str = g_strdup_printf("#%s [%s]", human_readable, n->appname);
                        if (act_str) {
                                n->actions->dmenu_str = string_append(n->actions->dmenu_str, act_str, "\n");
                                free(act_str);
                        }
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
                        history_push(n);
                        target = n;
                        break;
                }
        }

        for (GList * iter = g_queue_peek_head_link(queue); iter;
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
        if (n->dup_count > 0 && (n->actions || n->urls)
            && settings.show_indicators) {
                buf = g_strdup_printf("(%d%s%s) %s",
                                      n->dup_count,
                                      n->actions ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if ((n->actions || n->urls) && settings.show_indicators) {
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
                return -1;
        } else {
                return n->timeout - (time(NULL) - n->start);
        }
}

int notification_get_age(notification *n) {
        return time(NULL) - n->timestamp;
}
/* vim: set ts=8 sw=8 tw=0: */

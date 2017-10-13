/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "notification.h"

#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>

#include "dbus.h"
#include "dunst.h"
#include "markup.h"
#include "menu.h"
#include "rules.h"
#include "settings.h"
#include "utils.h"
#include "x11/x.h"

int next_notification_id = 1;

/*
 * print a human readable representation
 * of the given notification to stdout.
 */
void notification_print(notification *n)
{
        printf("{\n");
        printf("\tappname: '%s'\n", n->appname);
        printf("\tsummary: '%s'\n", n->summary);
        printf("\tbody: '%s'\n", n->body);
        printf("\ticon: '%s'\n", n->icon);
        printf("\traw_icon set: %s\n", (n->raw_icon ? "true" : "false"));
        printf("\tcategory: %s\n", n->category);
        printf("\ttimeout: %ld\n", n->timeout/1000);
        printf("\turgency: %d\n", n->urgency);
        printf("\ttransient: %d\n", n->transient);
        printf("\tformatted: '%s'\n", n->msg);
        printf("\tfg: %s\n", n->color_strings[ColFG]);
        printf("\tbg: %s\n", n->color_strings[ColBG]);
        printf("\tframe: %s\n", n->color_strings[ColFrame]);
        printf("\tid: %d\n", n->id);
        if (n->urls) {
                printf("\turls:\n");
                printf("\t{\n");
                printf("\t\t%s\n", n->urls);
                printf("\t}\n");
        }

        if (n->actions) {
                printf("\tactions:\n");
                printf("\t{\n");
                for (int i = 0; i < n->actions->count; i += 2) {
                        printf("\t\t[%s,%s]\n", n->actions->actions[i],
                               n->actions->actions[i + 1]);
                }
                printf("\t}\n");
                printf("\tactions_dmenu: %s\n", n->actions->dmenu_str);
        }
        printf("\tscript: %s\n", n->script);
        printf("}\n");
}

/*
 * Run the script associated with the
 * given notification.
 */
void notification_run_script(notification *n)
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

int notification_is_duplicate(const notification *a, const notification *b)
{
        //Comparing raw icons is not supported, assume they are not identical
        if (settings.icon_position != icons_off
                && (a->raw_icon != NULL || b->raw_icon != NULL))
                return false;

        return strcmp(a->appname, b->appname) == 0
            && strcmp(a->summary, b->summary) == 0
            && strcmp(a->body,    b->body) == 0
            && (settings.icon_position != icons_off ? strcmp(a->icon, b->icon) == 0 : 1)
            && a->urgency == b->urgency;
}

/*
 * Free the memory used by the given notification.
 */
void notification_free(notification *n)
{
        assert(n != NULL);
        g_free(n->appname);
        g_free(n->summary);
        g_free(n->body);
        g_free(n->icon);
        g_free(n->msg);
        g_free(n->dbus_client);
        g_free(n->category);
        g_free(n->text_to_render);
        g_free(n->urls);

        if (n->actions) {
                g_strfreev(n->actions->actions);
                g_free(n->actions->dmenu_str);
        }

        if (n->raw_icon) {
                if (n->raw_icon->data)
                        g_free(n->raw_icon->data);
                g_free(n->raw_icon);
        }

        g_free(n);
}

/*
 * Replace the two chars where **needle points
 * with a quoted "replacement", according to the markup settings.
 *
 * The needle is a double pointer and gets updated upon return
 * to point to the first char, which occurs after replacement.
 *
 */
void notification_replace_single_field(char **haystack, char **needle,
                const char *replacement, enum markup_mode markup_mode) {

        assert(*needle[0] == '%');
        // needle has to point into haystack (but not on the last char)
        assert(*needle >= *haystack);
        assert(*needle - *haystack < strlen(*haystack) - 1);

        int pos = *needle - *haystack;

        char *input = markup_transform(g_strdup(replacement), markup_mode);
        *haystack = string_replace_at(*haystack, pos, 2, input);

        // point the needle to the next char
        // which was originally in haystack
        *needle = *haystack + pos + strlen(input);

        g_free(input);
}

char *notification_extract_markup_urls(char **str_ptr) {
        char *start, *end, *replace_buf, *str, *urls = NULL, *url, *index_buf;
        int linkno = 1;

        str = *str_ptr;
        while ((start = strstr(str, "<a href")) != NULL) {
                end = strstr(start, ">");
                if (end != NULL) {
                        replace_buf = g_strndup(start, end - start + 1);
                        url = extract_urls(replace_buf);
                        if (url != NULL) {
                                str = string_replace(replace_buf, "[", str);

                                index_buf = g_strdup_printf("[#%d]", linkno++);
                                if (urls == NULL) {
                                        urls = g_strconcat(index_buf, " ", url, NULL);
                                } else {
                                        char *tmp = urls;
                                        urls = g_strconcat(tmp, "\n", index_buf, " ", url, NULL);
                                        g_free(tmp);
                                }

                                index_buf[0] = ' ';
                                str = string_replace("</a>", index_buf, str);
                                g_free(index_buf);
                                g_free(url);
                        } else {
                                str = string_replace(replace_buf, "", str);
                                str = string_replace("</a>", "", str);
                        }
                        g_free(replace_buf);
                } else {
                        break;
                }
        }
        *str_ptr = str;
        return urls;
}

/*
 * Create notification struct and initialise everything to NULL,
 * this function is guaranteed to return a valid pointer.
 */
notification *notification_create(void)
{
        return g_malloc0(sizeof(notification));
}

void notification_init_defaults(notification *n)
{
        assert(n != NULL);
        if(n->appname == NULL) n->appname = g_strdup("unknown");
        if(n->summary == NULL) n->summary = g_strdup("");
        if(n->body == NULL) n->body = g_strdup("");
        if(n->category == NULL) n->category = g_strdup("");
}

/*
 * Initialize the given notification and add it to
 * the queue. Replace notification with id if id > 0.
 *
 * n should be a pointer to a notification allocated with
 * notification_create, it is undefined behaviour to pass a notification
 * allocated some other way.
 */
int notification_init(notification *n, int id)
{
        assert(n != NULL);

        //Prevent undefined behaviour by initialising required fields
        notification_init_defaults(n);

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

        if (n->icon != NULL && strlen(n->icon) <= 0) {
                g_free(n->icon);
                n->icon = NULL;
        }

        if (n->raw_icon == NULL && n->icon == NULL) {
                n->icon = g_strdup(settings.icons[n->urgency]);
        }

        n->urls = notification_extract_markup_urls(&(n->body));

        n->msg = string_replace_all("\\n", "\n", g_strdup(n->format));

        /* replace all formatter */
        for(char *substr = strchr(n->msg, '%');
                  substr;
                  substr = strchr(substr, '%')){

                char pg[16];
                char *icon_tmp;

                switch(substr[1]){
                        case 'a':
                                notification_replace_single_field(
                                        &n->msg,
                                        &substr,
                                        n->appname,
                                        MARKUP_NO);
                                break;
                        case 's':
                                notification_replace_single_field(
                                        &n->msg,
                                        &substr,
                                        n->summary,
                                        n->markup);
                                break;
                        case 'b':
                                notification_replace_single_field(
                                        &n->msg,
                                        &substr,
                                        n->body,
                                        n->markup);
                                break;
                        case 'I':
                                icon_tmp = g_strdup(n->icon);
                                notification_replace_single_field(
                                        &n->msg,
                                        &substr,
                                        icon_tmp ? basename(icon_tmp) : "",
                                        MARKUP_NO);
                                g_free(icon_tmp);
                                break;
                        case 'i':
                                notification_replace_single_field(
                                        &n->msg,
                                        &substr,
                                        n->icon ? n->icon : "",
                                        MARKUP_NO);
                                break;
                        case 'p':
                                if (n->progress)
                                        sprintf(pg, "[%3d%%]", n->progress - 1);

                                notification_replace_single_field(
                                        &n->msg,
                                        &substr,
                                        n->progress ? pg : "",
                                        MARKUP_NO);
                                break;
                        case 'n':
                                if (n->progress)
                                        sprintf(pg, "%d", n->progress - 1);

                                notification_replace_single_field(
                                        &n->msg,
                                        &substr,
                                        n->progress ? pg : "",
                                        MARKUP_NO);
                                break;
                        case '%':
                                notification_replace_single_field(
                                        &n->msg,
                                        &substr,
                                        "%",
                                        MARKUP_NO);
                                break;
                        case '\0':
                                fprintf(stderr, "WARNING: format_string has trailing %% character."
                                                "To escape it use %%%%.");
                                break;
                        default:
                                fprintf(stderr, "WARNING: format_string %%%c"
                                                " is unknown\n", substr[1]);
                                // shift substr pointer forward,
                                // as we can't interpret the format string
                                substr++;
                                break;
                }
        }

        n->msg = g_strchomp(n->msg);

        /* truncate overlong messages */
        if (strlen(n->msg) > DUNST_NOTIF_MAX_CHARS) {
                char *buffer = g_malloc(DUNST_NOTIF_MAX_CHARS);
                strncpy(buffer, n->msg, DUNST_NOTIF_MAX_CHARS);
                buffer[DUNST_NOTIF_MAX_CHARS-1] = '\0';

                g_free(n->msg);
                n->msg = buffer;
        }

        if (id == 0) {
                n->id = ++next_notification_id;
        } else {
                n->id = id;
        }

        n->dup_count = 0;

        /* check if n is a duplicate */
        if (settings.stack_duplicates) {
                for (GList *iter = g_queue_peek_head_link(queue); iter;
                     iter = iter->next) {
                        notification *orig = iter->data;
                        if (notification_is_duplicate(orig, n)) {
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
                                g_free(orig->msg);
                                orig->msg = g_strdup(n->msg);
                                notification_free(n);
                                wake_up();
                                return orig->id;
                        }
                }

                for (GList *iter = g_queue_peek_head_link(displayed); iter;
                     iter = iter->next) {
                        notification *orig = iter->data;
                        if (notification_is_duplicate(orig, n)) {
                                /* notifications that differ only in progress hints should be expected equal,
                                 * but we want the latest message, with the latest hint value
                                 */
                                g_free(orig->msg);
                                orig->msg = g_strdup(n->msg);
                                /* If the progress differs this was probably intended to replace the notification
                                 * but notify-send was used. So don't increment dup_count in this case
                                 */
                                if (orig->progress == n->progress) {
                                        orig->dup_count++;
                                } else {
                                        orig->progress = n->progress;
                                }
                                orig->start = g_get_monotonic_time();
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

        if (!n->color_strings[ColFrame]) {
                n->color_strings[ColFrame] = xctx.color_strings[ColFrame][n->urgency];
        }

        n->timeout =
            n->timeout < 0 ? settings.timeouts[n->urgency] : n->timeout;
        n->start = 0;

        n->timestamp = g_get_monotonic_time();

        n->redisplayed = false;

        n->first_render = true;

        if (strlen(n->msg) == 0) {
                notification_close(n, 2);
                if (settings.always_run_script) {
                        notification_run_script(n);
                }
                printf("skipping notification: %s %s\n", n->body, n->summary);
        } else {
                if (id == 0 || !notification_replace_by_id(n))
                        g_queue_insert_sorted(queue, n, notification_cmp_data, NULL);
        }

        char *tmp = g_strconcat(n->summary, " ", n->body, NULL);

        char *tmp_urls = extract_urls(tmp);
        n->urls = string_append(n->urls, tmp_urls, "\n");
        g_free(tmp_urls);

        if (n->actions) {
                n->actions->dmenu_str = NULL;
                for (int i = 0; i < n->actions->count; i += 2) {
                        char *human_readable = n->actions->actions[i + 1];
                        string_replace_char('[', '(', human_readable); // kill square brackets
                        string_replace_char(']', ')', human_readable);

                        char *act_str = g_strdup_printf("#%s [%s]", human_readable, n->appname);
                        if (act_str) {
                                n->actions->dmenu_str = string_append(n->actions->dmenu_str, act_str, "\n");
                                g_free(act_str);
                        }
                }
        }

        g_free(tmp);

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

        wake_up();
        return reason;
}

/*
 * Close the given notification. SEE notification_close_by_id.
 */
int notification_close(notification *n, int reason)
{
        assert(n != NULL);
        return notification_close_by_id(n->id, reason);
}

/*
 * Replace the notification which matches the id field of
 * the new notification. The given notification is inserted
 * right in the same position as the old notification.
 *
 * Returns true, if a matching notification has been found
 * and is replaced. Else false.
 */
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

void notification_update_text_to_render(notification *n)
{
        g_free(n->text_to_render);
        n->text_to_render = NULL;

        char *buf = NULL;

        char *msg = g_strchomp(n->msg);

        /* print dup_count and msg */
        if ((n->dup_count > 0 && !settings.hide_duplicate_count)
            && (n->actions || n->urls) && settings.show_indicators) {
                buf = g_strdup_printf("(%d%s%s) %s",
                                      n->dup_count,
                                      n->actions ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if ((n->actions || n->urls) && settings.show_indicators) {
                buf = g_strdup_printf("(%s%s) %s",
                                      n->actions ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if (n->dup_count > 0 && !settings.hide_duplicate_count) {
                buf = g_strdup_printf("(%d) %s", n->dup_count, msg);
        } else {
                buf = g_strdup(msg);
        }

        /* print age */
        gint64 hours, minutes, seconds;
        gint64 t_delta = g_get_monotonic_time() - n->timestamp;

        if (settings.show_age_threshold >= 0
            && t_delta >= settings.show_age_threshold) {
                hours   = t_delta / G_USEC_PER_SEC / 3600;
                minutes = t_delta / G_USEC_PER_SEC / 60 % 60;
                seconds = t_delta / G_USEC_PER_SEC % 60;

                char *new_buf;
                if (hours > 0) {
                        new_buf =
                            g_strdup_printf("%s (%ldh %ldm %lds old)", buf, hours,
                                            minutes, seconds);
                } else if (minutes > 0) {
                        new_buf =
                            g_strdup_printf("%s (%ldm %lds old)", buf, minutes,
                                            seconds);
                } else {
                        new_buf = g_strdup_printf("%s (%lds old)", buf, seconds);
                }

                g_free(buf);
                buf = new_buf;
        }

        n->text_to_render = buf;
}

/*
 * If the notification has exactly one action, or one is marked as default,
 * invoke it. If there are multiple and no default, open the context menu. If
 * there are no actions, proceed similarly with urls.
 */
void notification_do_action(notification *n) {
        if (n->actions) {
                if (n->actions->count == 2) {
                        action_invoked(n, n->actions->actions[0]);
                        return;
                }
                for (int i = 0; i < n->actions->count; i += 2) {
                        if (strcmp(n->actions->actions[i], "default") == 0) {
                                action_invoked(n, n->actions->actions[i]);
                                return;
                        }
                }
                context_menu();

        } else if (n->urls) {
                if (strstr(n->urls, "\n") == NULL)
                        open_browser(n->urls);
                else
                        context_menu();
        }
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

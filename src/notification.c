/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "notification.h"

#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dbus.h"
#include "dunst.h"
#include "markup.h"
#include "menu.h"
#include "queues.h"
#include "rules.h"
#include "settings.h"
#include "utils.h"
#include "x11/x.h"

static void notification_extract_urls(notification *n);
static void notification_format_message(notification *n);
static void notification_dmenu_string(notification *n);

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
        printf("\turgency: %s\n", notification_urgency_to_string(n->urgency));
        printf("\ttransient: %d\n", n->transient);
        printf("\tformatted: '%s'\n", n->msg);
        printf("\tfg: %s\n", n->colors[ColFG]);
        printf("\tbg: %s\n", n->colors[ColBG]);
        printf("\tframe: %s\n", n->colors[ColFrame]);
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

        const char *urgency = notification_urgency_to_string(n->urgency);

        int pid1 = fork();

        if (pid1) {
                int status;
                waitpid(pid1, &status, 0);
        } else {
                int pid2 = fork();
                if (pid2) {
                        exit(0);
                } else {
                        int ret = execlp(n->script,
                                         n->script,
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
 * Helper function to convert an urgency to a string
 */
const char *notification_urgency_to_string(enum urgency urgency)
{
        switch (urgency) {
        case URG_NONE:
                return "NONE";
        case URG_LOW:
                return "LOW";
        case URG_NORM:
                return "NORMAL";
        case URG_CRIT:
                return "CRITICAL";
        default:
                return "UNDEF";
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
 * Free the actions element
 * @a: (nullable): Pointer to #Actions
 */
void actions_free(Actions *a)
{
        if (!a)
                return;

        g_strfreev(a->actions);
        g_free(a->dmenu_str);
        g_free(a);
}

/*
 * Free a #RawImage
 * @i: (nullable): pointer to #RawImage
 */
void rawimage_free(RawImage *i)
{
        if (!i)
                return;

        g_free(i->data);
        g_free(i);
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
        g_free(n->colors[ColFG]);
        g_free(n->colors[ColBG]);
        g_free(n->colors[ColFrame]);

        actions_free(n->actions);
        rawimage_free(n->raw_icon);

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
void notification_replace_single_field(char **haystack,
                                       char **needle,
                                       const char *replacement,
                                       enum markup_mode markup_mode)
{

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

/*
 * Create notification struct and initialise all fields with either
 *  - the default (if it's not needed to be freed later)
 *  - its undefined representation (NULL, -1)
 *
 * This function is guaranteed to return a valid pointer.
 * @Returns: The generated notification
 */
notification *notification_create(void)
{
        notification *n = g_malloc0(sizeof(notification));

        /* Unparameterized default values */
        n->first_render = true;
        n->markup = settings.markup;
        n->format = settings.format;

        n->timestamp = g_get_monotonic_time();

        n->urgency = URG_NORM;
        n->timeout = -1;

        n->transient = false;
        n->progress = -1;

        return n;
}

/*
 * Sanitize values of notification, apply all matching rules
 * and generate derived fields.
 *
 * @n: the notification to sanitize
 */
void notification_init(notification *n)
{
        /* default to empty string to avoid further NULL faults */
        n->appname  = n->appname  ? n->appname  : g_strdup("unknown");
        n->summary  = n->summary  ? n->summary  : g_strdup("");
        n->body     = n->body     ? n->body     : g_strdup("");
        n->category = n->category ? n->category : g_strdup("");

        /* sanitize urgency */
        if (n->urgency < URG_MIN)
                n->urgency = URG_LOW;
        if (n->urgency > URG_MAX)
                n->urgency = URG_CRIT;

        /* Timeout processing */
        if (n->timeout < 0)
                n->timeout = settings.timeouts[n->urgency];

        /* Icon handling */
        if (n->icon && strlen(n->icon) <= 0)
                g_clear_pointer(&n->icon, g_free);
        if (!n->raw_icon && !n->icon)
                n->icon = g_strdup(settings.icons[n->urgency]);

        /* Color hints */
        if (!n->colors[ColFG])
                n->colors[ColFG] = g_strdup(xctx.colors[ColFG][n->urgency]);
        if (!n->colors[ColBG])
                n->colors[ColBG] = g_strdup(xctx.colors[ColBG][n->urgency]);
        if (!n->colors[ColFrame])
                n->colors[ColFrame] = g_strdup(xctx.colors[ColFrame][n->urgency]);

        /* Sanitize misc hints */
        if (n->progress < 0 || n->progress > 100)
                n->progress = -1;

        /* Process rules */
        rule_apply_all(n);

        /* UPDATE derived fields */
        notification_extract_urls(n);
        notification_dmenu_string(n);
        notification_format_message(n);
}

static void notification_format_message(notification *n)
{
        g_clear_pointer(&n->msg, g_free);

        n->msg = string_replace_all("\\n", "\n", g_strdup(n->format));

        /* replace all formatter */
        for(char *substr = strchr(n->msg, '%');
                  substr;
                  substr = strchr(substr, '%')) {

                char pg[16];
                char *icon_tmp;

                switch(substr[1]) {
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
                        if (n->progress != -1)
                                sprintf(pg, "[%3d%%]", n->progress);

                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->progress != -1 ? pg : "",
                                MARKUP_NO);
                        break;
                case 'n':
                        if (n->progress != -1)
                                sprintf(pg, "%d", n->progress);

                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->progress != -1 ? pg : "",
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
}

static void notification_extract_urls(notification *n)
{
        g_clear_pointer(&n->urls, g_free);

        char *urls_in = string_append(g_strdup(n->summary), n->body, " ");

        char *urls_a = NULL;
        char *urls_img = NULL;
        markup_strip_a(&urls_in, &urls_a);
        markup_strip_img(&urls_in, &urls_img);
        // remove links and images first to not confuse
        // plain urls extraction
        char *urls_text = extract_urls(urls_in);

        n->urls = string_append(n->urls, urls_a, "\n");
        n->urls = string_append(n->urls, urls_img, "\n");
        n->urls = string_append(n->urls, urls_text, "\n");

        g_free(urls_in);
        g_free(urls_a);
        g_free(urls_img);
        g_free(urls_text);
}

static void notification_dmenu_string(notification *n)
{
        if (n->actions) {
                g_clear_pointer(&n->actions->dmenu_str, g_free);
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
}

void notification_update_text_to_render(notification *n)
{
        g_clear_pointer(&n->text_to_render, g_free);

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
void notification_do_action(notification *n)
{
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

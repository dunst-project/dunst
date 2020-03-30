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
#include "icon.h"
#include "log.h"
#include "markup.h"
#include "menu.h"
#include "queues.h"
#include "rules.h"
#include "settings.h"
#include "utils.h"

static void notification_extract_urls(struct notification *n);
static void notification_format_message(struct notification *n);

/* see notification.h */
const char *enum_to_string_fullscreen(enum behavior_fullscreen in)
{
        switch (in) {
        case FS_SHOW: return "show";
        case FS_DELAY: return "delay";
        case FS_PUSHBACK: return "pushback";
        case FS_NULL: return "(null)";
        default:
                LOG_E("Invalid %s enum value in %s:%d", "fullscreen", __FILE__, __LINE__);
                break;
        }
}

struct _notification_private {
        gint refcount;
};

/* see notification.h */
void notification_print(const struct notification *n)
{
        //TODO: use logging info for this
        printf("{\n");
        printf("\tappname: '%s'\n", n->appname);
        printf("\tsummary: '%s'\n", n->summary);
        printf("\tbody: '%s'\n", n->body);
        printf("\ticon: '%s'\n", n->iconname);
        printf("\traw_icon set: %s\n", (n->icon_id && !STR_EQ(n->iconname, n->icon_id)) ? "true" : "false");
        printf("\ticon_id: '%s'\n", n->icon_id);
        printf("\tdesktop_entry: '%s'\n", n->desktop_entry ? n->desktop_entry : "");
        printf("\tcategory: %s\n", n->category);
        printf("\ttimeout: %ld\n", n->timeout/1000);
        printf("\turgency: %s\n", notification_urgency_to_string(n->urgency));
        printf("\ttransient: %d\n", n->transient);
        printf("\tformatted: '%s'\n", n->msg);
        printf("\tfg: %s\n", n->colors.fg);
        printf("\tbg: %s\n", n->colors.bg);
        printf("\tframe: %s\n", n->colors.frame);
        printf("\tfullscreen: %s\n", enum_to_string_fullscreen(n->fullscreen));
        printf("\tprogress: %d\n", n->progress);
        printf("\tstack_tag: %s\n", (n->stack_tag ? n->stack_tag : ""));
        printf("\tid: %d\n", n->id);
        if (n->urls) {
                char *urls = string_replace_all("\n", "\t\t\n", g_strdup(n->urls));
                printf("\turls:\n");
                printf("\t{\n");
                printf("\t\t%s\n", urls);
                printf("\t}\n");
                g_free(urls);
        }
        if (g_hash_table_size(n->actions) == 0) {
                printf("\tactions: {}\n");
        } else {
                gpointer p_key, p_value;
                GHashTableIter iter;
                g_hash_table_iter_init(&iter, n->actions);
                printf("\tactions: {\n");
                while (g_hash_table_iter_next(&iter, &p_key, &p_value))
                        printf("\t\t\"%s\": \"%s\"\n", (char*)p_key, (char*)p_value);
                printf("\t}\n");
        }
        printf("\tscript: %s\n", n->script);
        printf("}\n");
}

/* see notification.h */
void notification_run_script(struct notification *n)
{
        if (STR_EMPTY(n->script))
                return;

        if (n->script_run && !settings.always_run_script)
                return;

        n->script_run = true;

        const char *appname = n->appname ? n->appname : "";
        const char *summary = n->summary ? n->summary : "";
        const char *body = n->body ? n->body : "";
        const char *icon = n->iconname ? n->iconname : "";

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
                                LOG_W("Unable to run script: %s", strerror(errno));
                                exit(EXIT_FAILURE);
                        }
                }
        }
}

/*
 * Helper function to convert an urgency to a string
 */
const char *notification_urgency_to_string(const enum urgency urgency)
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

/* see notification.h */
int notification_cmp(const struct notification *a, const struct notification *b)
{
        if (a->urgency != b->urgency) {
                return b->urgency - a->urgency;
        } else {
                return a->id - b->id;
        }
}

/* see notification.h */
int notification_cmp_data(const void *va, const void *vb, void *data)
{
        struct notification *a = (struct notification *) va;
        struct notification *b = (struct notification *) vb;

        ASSERT_OR_RET(settings.sort, 1);

        return notification_cmp(a, b);
}

bool notification_is_duplicate(const struct notification *a, const struct notification *b)
{
        return STR_EQ(a->appname, b->appname)
            && STR_EQ(a->summary, b->summary)
            && STR_EQ(a->body, b->body)
            && (settings.icon_position != ICON_OFF ? STR_EQ(a->icon_id, b->icon_id) : 1)
            && a->urgency == b->urgency;
}

static void notification_private_free(NotificationPrivate *p)
{
        g_free(p);
}

/* see notification.h */
gint notification_refcount_get(struct notification *n)
{
        assert(n->priv->refcount > 0);
        return g_atomic_int_get(&n->priv->refcount);
}

/* see notification.h */
void notification_ref(struct notification *n)
{
        assert(n->priv->refcount > 0);
        g_atomic_int_inc(&n->priv->refcount);
}

/* see notification.h */
void notification_unref(struct notification *n)
{
        ASSERT_OR_RET(n,);

        assert(n->priv->refcount > 0);
        if (!g_atomic_int_dec_and_test(&n->priv->refcount))
                return;

        g_free(n->appname);
        g_free(n->summary);
        g_free(n->body);
        g_free(n->iconname);
        g_free(n->msg);
        g_free(n->dbus_client);
        g_free(n->category);
        g_free(n->text_to_render);
        g_free(n->urls);
        g_free(n->colors.fg);
        g_free(n->colors.bg);
        g_free(n->colors.frame);
        g_free(n->stack_tag);
        g_free(n->desktop_entry);

        g_hash_table_unref(n->actions);

        if (n->icon)
                g_object_unref(n->icon);
        g_free(n->icon_id);

        notification_private_free(n->priv);

        g_free(n);
}

void notification_icon_replace_path(struct notification *n, const char *new_icon)
{
        ASSERT_OR_RET(n,);
        ASSERT_OR_RET(new_icon,);
        ASSERT_OR_RET(n->iconname != new_icon,);

        g_free(n->iconname);
        n->iconname = g_strdup(new_icon);

        g_clear_object(&n->icon);
        g_clear_pointer(&n->icon_id, g_free);

        n->icon = icon_get_for_name(new_icon, &n->icon_id);
}

void notification_icon_replace_data(struct notification *n, GVariant *new_icon)
{
        ASSERT_OR_RET(n,);
        ASSERT_OR_RET(new_icon,);

        g_clear_object(&n->icon);
        g_clear_pointer(&n->icon_id, g_free);

        n->icon = icon_get_for_data(new_icon, &n->icon_id);
}

/* see notification.h */
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

static NotificationPrivate *notification_private_create(void)
{
        NotificationPrivate *priv = g_malloc0(sizeof(NotificationPrivate));
        g_atomic_int_set(&priv->refcount, 1);

        return priv;
}

/* see notification.h */
struct notification *notification_create(void)
{
        struct notification *n = g_malloc0(sizeof(struct notification));

        n->priv = notification_private_create();

        /* Unparameterized default values */
        n->first_render = true;
        n->markup = settings.markup;
        n->format = settings.format;

        n->timestamp = time_monotonic_now();

        n->urgency = URG_NORM;
        n->timeout = -1;

        n->transient = false;
        n->progress = -1;

        n->script_run = false;
        n->dbus_valid = false;

        n->fullscreen = FS_SHOW;

        n->actions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

        return n;
}

/* see notification.h */
void notification_init(struct notification *n)
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
        if (STR_EMPTY(n->iconname))
                g_clear_pointer(&n->iconname, g_free);
        if (!n->icon && n->iconname) {
                char *icon = g_strdup(n->iconname);
                notification_icon_replace_path(n, icon);
                g_free(icon);
        }
        if (!n->icon && !n->iconname)
                notification_icon_replace_path(n, settings.icons[n->urgency]);

        /* Color hints */
        struct notification_colors defcolors;
        switch (n->urgency) {
                case URG_LOW:
                        defcolors = settings.colors_low;
                        break;
                case URG_NORM:
                        defcolors = settings.colors_norm;
                        break;
                case URG_CRIT:
                        defcolors = settings.colors_crit;
                        break;
                default:
                        g_error("Unhandled urgency type: %d", n->urgency);
        }
        if (!n->colors.fg)
                n->colors.fg = g_strdup(defcolors.fg);
        if (!n->colors.bg)
                n->colors.bg = g_strdup(defcolors.bg);
        if (!n->colors.frame)
                n->colors.frame = g_strdup(defcolors.frame);

        /* Sanitize misc hints */
        if (n->progress < 0)
                n->progress = -1;

        /* Process rules */
        rule_apply_all(n);

        /* UPDATE derived fields */
        notification_extract_urls(n);
        notification_format_message(n);
}

static void notification_format_message(struct notification *n)
{
        g_clear_pointer(&n->msg, g_free);

        n->msg = string_replace_all("\\n", "\n", g_strdup(n->format));

        /* replace all formatter */
        for(char *substr = strchr(n->msg, '%');
                  substr && *substr;
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
                                MARKUP_NO);
                        break;
                case 'b':
                        notification_replace_single_field(
                                &n->msg,
                                &substr,
                                n->body,
                                n->markup);
                        break;
                case 'I':
                        icon_tmp = g_strdup(n->iconname);
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
                                n->iconname ? n->iconname : "",
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
                        LOG_W("format_string has trailing %% character. "
                              "To escape it use %%%%.");
                        substr++;
                        break;
                default:
                        LOG_W("format_string %%%c is unknown.", substr[1]);
                        // shift substr pointer forward,
                        // as we can't interpret the format string
                        substr++;
                        break;
                }
        }

        n->msg = g_strchomp(n->msg);

        /* truncate overlong messages */
        if (strnlen(n->msg, DUNST_NOTIF_MAX_CHARS + 1) > DUNST_NOTIF_MAX_CHARS) {
                char * buffer = g_strndup(n->msg, DUNST_NOTIF_MAX_CHARS);
                g_free(n->msg);
                n->msg = buffer;
        }
}

static void notification_extract_urls(struct notification *n)
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


void notification_update_text_to_render(struct notification *n)
{
        g_clear_pointer(&n->text_to_render, g_free);

        char *buf = NULL;

        char *msg = g_strchomp(n->msg);

        /* print dup_count and msg */
        if ((n->dup_count > 0 && !settings.hide_duplicate_count)
            && (g_hash_table_size(n->actions) || n->urls) && settings.show_indicators) {
                buf = g_strdup_printf("(%d%s%s) %s",
                                      n->dup_count,
                                      g_hash_table_size(n->actions) ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if ((g_hash_table_size(n->actions) || n->urls) && settings.show_indicators) {
                buf = g_strdup_printf("(%s%s) %s",
                                      g_hash_table_size(n->actions) ? "A" : "",
                                      n->urls ? "U" : "", msg);
        } else if (n->dup_count > 0 && !settings.hide_duplicate_count) {
                buf = g_strdup_printf("(%d) %s", n->dup_count, msg);
        } else {
                buf = g_strdup(msg);
        }

        /* print age */
        gint64 hours, minutes, seconds;
        gint64 t_delta = time_monotonic_now() - n->timestamp;

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

/* see notification.h */
void notification_do_action(const struct notification *n)
{
        if (g_hash_table_size(n->actions)) {
                if (g_hash_table_contains(n->actions, "default")) {
                        signal_action_invoked(n, "default");
                        return;
                }
                if (g_hash_table_size(n->actions) == 1) {
                        GList *keys = g_hash_table_get_keys(n->actions);
                        signal_action_invoked(n, keys->data);
                        g_list_free(keys);
                        return;
                }
                context_menu();

        } else if (n->urls) {
                if (strstr(n->urls, "\n"))
                        context_menu();
                else
                        open_browser(n->urls);
        }
}

void notification_invalidate_actions(struct notification *n) {
        g_hash_table_remove_all(n->actions);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

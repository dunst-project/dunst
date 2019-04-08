/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "menu.h"

#include <errno.h>
#include <glib.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dbus.h"
#include "dunst.h"
#include "log.h"
#include "notification.h"
#include "queues.h"
#include "settings.h"
#include "utils.h"

static bool is_initialized = false;
static regex_t url_regex;

struct notification_lock {
        struct notification *n;
        gint64 timeout;
};
static gpointer context_menu_thread(gpointer data);

/**
 * Initializes regexes needed for matching.
 *
 * @return true if initialization succeeded
 */
static bool regex_init(void)
{
        if (is_initialized)
                return true;

        char *regex =
            "\\<(https?://|ftps?://|news://|mailto:|file://|www\\.)"
            "[-[:alnum:]_\\@;/?:&=%$.+!*\x27,~#]*"
            "(\\([-[:alnum:]_\\@;/?:&=%$.+!*\x27,~#]*\\)|[-[:alnum:]_\\@;/?:&=%$+*~])+";
        int code = regcomp(&url_regex, regex, REG_EXTENDED | REG_ICASE);
        if (code != 0) {
                char error_buf[120];
                regerror(code, &url_regex, error_buf, sizeof(error_buf));
                LOG_W("Failed to compile URL-matching regex: %s", error_buf);
                return false;
        } else {
                is_initialized = true;
                return true;
        }
}

void regex_teardown(void)
{
        if (is_initialized) {
                regfree(&url_regex);
                is_initialized = false;
        }
}

/* see menu.h */
char *extract_urls(const char *to_match)
{
        if (!to_match)
                return NULL;

        if (!regex_init())
                return NULL;

        char *urls = NULL;
        const char *p = to_match;
        regmatch_t m;

        while (1) {
                int nomatch = regexec(&url_regex, p, 1, &m, 0);

                if (nomatch || m.rm_so == -1)
                        break;

                int start = m.rm_so + (p - to_match);
                int finish = m.rm_eo + (p - to_match);

                char *match = g_strndup(to_match + start, finish - start);
                urls = string_append(urls, match, "\n");

                g_free(match);
                p += m.rm_eo;
        }
        return urls;
}

/*
 * Open url in browser.
 *
 */
void open_browser(const char *in)
{
        if (!settings.browser_cmd) {
                LOG_C("Unable to open browser: No browser command set.");
                return;
        }

        char *url, *end;
        // If any, remove leading [ linktext ] from URL
        if (*in == '[' && (end = strstr(in, "] ")))
                url = g_strdup(end + 2);
        else
                url = g_strdup(in);

        int argc = 2+g_strv_length(settings.browser_cmd);
        char **argv = g_malloc_n(argc, sizeof(char*));

        memcpy(argv, settings.browser_cmd, argc * sizeof(char*));
        argv[argc-2] = url;
        argv[argc-1] = NULL;

        GError *err = NULL;
        g_spawn_async(NULL,
                      argv,
                      NULL,
                      G_SPAWN_DEFAULT
                        | G_SPAWN_SEARCH_PATH
                        | G_SPAWN_STDOUT_TO_DEV_NULL
                        | G_SPAWN_STDERR_TO_DEV_NULL,
                      NULL,
                      NULL,
                      NULL,
                      &err);

        if (err) {
                LOG_C("Cannot spawn browser: %s", err->message);
                g_error_free(err);
        }

        g_free(argv);
        g_free(url);
}

char *notification_dmenu_string(struct notification *n)
{
        char *dmenu_str = NULL;

        gpointer p_key;
        gpointer p_value;
        GHashTableIter iter;
        g_hash_table_iter_init(&iter, n->actions);
        while (g_hash_table_iter_next(&iter, &p_key, &p_value)) {

                char *key   = (char*) p_key;
                char *value = (char*) p_value;

                char *act_str = g_strdup_printf("#%s (%s) [%d,%s]", value, n->summary, n->id, key);
                dmenu_str = string_append(dmenu_str, act_str, "\n");

                g_free(act_str);
        }
        return dmenu_str;
}

/*
 * Notify the corresponding client
 * that an action has been invoked
 */
void invoke_action(const char *action)
{
        struct notification *invoked = NULL;
        uint id;

        char *data_start, *data_comma, *data_end;

        /* format: #<human readable> (<summary>)[<id>,<action>] */
        data_start = strrchr(action, '[');
        if (!data_start) {
                LOG_W("Invalid action: '%s'", action);
                return;
        }

        id = strtol(++data_start, &data_comma, 10);
        if (*data_comma != ',') {
                LOG_W("Invalid action: '%s'", action);
                return;
        }

        data_end = strchr(data_comma+1, ']');
        if (!data_end) {
                LOG_W("Invalid action: '%s'", action);
                return;
        }

        char *action_key = g_strndup(data_comma+1, data_end-data_comma-1);

        for (const GList *iter = queues_get_displayed();
                          iter;
                          iter = iter->next) {
                struct notification *n = iter->data;
                if (n->id != id)
                        continue;

                if (g_hash_table_contains(n->actions, action_key)) {
                        invoked = n;
                        break;
                }
        }

        if (invoked && action_key) {
                signal_action_invoked(invoked, action_key);
        }

        g_free(action_key);
}

/**
 * Dispatch whatever has been returned by dmenu.
 * If the given result of dmenu is empty or NULL, nothing will be done.
 *
 * @param input The result from dmenu.
 */
void dispatch_menu_result(const char *input)
{
        ASSERT_OR_RET(input,);

        char *in = g_strdup(input);
        g_strstrip(in);

        if (in[0] == '#')
                invoke_action(in + 1);
        else if (in[0] != '\0')
                open_browser(in);

        g_free(in);
}

/** Call dmenu with the specified input. Blocks until dmenu is finished.
 *
 * @param dmenu_input The input string to feed into dmenu
 * @returns the selected string from dmenu
 */
char *invoke_dmenu(const char *dmenu_input)
{
        if (!settings.dmenu_cmd) {
                LOG_C("Unable to open dmenu: No dmenu command set.");
                return NULL;
        }

        ASSERT_OR_RET(STR_FULL(dmenu_input), NULL);

        gint dunst_to_dmenu;
        gint dmenu_to_dunst;
        GError *err = NULL;
        char buf[1024];
        char *ret = NULL;

        g_spawn_async_with_pipes(NULL,
                                 settings.dmenu_cmd,
                                 NULL,
                                 G_SPAWN_DEFAULT
                                   | G_SPAWN_SEARCH_PATH,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &dunst_to_dmenu,
                                 &dmenu_to_dunst,
                                 NULL,
                                 &err);

        if (err) {
                LOG_C("Cannot spawn dmenu: %s", err->message);
                g_error_free(err);
        } else {
                size_t wlen = strlen(dmenu_input);
                if (write(dunst_to_dmenu, dmenu_input, wlen) != wlen) {
                        LOG_W("Cannot feed dmenu with input: %s", strerror(errno));
                }
                close(dunst_to_dmenu);

                ssize_t rlen = read(dmenu_to_dunst, buf, sizeof(buf));
                close(dmenu_to_dunst);

                if (rlen > 0)
                        ret = g_strndup(buf, rlen);
                else
                        LOG_W("Didn't receive input from dmenu.");
        }

        return ret;
}

/* see menu.h */
void context_menu(void)
{
        GError *err = NULL;
        g_thread_unref(g_thread_try_new("dmenu",
                                        context_menu_thread,
                                        NULL,
                                        &err));

        if (err) {
                LOG_C("Cannot start thread to call dmenu: %s", err->message);
                g_error_free(err);
        }
}

static gpointer context_menu_thread(gpointer data)
{
        char *dmenu_input = NULL;
        char *dmenu_output;

        GList *locked_notifications = NULL;

        for (const GList *iter = queues_get_displayed(); iter;
             iter = iter->next) {
                struct notification *n = iter->data;


                // Reference and lock the notification if we need it
                if (n->urls || g_hash_table_size(n->actions)) {
                        notification_ref(n);

                        struct notification_lock *nl =
                                g_malloc(sizeof(struct notification_lock));

                        nl->n = n;
                        nl->timeout = n->timeout;
                        n->timeout = 0;

                        locked_notifications = g_list_prepend(locked_notifications, nl);
                }

                char *dmenu_str = notification_dmenu_string(n);
                dmenu_input = string_append(dmenu_input, dmenu_str, "\n");
                g_free(dmenu_str);

                if (n->urls)
                        dmenu_input = string_append(dmenu_input, n->urls, "\n");
        }

        dmenu_output = invoke_dmenu(dmenu_input);
        dispatch_menu_result(dmenu_output);

        g_free(dmenu_input);
        g_free(dmenu_output);

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

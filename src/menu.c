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
static regex_t cregex;

struct notification_lock {
        struct notification *n;
        gint64 timeout;
};
static gpointer context_menu_thread(gpointer data);

static int regex_init(void)
{
        if (is_initialized)
                return 1;

        char *regex =
            "\\b(https?://|ftps?://|news://|mailto:|file://|www\\.)"
            "[-[:alnum:]_\\@;/?:&=%$.+!*\x27,~#]*"
            "(\\([-[:alnum:]_\\@;/?:&=%$.+!*\x27,~#]*\\)|[-[:alnum:]_\\@;/?:&=%$+*~])+";
        int ret = regcomp(&cregex, regex, REG_EXTENDED | REG_ICASE);
        if (ret != 0) {
                LOG_W("Failed to compile regex.");
                return 0;
        } else {
                is_initialized = true;
                return 1;
        }
}

void regex_teardown(void)
{
        if (is_initialized) {
                regfree(&cregex);
                is_initialized = false;
        }
}

/*
 * Extract all urls from a given string.
 *
 * Return: a string of urls separated by \n
 *
 */
char *extract_urls(const char *to_match)
{
        char *urls = NULL;

        if (!regex_init())
                return NULL;

        const char *p = to_match;
        regmatch_t m;

        while (1) {
                int nomatch = regexec(&cregex, p, 1, &m, 0);
                if (nomatch) {
                        return urls;
                }
                int start;
                int finish;
                if (m.rm_so == -1) {
                        break;
                }
                start = m.rm_so + (p - to_match);
                finish = m.rm_eo + (p - to_match);

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

/*
 * Notify the corresponding client
 * that an action has been invoked
 */
void invoke_action(const char *action)
{
        struct notification *invoked = NULL;
        char *action_identifier = NULL;

        char *appname_begin = strchr(action, '[');
        if (!appname_begin) {
                LOG_W("Invalid action: '%s'", action);
                return;
        }
        appname_begin++;
        int appname_len = strlen(appname_begin) - 1; // remove ]
        int action_len = strlen(action) - appname_len - 3; // remove space, [, ]

        for (const GList *iter = queues_get_displayed(); iter;
             iter = iter->next) {
                struct notification *n = iter->data;
                if (g_str_has_prefix(appname_begin, n->appname) && strlen(n->appname) == appname_len) {
                        if (!n->actions)
                                continue;

                        for (int i = 0; i < n->actions->count; i += 2) {
                                char *a_identifier = n->actions->actions[i];
                                char *name = n->actions->actions[i + 1];
                                if (g_str_has_prefix(action, name) && strlen(name) == action_len) {
                                        invoked = n;
                                        action_identifier = a_identifier;
                                        break;
                                }
                        }
                }
        }

        if (invoked && action_identifier) {
                signal_action_invoked(invoked, action_identifier);
        }
}

/**
 * Dispatch whatever has been returned by dmenu.
 * If the given result of dmenu is empty or NULL, nothing will be done.
 *
 * @param input The result from dmenu.
 */
void dispatch_menu_result(const char *input)
{
        if (!input)
                return;

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

        if (!dmenu_input || *dmenu_input == '\0')
                return NULL;

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

/*
 * Open the context menu that let's the user
 * select urls/actions/etc
 */
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
                if (n->urls || n->actions) {
                        notification_ref(n);

                        struct notification_lock *nl =
                                g_malloc(sizeof(struct notification_lock));

                        nl->n = n;
                        nl->timeout = n->timeout;
                        n->timeout = 0;

                        locked_notifications = g_list_prepend(locked_notifications, nl);
                }

                if (n->urls)
                        dmenu_input = string_append(dmenu_input, n->urls, "\n");

                if (n->actions)
                        dmenu_input =
                            string_append(dmenu_input, n->actions->dmenu_str,
                                          "\n");
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

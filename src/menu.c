/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE
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
#include "settings.h"
#include "notification.h"
#include "utils.h"

static bool is_initialized = false;
static regex_t cregex;

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
                fputs("failed to compile regex", stderr);
                return 0;
        } else {
                is_initialized = true;
                return 1;
        }
}

void regex_teardown(void)
{
        if (is_initialized)
        {
                regfree(&cregex);
                is_initialized = false;
        }
}

/*
 * Exctract all urls from a given string.
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
void open_browser(const char *in) {
        // remove prefix and test url
        char *url = extract_urls(in);
        if (!url)
                return;

        int browser_pid1 = fork();

        if (browser_pid1) {
                g_free(url);
                int status;
                waitpid(browser_pid1, &status, 0);
        } else {
                int browser_pid2 = fork();
                if (browser_pid2) {
                        exit(0);
                } else {
                        char *browser_cmd;
                        
                        if(strncmp(url,"file",4) == 0) {
                                browser_cmd = string_append(settings.file_handler, url, " ");
                        } else {
                                browser_cmd = string_append(settings.browser, url, " ");
                        }

                        char **cmd = g_strsplit(browser_cmd, " ", 0);
                        execvp(cmd[0], cmd);
                }
        }
}

/*
 * Notify the corresponding client
 * that an action has been invoked
 */
void invoke_action(const char *action)
{
        notification *invoked = NULL;
        char *action_identifier = NULL;

        char *appname_begin = strchr(action, '[');
        if (!appname_begin) {
                printf("invalid action: %s\n", action);
                return;
        }
        appname_begin++;
        int appname_len = strlen(appname_begin) - 1; // remove ]
        int action_len = strlen(action) - appname_len - 3; // remove space, [, ]

        for (GList *iter = g_queue_peek_head_link(displayed); iter;
             iter = iter->next) {
                notification *n = iter->data;
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
                action_invoked(invoked, action_identifier);
        }
}

/*
 * Dispatch whatever has been returned
 * by the menu.
 */
void dispatch_menu_result(const char *input)
{
        char *in = g_strdup(input);
        g_strstrip(in);
        if (in[0] == '#') {
                invoke_action(in + 1);
        } else {
                open_browser(in);
        }
        g_free(in);
}

/*
 * Open the context menu that let's the user
 * select urls/actions/etc
 */
void context_menu(void)
{
        if (settings.dmenu_cmd == NULL) {
                fprintf(stderr, "dmenu command not set properly. Cowardly refusing to open the context menu.\n");
                return;
        }
        char *dmenu_input = NULL;

        for (GList *iter = g_queue_peek_head_link(displayed); iter;
             iter = iter->next) {
                notification *n = iter->data;

                if (n->urls)
                        dmenu_input = string_append(dmenu_input, n->urls, "\n");

                if (n->actions)
                        dmenu_input =
                            string_append(dmenu_input, n->actions->dmenu_str,
                                          "\n");
        }

        if (!dmenu_input)
                return;

        char buf[1024] = {0};
        int child_io[2];
        int parent_io[2];
        if (pipe(child_io) != 0) {
                PERR("pipe()", errno);
                g_free(dmenu_input);
                return;
        }
        if (pipe(parent_io) != 0) {
                PERR("pipe()", errno);
                g_free(dmenu_input);
                return;
        }
        int pid = fork();

        if (pid == 0) {
                close(child_io[1]);
                close(parent_io[0]);
                close(0);
                if (dup(child_io[0]) == -1) {
                        PERR("dup()", errno);
                        exit(EXIT_FAILURE);
                }
                close(1);
                if (dup(parent_io[1]) == -1) {
                        PERR("dup()", errno);
                        exit(EXIT_FAILURE);
                }
                execvp(settings.dmenu_cmd[0], settings.dmenu_cmd);
        } else {
                close(child_io[0]);
                close(parent_io[1]);
                size_t wlen = strlen(dmenu_input);
                if (write(child_io[1], dmenu_input, wlen) != wlen) {
                        PERR("write()", errno);
                }
                close(child_io[1]);

                size_t len = read(parent_io[0], buf, 1023);

                waitpid(pid, NULL, 0);

                if (len == 0) {
                        g_free(dmenu_input);
                        return;
                }
        }

        close(parent_io[0]);

        dispatch_menu_result(buf);

        g_free(dmenu_input);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

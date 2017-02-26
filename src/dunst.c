/* copyright 2012 - 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE
#define XLIB_ILLEGAL_ACCESS

#include "dunst.h"

#include <X11/Xlib.h>
#include <glib-unix.h>
#include <glib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dbus.h"
#include "menu.h"
#include "notification.h"
#include "option_parser.h"
#include "settings.h"
#include "x11/x.h"
#include "x11/screen.h"

#define LENGTH(X)               (sizeof X / sizeof X[0])

#ifndef VERSION
#define VERSION "version info needed"
#endif

#define MSG 1
#define INFO 2
#define DEBUG 3

typedef struct _x11_source {
        GSource source;
        Display *dpy;
        Window w;
} x11_source_t;

/* index of colors fit to urgency level */
bool pause_display = false;

GMainLoop *mainloop = NULL;

/* notification lists */
GQueue *queue = NULL;           /* all new notifications get into here */
GQueue *displayed = NULL;       /* currently displayed notifications */
GQueue *history = NULL;         /* history of displayed notifications */
GSList *rules = NULL;

/* misc funtions */

void check_timeouts(void)
{
        /* nothing to do */
        if (displayed->length == 0)
                return;

        for (GList * iter = g_queue_peek_head_link(displayed); iter;
             iter = iter->next) {
                notification *n = iter->data;

                /* don't timeout when user is idle */
                if (x_is_idle()) {
                        n->start = time(NULL);
                        continue;
                }

                /* skip hidden and sticky messages */
                if (n->start == 0 || n->timeout == 0) {
                        continue;
                }

                /* remove old message */
                if (difftime(time(NULL), n->start) > n->timeout) {
                        /* close_notification may conflict with iter, so restart */
                        notification_close(n, 1);
                        check_timeouts();
                        return;
                }
        }
}

void update_lists()
{
        int limit;

        check_timeouts();

        if (pause_display) {
                while (displayed->length > 0) {
                        g_queue_insert_sorted(queue, g_queue_pop_head(displayed),
                                              notification_cmp_data, NULL);
                }
                return;
        }

        if (xctx.geometry.h == 0) {
                limit = 0;
        } else if (xctx.geometry.h == 1) {
                limit = 1;
        } else if (settings.indicate_hidden) {
                limit = xctx.geometry.h - 1;
        } else {
                limit = xctx.geometry.h;
        }

        /* move notifications from queue to displayed */
        while (queue->length > 0) {

                if (limit > 0 && displayed->length >= limit) {
                        /* the list is full */
                        break;
                }

                notification *n = g_queue_pop_head(queue);

                if (!n)
                        return;
                n->start = time(NULL);
                if (!n->redisplayed && n->script) {
                        notification_run_script(n);
                }

                g_queue_insert_sorted(displayed, n, notification_cmp_data,
                                      NULL);
        }
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

        wake_up();
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

void wake_up(void)
{
        run(NULL);
}

static int get_sleep_time(void)
{

        if (settings.show_age_threshold == 0) {
                /* we need to update every second */
                return 1;
        }

        bool have_ttl = false;
        int min_ttl = 0;
        int max_age = 0;
        for (GList *iter = g_queue_peek_head_link(displayed); iter;
                        iter = iter->next) {
                notification *n = iter->data;

                max_age = MAX(max_age, notification_get_age(n));
                int ttl = notification_get_ttl(n);
                if (ttl >= 0) {
                        if (have_ttl) {
                                min_ttl = MIN(min_ttl, ttl);
                        } else {
                                min_ttl = ttl;
                                have_ttl = true;
                        }
                }
        }

        int min_timeout;
        int show_age_timeout = settings.show_age_threshold - max_age;

        if (show_age_timeout < 1) {
                return 1;
        }

        if (!have_ttl) {
                min_timeout = show_age_timeout;
        } else {
                min_timeout = MIN(show_age_timeout, min_ttl);
        }

        /* show_age_timeout might be negative */
        if (min_timeout < 1) {
                return 1;
        } else {
                return min_timeout;
        }
}

gboolean run(void *data)
{
        update_lists();
        static int timeout_cnt = 0;
        static int next_timeout = 0;

        if (data) {
                timeout_cnt--;
        }

        if (displayed->length > 0 && !xctx.visible && !pause_display) {
            x_win_show();
        }

        if (xctx.visible && (pause_display || displayed->length == 0)) {
            x_win_hide();
        }

        if (xctx.visible) {
            x_win_draw();
        }

        if (xctx.visible) {
                int now = time(NULL);
                int sleep = get_sleep_time();

                if (sleep > 0) {
                        int timeout_at = now + sleep;
                        if (timeout_cnt == 0 || timeout_at < next_timeout) {
                                g_timeout_add_seconds(sleep, run, mainloop);
                                next_timeout = timeout_at;
                                timeout_cnt++;
                        }
                }
        }

        /* always return false to delete timers */
        return false;
}

gboolean pause_signal(gpointer data)
{
        pause_display = true;
        wake_up();

        return G_SOURCE_CONTINUE;
}

gboolean unpause_signal(gpointer data)
{
        pause_display = false;
        wake_up();

        return G_SOURCE_CONTINUE;
}

gboolean quit_signal(gpointer data)
{
        g_main_loop_quit(mainloop);

        return G_SOURCE_CONTINUE;
}

static void teardown_notification(gpointer data)
{
        notification *n = data;
        notification_free(n);
}

static void teardown(void)
{
        regex_teardown();

        g_queue_free_full(history, teardown_notification);
        g_queue_free_full(displayed, teardown_notification);
        g_queue_free_full(queue, teardown_notification);

        x_free();
}

int dunst_main(int argc, char *argv[])
{

        history = g_queue_new();
        displayed = g_queue_new();
        queue = g_queue_new();

        cmdline_load(argc, argv);

        if (cmdline_get_bool("-v/-version", false, "Print version")
            || cmdline_get_bool("--version", false, "Print version")) {
                print_version();
        }

        char *cmdline_config_path;
        cmdline_config_path =
            cmdline_get_string("-conf/-config", NULL,
                               "Path to configuration file");
        load_settings(cmdline_config_path);

        if (cmdline_get_bool("-h/-help", false, "Print help")
            || cmdline_get_bool("--help", false, "Print help")) {
                usage(EXIT_SUCCESS);
        }

        int owner_id = initdbus();

        x_setup();

        if (settings.startup_notification) {
                notification *n = notification_create();
                n->appname = g_strdup("dunst");
                n->summary = g_strdup("startup");
                n->body = g_strdup("dunst is up and running");
                n->progress = 0;
                n->timeout = 10;
                n->markup = MARKUP_NO;
                n->urgency = LOW;
                notification_init(n, 0);
        }

        mainloop = g_main_loop_new(NULL, FALSE);

        GPollFD dpy_pollfd = { xctx.dpy->fd,
                G_IO_IN | G_IO_HUP | G_IO_ERR, 0
        };

        GSourceFuncs x11_source_funcs = {
                x_mainloop_fd_prepare,
                x_mainloop_fd_check,
                x_mainloop_fd_dispatch,
                NULL,
                NULL,
                NULL
        };

        GSource *x11_source =
            g_source_new(&x11_source_funcs, sizeof(x11_source_t));
        ((x11_source_t *) x11_source)->dpy = xctx.dpy;
        ((x11_source_t *) x11_source)->w = xctx.win;
        g_source_add_poll(x11_source, &dpy_pollfd);

        g_source_attach(x11_source, NULL);

        guint pause_src = g_unix_signal_add(SIGUSR1, pause_signal, NULL);
        guint unpause_src = g_unix_signal_add(SIGUSR2, unpause_signal, NULL);

        /* register SIGINT/SIGTERM handler for
         * graceful termination */
        guint term_src = g_unix_signal_add(SIGTERM, quit_signal, NULL);
        guint int_src = g_unix_signal_add(SIGINT, quit_signal, NULL);

        run(NULL);
        g_main_loop_run(mainloop);
        g_main_loop_unref(mainloop);

        /* remove signal handler watches */
        g_source_remove(pause_src);
        g_source_remove(unpause_src);
        g_source_remove(term_src);
        g_source_remove(int_src);

        g_source_destroy(x11_source);

        dbus_tear_down(owner_id);

        teardown();

        return 0;
}

void usage(int exit_status)
{
        fputs("usage:\n", stderr);
        char *us = cmdline_create_usage();
        puts(us);
        exit(exit_status);
}

void print_version(void)
{
        printf
            ("Dunst - A customizable and lightweight notification-daemon %s\n",
             VERSION);
        exit(EXIT_SUCCESS);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

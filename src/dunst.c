/* copyright 2012 - 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "dunst.h"

#include <X11/Xlib.h>
#include <glib-unix.h>
#include <glib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbus.h"
#include "draw.h"
#include "log.h"
#include "menu.h"
#include "notification.h"
#include "option_parser.h"
#include "queues.h"
#include "settings.h"
#include "utils.h"
#include "x11/screen.h"
#include "x11/x.h"

#ifndef VERSION
#define VERSION "version info needed"
#endif

/* index of colors fit to urgency level */

GMainLoop *mainloop = NULL;

GSList *rules = NULL;

/* misc funtions */
static gboolean run(void *data);

void wake_up(void)
{
        run(NULL);
}

static gboolean run(void *data)
{
        LOG_D("RUN");

        bool fullscreen = have_fullscreen_window();

        queues_check_timeouts(x_is_idle(), fullscreen);
        queues_update(fullscreen);

        static gint64 next_timeout = 0;

        if (!win->visible && queues_length_displayed() > 0) {
                x_win_show();
        }

        if (win->visible && queues_length_displayed() == 0) {
                x_win_hide();
        }

        if (win->visible) {
                draw();
        }

        if (win->visible) {
                gint64 now = time_monotonic_now();
                gint64 sleep = queues_get_next_datachange(now);
                gint64 timeout_at = now + sleep;

                if (sleep >= 0) {
                        if (next_timeout < now || timeout_at < next_timeout) {
                                g_timeout_add(sleep/1000, run, NULL);
                                next_timeout = timeout_at;
                        }
                }
        }

        /* If the execution got triggered by g_timeout_add,
         * we have to remove the timeout (which is actually a
         * recurring interval), as we have set a new one
         * by ourselves.
         */
        return G_SOURCE_REMOVE;
}

gboolean pause_signal(gpointer data)
{
        queues_pause_on();
        wake_up();

        return G_SOURCE_CONTINUE;
}

gboolean unpause_signal(gpointer data)
{
        queues_pause_off();
        wake_up();

        return G_SOURCE_CONTINUE;
}

gboolean quit_signal(gpointer data)
{
        g_main_loop_quit(mainloop);

        return G_SOURCE_CONTINUE;
}

static void teardown(void)
{
        regex_teardown();

        teardown_queues();

        draw_deinit();
}

int dunst_main(int argc, char *argv[])
{

        queues_init();

        cmdline_load(argc, argv);

        dunst_log_init(false);

        if (cmdline_get_bool("-v/-version", false, "Print version")
            || cmdline_get_bool("--version", false, "Print version")) {
                print_version();
        }

        char *verbosity = cmdline_get_string("-verbosity", NULL, "Minimum level for message");
        log_set_level_from_string(verbosity);
        g_free(verbosity);

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

        if (settings.startup_notification) {
                notification *n = notification_create();
                n->id = 0;
                n->appname = g_strdup("dunst");
                n->summary = g_strdup("startup");
                n->body = g_strdup("dunst is up and running");
                n->progress = -1;
                n->timeout = 10 * G_USEC_PER_SEC;
                n->markup = MARKUP_NO;
                n->urgency = URG_LOW;
                notification_init(n);
                queues_notification_insert(n);
                // we do not call wakeup now, wake_up does not work here yet
        }

        mainloop = g_main_loop_new(NULL, FALSE);

        draw_setup();

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

        dbus_tear_down(owner_id);

        teardown();

        return 0;
}

void usage(int exit_status)
{
        puts("usage:\n");
        const char *us = cmdline_create_usage();
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

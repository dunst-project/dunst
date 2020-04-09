/* copyright 2012 - 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "dunst.h"

#include <assert.h>
#include <glib.h>
#include <glib-unix.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>

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

GMainLoop *mainloop = NULL;

static struct dunst_status status;

/* see dunst.h */
void dunst_status(const enum dunst_status_field field,
                  bool value)
{
        switch (field) {
        case S_FULLSCREEN:
                status.fullscreen = value;
                break;
        case S_IDLE:
                status.idle = value;
                break;
        case S_RUNNING:
                status.running = value;
                break;
        default:
                LOG_E("Invalid %s enum value in %s:%d", "dunst_status", __FILE__, __LINE__);
                break;
        }
}

/* see dunst.h */
struct dunst_status dunst_status_get(void)
{
        return status;
}

/* misc functions */
static gboolean run(void *data);

void wake_up(void)
{
        run(NULL);
}

static gboolean run(void *data)
{
        static gint64 next_timeout = 0;

        LOG_D("RUN");

        dunst_status(S_FULLSCREEN, have_fullscreen_window());
        dunst_status(S_IDLE, x_is_idle());

        queues_update(status);

        bool active = queues_length_displayed() > 0;

        if (active) {
                // Call draw before showing the window to avoid flickering
                draw();
                x_win_show(win);
        } else {
                x_win_hide(win);
        }

        if (active) {
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
        dunst_status(S_RUNNING, false);
        wake_up();

        return G_SOURCE_CONTINUE;
}

gboolean unpause_signal(gpointer data)
{
        dunst_status(S_RUNNING, true);
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

        queues_teardown();

        draw_deinit();
}

int dunst_main(int argc, char *argv[])
{

        dunst_status(S_RUNNING, true);
        dunst_status(S_IDLE, false);

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

        int dbus_owner_id = dbus_init();

        mainloop = g_main_loop_new(NULL, FALSE);

        draw_setup();

        guint pause_src = g_unix_signal_add(SIGUSR1, pause_signal, NULL);
        guint unpause_src = g_unix_signal_add(SIGUSR2, unpause_signal, NULL);

        /* register SIGINT/SIGTERM handler for
         * graceful termination */
        guint term_src = g_unix_signal_add(SIGTERM, quit_signal, NULL);
        guint int_src = g_unix_signal_add(SIGINT, quit_signal, NULL);

        if (settings.startup_notification) {
                struct notification *n = notification_create();
                n->id = 0;
                n->appname = g_strdup("dunst");
                n->summary = g_strdup("startup");
                n->body = g_strdup("dunst is up and running");
                n->progress = -1;
                n->timeout = S2US(10);
                n->markup = MARKUP_NO;
                n->urgency = URG_LOW;
                notification_init(n);
                queues_notification_insert(n);
                // we do not call wakeup now, wake_up does not work here yet
        }

        run(NULL);
        g_main_loop_run(mainloop);
        g_clear_pointer(&mainloop, g_main_loop_unref);

        /* remove signal handler watches */
        g_source_remove(pause_src);
        g_source_remove(unpause_src);
        g_source_remove(term_src);
        g_source_remove(int_src);

        dbus_teardown(dbus_owner_id);

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

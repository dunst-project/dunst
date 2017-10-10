/* copyright 2012 - 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define XLIB_ILLEGAL_ACCESS

#include "dunst.h"

#include <X11/Xlib.h>
#include <glib-unix.h>
#include <glib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbus.h"
#include "menu.h"
#include "notification.h"
#include "option_parser.h"
#include "queues.h"
#include "settings.h"
#include "x11/x.h"
#include "x11/screen.h"

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

GMainLoop *mainloop = NULL;

GSList *rules = NULL;

/* misc funtions */

void wake_up(void)
{
        run(NULL);
}

gboolean run(void *data)
{
        queues_check_timeouts(x_is_idle());
        queues_update();

        static int timeout_cnt = 0;
        static gint64 next_timeout = 0;

        if (data && timeout_cnt > 0) {
                timeout_cnt--;
        }

        if (displayed->length > 0 && !xctx.visible) {
                x_win_show();
        }

        if (xctx.visible && displayed->length == 0) {
                x_win_hide();
        }

        if (xctx.visible) {
                x_win_draw();
        }

        if (xctx.visible) {
                gint64 now = g_get_monotonic_time();
                gint64 sleep = queues_get_next_datachange(now);
                gint64 timeout_at = now + sleep;

                if (sleep >= 0) {
                        if (timeout_cnt == 0 || timeout_at < next_timeout) {
                                g_timeout_add(sleep/1000, run, mainloop);
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

        x_free();
}

int dunst_main(int argc, char *argv[])
{

        queues_init();

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
                n->timeout = 10 * G_USEC_PER_SEC;
                n->markup = MARKUP_NO;
                n->urgency = LOW;
                notification_init(n);
                queues_notification_insert(n, 0);
                // we do not call wakeup now, wake_up does not work here yet
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

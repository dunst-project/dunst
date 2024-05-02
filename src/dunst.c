/* copyright 2012 - 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "dunst.h"

#include <assert.h>
#include <glib.h>
#include <glib-unix.h>
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
#include "output.h"

GMainLoop *mainloop = NULL;

static struct dunst_status status;
static bool setup_done = false;
static char **config_paths = NULL;

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
        default:
                LOG_E("Invalid %s enum value in %s:%d for bool type", "dunst_status", __FILE__, __LINE__);
                break;
        }
}

void dunst_status_int(const enum dunst_status_field field,
                  int value)
{
        switch (field) {
        case S_PAUSE_LEVEL:
                status.pause_level = value;
                break;
        default:
                LOG_E("Invalid %s enum value in %s:%d for int type", "dunst_status", __FILE__, __LINE__);
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

/**
 * The reason for which the run function was invoked.
 * - DUNST_TIMER: the timer until the next event in queue has expired (routine
 *   wakeup)
 * - DUNST_WAKEUP: an external event (eg. new notification) triggered a call to
 *   wake_up (unscheduled wakeup)
 */
enum dunst_run_reason {
        DUNST_TIMER,
        DUNST_WAKEUP,
};

const char* dunst_run_reason_str(enum dunst_run_reason reason) {
        switch(reason) {
                case DUNST_TIMER:
                        return "DUNST_TIMER";
                case DUNST_WAKEUP:
                        return "DUNST_WAKEUP";
                default:
                        return "BAD VALUE";
        }
}

void wake_up(void)
{
        // If wake_up is being called before the output has been setup we should
        // return.
        if (!setup_done) {
                LOG_D("Ignoring wake up");
                return;
        }

        LOG_D("Waking up");
        run(GINT_TO_POINTER(DUNST_WAKEUP));
}

static gboolean run(void *data)
{
        /* Timer gestion
         * =============
         *
         * - At any time (except transiently during the execution of `run`), at
         *   most one glib timeout source (or "timer" here) exists. If `run`
         *   was invoked by a timer, it will be deleted upon return, as the
         *   function always returns G_SOURCE_REMOVE.
         * - Furthermore, if next_timeout_id is not null, a timer with this
         *   glib source id exists and is running. As a consequence,
         *   - if reason is DUNST_TIMER, it is the timer that triggered the
         *     current call to run;
         *   - if reason is DUNST_WAKEUP, this timer was scheduled some time in
         *     the future (or in the past, but not yet executed my the main loop,
         *     which is equivalent for our purpose).
         *
         * Thus, in each call to run,
         * - if reason is DUNST_WAKEUP and next_timeout_id != 0, we delete this
         *   timer -- we now have more recent information on which we can
         *   decide of a (maybe) better timeout.
         * - in any case, we reset next_timeout_id to 0.
         * - if there is any event to be run in the future, we set a new timer
         *   to this time, and update next_timeout_id accordingly.
         */

        static guint next_timeout_id = 0;
        enum dunst_run_reason reason = GPOINTER_TO_INT(data);

        LOG_D("RUN, reason %i: %s", reason, dunst_run_reason_str(reason));
        gint64 now = time_monotonic_now();

        dunst_status(S_FULLSCREEN, output->have_fullscreen_window());
        dunst_status(S_IDLE, output->is_idle());

        queues_update(status, now);

        if(reason == DUNST_WAKEUP && next_timeout_id != 0) {
                // Delete the upcoming timer
                g_source_remove(next_timeout_id);
        }
        next_timeout_id = 0;

        if (!queues_length_displayed()) {
                output->win_hide(win);
                return G_SOURCE_REMOVE;
        }

        // Call draw before showing the window to avoid flickering
        draw();
        output->win_show(win);

        gint64 timeout_at = queues_get_next_datachange(now);
        if (timeout_at != -1) {
                // Previous computations may have taken time, update `now`
                // This might mean that `timeout_at` is now before `now`, so
                // we have to make sure that `sleep` is still positive.
                now = time_monotonic_now();
                gint64 sleep = timeout_at - now;
                sleep = MAX(sleep, 1000); // Sleep at least 1ms

                LOG_D("Sleeping for %li ms", sleep/1000);

                next_timeout_id = g_timeout_add(sleep/1000, run, NULL);
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
        dunst_status_int(S_PAUSE_LEVEL, MAX_PAUSE_LEVEL);
        wake_up();

        return G_SOURCE_CONTINUE;
}

gboolean unpause_signal(gpointer data)
{
        dunst_status_int(S_PAUSE_LEVEL, 0);
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

        g_strfreev(config_paths);
}

void reload(char **const configs)
{
        if (configs) {
                g_strfreev(config_paths);
                config_paths = configs;
        }

        pause_signal(NULL);

        setup_done = false;
        draw_deinit();

        load_settings(config_paths);
        draw_setup();
        setup_done = true;

        queues_reapply_all_rules();

        unpause_signal(NULL);
}

int dunst_main(int argc, const char *argv[])
{

        dunst_status_int(S_PAUSE_LEVEL, 0);
        dunst_status(S_IDLE, false);

        settings_init();

        queues_init();

        cmdline_load(argc, argv);

        dunst_log_init(false);

        if (cmdline_get_bool("-v/-version/--version", false, "Print version")) {
                print_version();
        }

        char *verbosity = cmdline_get_string("-verbosity", NULL, "Minimum level for message");
        log_set_level_from_string(verbosity);
        g_free(verbosity);

        GStrvBuilder *builder = g_strv_builder_new();
        char *path = NULL;
        int start = 1;

        while ((path = cmdline_get_string_offset("-conf/-config", NULL,
                                                 "Path to configuration file", start, &start)))
                g_strv_builder_add(builder, path);

        config_paths = g_strv_builder_end(builder);
        g_strv_builder_unref(builder);

        settings.print_notifications = cmdline_get_bool("-print/--print", false, "Print notifications to stdout");

        settings.startup_notification = cmdline_get_bool("-startup_notification/--startup_notification",
                        false, "Display a notification on startup.");

        /* Help should always be the last to set up as calls to cmdline_get_* (as a side effect) add entries to the usage list. */
        if (cmdline_get_bool("-h/-help/--help", false, "Print help")) {
                usage(EXIT_SUCCESS);
        }

        load_settings(config_paths);
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

        setup_done = true;
        run(GINT_TO_POINTER(DUNST_TIMER)); // The first run() is a scheduled one
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
        printf("Dunst - A customizable and lightweight notification-daemon %s\n", VERSION);
        printf("Compiled on %s with the following options:\n", STR_TO(_CCDATE));

        printf("X11 support: %s\n", X11_SUPPORT ? "enabled" : "disabled");
        printf("Wayland support: %s\n", WAYLAND_SUPPORT ? "enabled" : "disabled");
        printf("SYSCONFDIR set to: %s\n", SYSCONFDIR);

        printf("Compiler flags: %s\n", STR_TO(_CFLAGS));
        printf("Linker flags: %s\n", STR_TO(_LDFLAGS));
        exit(EXIT_SUCCESS);
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

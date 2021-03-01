/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "settings.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "dunst.h"
#include "log.h"
#include "notification.h"
#include "option_parser.h"
#include "rules.h"
#include "utils.h"
#include "x11/x.h"
#include "output.h"

struct settings settings;

static FILE *xdg_config(const char *filename)
{
        const gchar * const * systemdirs = g_get_system_config_dirs();
        const gchar * userdir = g_get_user_config_dir();

        FILE *f;
        char *path;

        path = g_strconcat(userdir, filename, NULL);
        f = fopen(path, "r");
        g_free(path);

        for (const gchar * const *d = systemdirs;
             !f && *d;
             d++) {
                path = g_strconcat(*d, filename, NULL);
                f = fopen(path, "r");
                g_free(path);
        }

        if (!f) {
                f = fopen("/etc/dunst/dunstrc", "r");
        }

        return f;
}

void settings_init() {
        static int count = 0;
        if (count == 0)
                settings = (struct settings) {0};
        count++;
}

void print_rule(struct rule* r) {
        LOG_D("Rule %s", r->name);
        LOG_D("summary %s", r->summary);
        LOG_D("appname %s", r->appname);
        LOG_D("script %s", r->script);
}

void check_and_correct_settings(struct settings *s) {

#ifndef ENABLE_WAYLAND
        if (is_running_wayland()){
                /* We are using xwayland now. Setting force_xwayland to make sure
                 * the idle workaround below is activated */
                settings.force_xwayland = true;
        }
#endif

        if (settings.force_xwayland && is_running_wayland()) {
                if (settings.idle_threshold > 0)
                        LOG_W("Using xwayland. Disabling idle.");
                /* There is no way to detect if the user is idle
                 * on xwayland, so turn this feature off */
                settings.idle_threshold = 0;
        }

        // check sanity of the progress bar options
        {
                if (s->progress_bar_height < (2 * s->progress_bar_frame_width)){
                        LOG_E("setting progress_bar_frame_width is bigger than half of progress_bar_height");
                }
                if (s->progress_bar_max_width < (2 * s->progress_bar_frame_width)){
                        LOG_E("setting progress_bar_frame_width is bigger than half of progress_bar_max_width");
                }
                if (s->progress_bar_max_width < s->progress_bar_min_width){
                        LOG_E("setting progress_bar_max_width is smaller than progress_bar_min_width");
                }
        }

        // restrict the icon size to a reasonable limit if we have a fixed width.
        // Otherwise the layout will be broken by too large icons.
        // See https://github.com/dunst-project/dunst/issues/540
        if (s->geometry.width_set && s->geometry.w != 0) {
                const int icon_size_limit = s->geometry.w / 2;
                if (   s->max_icon_size == 0
                    || s->max_icon_size > icon_size_limit) {
                        if (s->max_icon_size != 0) {
                                LOG_W("Max width was set to %d but got a max_icon_size of %d, too large to use. Setting max_icon_size=%d",
                                        s->geometry.w, s->max_icon_size, icon_size_limit);
                        } else {
                                LOG_I("Max width was set but max_icon_size is unlimited. Limiting icons to %d pixels", icon_size_limit);
                        }

                        s->max_icon_size = icon_size_limit;
                }
        }
}

void load_settings(char *cmdline_config_path)
{

        FILE *config_file = NULL;

        if (cmdline_config_path) {
                if (STR_EQ(cmdline_config_path, "-")) {
                        config_file = stdin;
                } else {
                        config_file = fopen(cmdline_config_path, "r");
                }

                if (!config_file) {
                        DIE("Cannot find config file: '%s'", cmdline_config_path);
                }
        }

        if (!config_file) {
                config_file = xdg_config("/dunst/dunstrc");
        }

        if (!config_file) {
                /* Fall back to just "dunstrc", which was used before 2013-06-23
                 * (before v0.2). */
                config_file = xdg_config("/dunstrc");
        }

        if (!config_file) {
                LOG_W("No dunstrc found.");
        }

        load_ini_file(config_file);
        settings_init();
        set_defaults();
        save_settings();

        check_and_correct_settings(&settings);

        for (GSList *iter = rules; iter; iter = iter->next) {
                struct rule *r = iter->data;
                print_rule(r);
        }

        if (config_file) {
                fclose(config_file);
                free_ini();
        }
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

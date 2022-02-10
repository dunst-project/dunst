/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "settings.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "dunst.h"
#include "log.h"
#include "notification.h"
#include "option_parser.h"
#include "ini.h"
#include "rules.h"
#include "utils.h"
#include "x11/x.h"
#include "output.h"

#ifdef SYSCONFDIR
#define XDG_CONFIG_DIRS_DEFAULT SYSCONFDIR // alternative default
#else
#define XDG_CONFIG_DIRS_DEFAULT "/etc/xdg"
#endif

// path to dunstrc, relative to config directory
#define RPATH_RC "dunst/dunstrc"
#define RPATH_RC_D RPATH_RC ".d/*.conf"

struct settings settings;

/**
 * Tries to open all existing config files in *descending* order of importance.
 * If cmdline_config_path is not NULL return early after trying to open the
 * referenced file.
 *
 * @param cmdline_config_path
 * @returns GQueue* of FILE* to config files
 * @retval empty GQueue* if no file could be opened
 *
 * Use g_queue_pop_tail() to retrieve FILE* in *ascending* order of importance
 *
 * Use g_queue_free() to free if not NULL
 */
static GQueue *open_conf_files(char *cmdline_config_path) {
        FILE *config_file;

        // Used as stack, least important pushed last but popped first.
        GQueue *config_files = g_queue_new();

        if (cmdline_config_path) {
                config_file = STR_EQ(cmdline_config_path, "-")
                              ? stdin
                              : fopen(cmdline_config_path, "r");

                if (config_file) {
                        LOG_I(MSG_FOPEN_SUCCESS(cmdline_config_path, config_file));
                        g_queue_push_tail(config_files, config_file);
                } else {
                        // warn because we exit early
                        LOG_W(MSG_FOPEN_FAILURE(cmdline_config_path));
                }

                return config_files; // ignore other config files if '-conf' given
        }

        /*
         * Fix peculiar behaviour if installed to a local PREFIX, i.e.
         * /usr/local, when SYSCONFDIR should be /usr/local/etc/xdg and not
         * /etc/xdg, hence use SYSCONFDIR (defined at compile time, see
         * config.mk) as default for XDG_CONFIG_DIRS. The spec says 'should' and
         * not 'must' use /etc/xdg.
         * Users/admins can override this by explicitly setting XDG_CONFIG_DIRS
         * to their liking at runtime or by setting SYSCONFDIR=/etc/xdg at
         * compile time.
         */
        gchar * const xdg_cdirs = g_strdup(g_getenv("XDG_CONFIG_DIRS"));
        gchar * const xdg_config_dirs = xdg_cdirs && strnlen((gchar *) xdg_cdirs, 1)
                                        ? g_strdup(xdg_cdirs)
                                        : g_strdup(XDG_CONFIG_DIRS_DEFAULT);
        g_free(xdg_cdirs);

        /*
         * Prepend XDG_CONFIG_HOME, most important first because XDG_CONFIG_DIRS
         * is already ordered that way.
         */
        gchar * const all_conf_dirs = g_strconcat(g_get_user_config_dir(), ":",
                                                  g_strdup(xdg_config_dirs), NULL);
        g_free(xdg_config_dirs);
        LOG_D("Config directories: '%s'", all_conf_dirs);

        for (gchar * const *d = string_to_array(all_conf_dirs, ":"); *d; d++) {
                gchar * const path = string_to_path(g_strconcat(*d, "/", RPATH_RC, NULL));
                if ((config_file = fopen(path, "r"))) {
                        LOG_I(MSG_FOPEN_SUCCESS(path, config_file));
                        g_queue_push_tail(config_files, config_file);
                } else
                        // debug level because of low relevance
                        LOG_D(MSG_FOPEN_FAILURE(path));
                g_free(path);
        }
        g_free(all_conf_dirs);

        return config_files;
}

void settings_init() {
        static bool init_done = false;
        if (!init_done) {
                LOG_D("Initializing settings");
                settings = (struct settings) {0};

                init_done = true;
        }
}

void print_rule(struct rule* r) {
        LOG_D("Rule %s", r->name);
        LOG_D("summary %s", r->summary);
        LOG_D("appname %s", r->appname);
        LOG_D("script %s", r->script);
        LOG_D("frame %s", r->fc);
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
                        DIE("setting progress_bar_frame_width is bigger than half of progress_bar_height");
                }
                if (s->progress_bar_max_width < (2 * s->progress_bar_frame_width)){
                        DIE("setting progress_bar_frame_width is bigger than half of progress_bar_max_width");
                }
                if (s->progress_bar_max_width < s->progress_bar_min_width){
                        DIE("setting progress_bar_max_width is smaller than progress_bar_min_width");
                }
                if (s->progress_bar_min_width > s->width.max) {
                        LOG_W("Progress bar min width is greater than the max width of the notification.");
                }
        }

        // restrict the icon size to a reasonable limit if we have a fixed width.
        // Otherwise the layout will be broken by too large icons.
        // See https://github.com/dunst-project/dunst/issues/540
        if (s->width.max > 0) {
                const int icon_size_limit = s->width.max / 2;
                if (   s->max_icon_size == 0
                    || s->max_icon_size > icon_size_limit) {
                        if (s->max_icon_size != 0) {
                                LOG_W("Max width was set to %d but got a max_icon_size of %d, too large to use. Setting max_icon_size=%d",
                                        s->width.max, s->max_icon_size, icon_size_limit);
                        } else {
                                LOG_I("Max width was set but max_icon_size is unlimited. Limiting icons to %d pixels", icon_size_limit);
                        }

                        s->max_icon_size = icon_size_limit;
                }
        }

        int text_icon_padding = settings.text_icon_padding != 0 ? settings.text_icon_padding : settings.h_padding;
        int max_text_width = settings.width.max - settings.max_icon_size - text_icon_padding - 2 * settings.h_padding;
        if (max_text_width < 10) {
                DIE("max_icon_size and horizontal padding are too large for the given width");
        }

}

void load_settings(char *cmdline_config_path) {
        FILE *config_file;
        GQueue *config_files;

        settings_init();
        LOG_D("Setting defaults");
        set_defaults();

        config_files = open_conf_files(cmdline_config_path);
        if (g_queue_is_empty(config_files)) {
                LOG_W("No configuration file, using defaults");
        } else { // Add entries from all files, most important last
                while ((config_file = g_queue_pop_tail(config_files))) {
                        LOG_I("Parsing config, fd: '%d'", fileno(config_file));
                        struct ini *ini = load_ini_file(config_file);
                        fclose(config_file);

                        LOG_D("Loading settings");
                        save_settings(ini);

                        LOG_D("Checking/correcting settings");
                        check_and_correct_settings(&settings);

                        finish_ini(ini);
                        free(ini);
                }
        }
        g_queue_free(config_files);

        for (GSList *iter = rules; iter; iter = iter->next) {
                struct rule *r = iter->data;
                print_rule(r);
        }
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

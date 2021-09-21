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

/**
 * Tries to open all existing config files in *descending* order of importance
 *
 * @returns GQueue* of FILE* to config files
 *
 * Use g_queue_pop_tail() to retrieve FILE* in *ascending* order of importance
 *
 * Use g_queue_free() to free if not NULL
 */
static GQueue *xdg_config() {
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
        const gchar * xdg_config_dirs = g_getenv("XDG_CONFIG_DIRS");
        xdg_config_dirs = xdg_config_dirs && strnlen(g_strstrip((gchar *) xdg_config_dirs), 1)
                          ? xdg_config_dirs
                          : SYSCONFDIR; // alternative default

        /*
         * Prepend XDG_CONFIG_HOME, most important first because XDG_CONFIG_DIRS
         * is already ordered that way.
         */
        gchar * const all_conf_dirs = g_strconcat(g_get_user_config_dir(), ":",
                                                  xdg_config_dirs, NULL);
        LOG_D("Config directories: '%s'", all_conf_dirs);

        FILE *f = NULL;

        /*
         * Possible relative paths to dunstrc. First match wins.
         * TODO: Maybe warn about deprecated location if found?
         */
        const char * const rel_paths[] = { "dunst/dunstrc",
                                           "dunstrc", // deprecated since v0.2 (2013-06-23)
                                           NULL };

        // Used as stack, least important pushed last but popped first.
        GQueue *config_files = g_queue_new();
        for (gchar * const *d = string_to_array(all_conf_dirs, ":"); *d; d++)
                // FIXME: Flatten this out after deprecation grace period
                for (const char * const *rp = rel_paths; *rp; rp++) {
                        f = fopen(string_to_path(g_strconcat(*d, "/", *rp, NULL)), "r");
                        LOG_I("Trying to open config file '%s' in '%s': '%s'",
                                                          *rp,
                                                                   *d,
                                                                         f ? "HIT" : "MISS");
                        if (f) {
                                g_queue_push_tail(config_files, f);
                                break; // ignore deprecated
                        }
                }
        g_free(all_conf_dirs);

        return config_files;
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

static void apply_settings(FILE *config_file) {
        if (config_file)
                load_ini_file(config_file);

        settings_init();
        set_defaults();
        save_settings();
        check_and_correct_settings(&settings);

        for (GSList *iter = rules; iter; iter = iter->next) {
                struct rule *r = iter->data;
                print_rule(r);
        }

        if (config_file)
                fclose(config_file);

        free_ini();
}

void load_settings(char *cmdline_config_path) {
        FILE *config_file = NULL;
        if (cmdline_config_path) {
                if (STR_EQ(cmdline_config_path, "-"))
                        config_file = stdin;
                else
                        config_file = fopen(cmdline_config_path, "r");

                if (config_file) {
                        apply_settings(config_file);
                        return;
                } else
                        // FIXME: Really DIE or just warn and carry on with defaults?
                        DIE("Cannot find config file: '%s'", cmdline_config_path);
        }

        GQueue *config_files = xdg_config();
        if (g_queue_is_empty(config_files)) {
                LOG_W("No dunstrc found, falling back on internal defaults.");
                apply_settings(NULL);
        } else { // Apply settings over and over, from least to most important.
                while ((config_file = g_queue_pop_tail(config_files)))
                        apply_settings(config_file);
        }
        g_queue_free(config_files);
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

/** @file src/settings.c
 * @brief Take care of the settings.
 */

#include "settings.h"

#include <dirent.h>
#include <fnmatch.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "notification.h"
#include "option_parser.h"
#include "ini.h"
#include "utils.h"
#include "output.h"

#ifndef SYSCONFDIR
/** @brief Fallback for doxygen, mostly.
 *
 * Since this gets defined by $DEFAULT_CPPFLAGS at compile time doxygen seems to
 * miss the correct value.
 */
#define SYSCONFDIR "/usr/local/etc/xdg"
#endif

struct settings settings;
bool print_notifications = false;

/** @brief Filter for scandir().
 *
 * @returns @brief An integer indicating success
 *
 * @retval @brief 1 if file name matches *.conf
 * @retval @brief 0 otherwise
 *
 * @param dent [in] @brief directory entry
 */
static int is_drop_in(const struct dirent *dent) {
        return 0 == fnmatch("*.conf", dent->d_name, FNM_PATHNAME | FNM_PERIOD)
                    ? 1 // success
                    : 0;
}

/** @brief Get all relevant config base directories
 *
 * Returns an array of all XDG config base directories, @e most @e important @e
 * first.
 *
 * @returns A %NULL-terminated array of gchar* strings representing the paths
 * of all XDG base directories in @e descending order of importance.
 *
 * The result @e must @e not be freed! The array is cached in a static variable,
 * so it is OK to call this again instead of caching its return value.
 */
static GPtrArray *get_xdg_conf_basedirs(void) {
        GPtrArray *arr = g_ptr_array_new_full(4, g_free);
        g_ptr_array_add(arr, g_build_filename(g_get_user_config_dir(), "dunst", NULL));

        /*
         * A default of SYSCONFDIR is set to separate installs to a
         * local PREFIX. With this default /usr/local/etc/xdg is set a
         * system-wide config location and not /etc/xdg. Users/admins
         * can override this by explicitly setting XDG_CONFIG_DIRS to
         * their liking at runtime or by setting SYSCONFDIR=/etc/xdg at
         * compile time.
         */
        add_paths_from_env(arr, "XDG_CONFIG_DIRS", "dunst", SYSCONFDIR);
        return arr;
}

static void config_files_add_drop_ins(GPtrArray *config_files, const char *path) {
        int insert_index = config_files->len;
        if (insert_index == 0) {
                // there is no base config file
                return;
        }
        char *drop_in_dir = g_strconcat(path, ".d", NULL);
        struct dirent **drop_ins = NULL;
        int n = scandir(drop_in_dir, &drop_ins, is_drop_in, alphasort);

        if (n == -1) {
                // Scandir error. Most likely the directory doesn't exist.
                g_free(drop_in_dir);
                return;
        }

        while (n--) {
                char *drop_in = g_strconcat(drop_in_dir, "/",
                                drop_ins[n]->d_name, NULL);
                LOG_D("Found drop-in: %s\n", drop_in);
                g_ptr_array_insert(config_files, insert_index, drop_in);
                g_free(drop_ins[n]);
        }

        g_free(drop_in_dir);
        g_free(drop_ins);
}

/** @brief Find all config files.
 *
 * Searches the default config locations most important config file and it's
 * drop-ins and puts their locations in a GPtrArray, @e most important last.
 *
 * The returned GPtrArray and it's elements are owned by the caller.
 */
static GPtrArray* get_conf_files(void) {
        GPtrArray *config_locations = get_xdg_conf_basedirs();
        GPtrArray *config_files = g_ptr_array_new_full(3, g_free);
        char *dunstrc_location = NULL;
        for (size_t i = 0; i < config_locations->len; i++) {
                dunstrc_location = g_build_filename(config_locations->pdata[i],
                                "dunstrc", NULL);
                LOG_D("Trying config location: %s", dunstrc_location);
                if (is_readable_file(dunstrc_location)) {
                        g_ptr_array_add(config_files, dunstrc_location);
                        break;
                }
        }

        config_files_add_drop_ins(config_files, dunstrc_location);

        g_ptr_array_unref(config_locations);
        return config_files;
}

FILE *fopen_conf(char * const path)
{
        FILE *f = NULL;
        char *real_path = string_to_path(g_strdup(path));

        if (is_readable_file(real_path) && NULL != (f = fopen(real_path, "r")))
                LOG_I(MSG_FOPEN_SUCCESS(path, f));
        else
                LOG_W(MSG_FOPEN_FAILURE(path));

        g_free(real_path);
        return f;
}

void check_and_correct_settings(struct settings *s) {
        bool on_wayland = is_running_wayland();

#ifndef ENABLE_WAYLAND
        if (on_wayland) {
                /* We are using xwayland now. Setting force_xwayland to make sure
                 * the idle workaround below is activated */
                settings.force_xwayland = true;
        }
#endif

        if (settings.force_xwayland && on_wayland) {
                if (settings.idle_threshold > 0)
                        LOG_W("Using xwayland. Disabling idle.");
                /* There is no way to detect if the user is idle
                 * on xwayland, so turn this feature off */
                settings.idle_threshold = 0;
        }

        // check sanity of the progress bar options
        {
                if (s->progress_bar_height < (2 * s->progress_bar_frame_width)) {
                        DIE("setting progress_bar_frame_width is bigger than half of progress_bar_height");
                }
                if (s->progress_bar_max_width < (2 * s->progress_bar_frame_width)) {
                        DIE("setting progress_bar_frame_width is bigger than half of progress_bar_max_width");
                }
                if (s->progress_bar_max_width < s->progress_bar_min_width) {
                        DIE("setting progress_bar_max_width is smaller than progress_bar_min_width");
                }
                if (s->progress_bar_min_width > s->width.max) {
                        LOG_W("Progress bar min width is greater than the max width of the notification");
                }
                int progress_bar_max_corner_radius = (s->progress_bar_height / 2);
                if (s->progress_bar_corner_radius > progress_bar_max_corner_radius) {
                        settings.progress_bar_corner_radius = progress_bar_max_corner_radius;
                        LOG_W("Progress bar corner radius clamped to half of progress bar height (%i)",
                                progress_bar_max_corner_radius);
                }
        }

        // check lengths
        if (s->width.min == INT_MIN) {
                s->width.min = 0;
        }
        if (s->width.min < 0 || s->width.max < 0) {
                DIE("setting width does not support negative values");
        }
        if (s->width.min > s->width.max) {
                DIE("setting width min (%i) is always greather than max (%i)", s->width.min, s->width.max);
        }

        if (s->height.min == INT_MIN) {
                s->height.min = 0;
        }
        if (s->height.min < 0 || s->height.max < 0) {
                DIE("setting height does not support negative values");
        }
        if (s->height.min > s->height.max) {
                DIE("setting height min (%i) is always greather than max (%i)", s->height.min, s->height.max);
        }

        if (s->offset.x == INT_MIN || s->offset.y == INT_MAX) {
                DIE("setting offset needs both horizontal and vertical values");
        }

        // TODO Implement this with icon sizes as rules

        // restrict the icon size to a reasonable limit if we have a fixed width.
        // Otherwise the layout will be broken by too large icons.
        // See https://github.com/dunst-project/dunst/issues/540
        // if (s->width.max > 0) {
        //         const int icon_size_limit = s->width.max / 2;
        //         if (   s->max_icon_size == 0
        //             || s->max_icon_size > icon_size_limit) {
        //                 if (s->max_icon_size != 0) {
        //                         LOG_W("Max width was set to %d but got a max_icon_size of %d, too large to use. Setting max_icon_size=%d",
        //                                 s->width.max, s->max_icon_size, icon_size_limit);
        //                 } else {
        //                         LOG_I("Max width was set but max_icon_size is unlimited. Limiting icons to %d pixels", icon_size_limit);
        //                 }

        //                 s->max_icon_size = icon_size_limit;
        //         }
        // }

        // int text_icon_padding = settings.text_icon_padding != 0 ? settings.text_icon_padding : settings.h_padding;
        // int max_text_width = settings.width.max - settings.max_icon_size - text_icon_padding - 2 * settings.h_padding;
        // if (max_text_width < 10) {
        //         DIE("max_icon_size and horizontal padding are too large for the given width");
        // }

}

static void process_conf_file(const gpointer conf_fname, gpointer n_success) {
        const gchar * const p = conf_fname;

        LOG_D("Reading config file '%s'", p);
        /* Check for "-" here, so the file handling stays in one place */
        FILE *f = STR_EQ(p, "-") ? stdin : fopen_verbose(p);
        if (!f)
                return;

        struct ini *ini = load_ini_file(f);
        fclose(f);

        LOG_D("Loading settings");
        save_settings(ini);

        LOG_D("Checking/correcting settings");
        check_and_correct_settings(&settings);

        finish_ini(ini);
        g_free(ini);

        ++(*(int *) n_success);
}

void load_settings(char **const paths)
{
        LOG_D("Setting defaults");
        set_defaults();

        guint length = g_strv_length(paths);

        GPtrArray *conf_files;

        if (length != 0) {
                conf_files = g_ptr_array_new_full(length, g_free);
                for (int i = 0; paths[i]; i++)
                        g_ptr_array_add(conf_files, g_strdup(paths[i]));
        } else {
                // Use default locations (and search drop-ins)
                conf_files = get_conf_files();
        }

        /* Load all conf files and drop-ins, least important first. */
        int n_loaded_confs = 0;
        g_ptr_array_foreach(conf_files, process_conf_file, &n_loaded_confs);

        if (0 == n_loaded_confs)
                LOG_M("No configuration file found, using defaults");

        g_ptr_array_unref(conf_files);
}

void settings_free(struct settings *s)
{
        gradient_release(s->colors_low.highlight);
        gradient_release(s->colors_norm.highlight);
        gradient_release(s->colors_crit.highlight);

        g_free(s->font);
        g_free(s->format);
        g_free(s->icons[0]);
        g_free(s->icons[1]);
        g_free(s->icons[2]);
        g_free(s->title);
        g_free(s->class);
        g_free(s->monitor);
        g_free(s->dmenu);
        g_strfreev(s->dmenu_cmd);
        g_free(s->browser);
        g_strfreev(s->browser_cmd);
        g_strfreev(s->icon_theme);
        g_free(s->icon_path);

        g_free(s->mouse_left_click);
        g_free(s->mouse_middle_click);
        g_free(s->mouse_right_click);

        g_free(s->close_ks.str);
        g_free(s->close_all_ks.str);
        g_free(s->history_ks.str);
        g_free(s->context_ks.str);
}

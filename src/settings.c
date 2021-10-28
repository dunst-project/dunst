/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

/** @file src/settings.c
 * @brief Take care of the settings.
 */

#include "settings.h"

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
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

#ifndef SYSCONFDIR
/** @brief Fallback for doxygen, mostly.
 *
 * Since this gets defined by $DEFAULT_CPPFLAGS at compile time doxygen seems to
 * miss the correct value.
 */
#define SYSCONFDIR "/usr/local/etc/xdg"
#endif

/** @brief Alternative default for XDG_CONFIG_DIRS
 *
 * Fix peculiar behaviour if installed to a local PREFIX, i.e.
 * /usr/local, when SYSCONFDIR should be /usr/local/etc/xdg and not
 * /etc/xdg, hence use SYSCONFDIR (defined at compile time, see
 * config.mk) as default for XDG_CONFIG_DIRS. The spec says 'should'
 * and not 'must' use /etc/xdg.  Users/admins can override this by
 * explicitly setting XDG_CONFIG_DIRS to their liking at runtime or by
 * setting SYSCONFDIR=/etc/xdg at compile time.
 */
#define XDG_CONFIG_DIRS_DEFAULT SYSCONFDIR

/** @brief Generate path to base config 'dunstrc' in a base directory
 * 
 * @returns a newly-allocated gchar* string that must be freed with g_free().
 */
#define BASE_RC(basedir) g_build_filename(basedir, "dunst", "dunstrc", NULL)

/** @brief Generate drop-in directory path for a base directory
 * 
 * @returns a newly-allocated gchar* string that must be freed with g_free().
 */
#define DROP_IN_DIR(basedir) g_strconcat(BASE_RC(basedir), ".d", NULL)

/** @brief Match pattern for drop-in file names */
#define DROP_IN_PATTERN "*.conf"

struct settings settings;

static const char * const *get_xdg_conf_basedirs(void);

static int is_drop_in(const struct dirent *dent);

static void get_conf_files(GQueue *config_files);

/** @brief Filter for scandir().
 *
 * @returns @brief An integer indicating success
 *
 * @retval @brief 1 if file name matches #DROP_IN_PATTERN
 * @retval @brief 0 otherwise
 *
 * @param dent [in] @brief directory entry
 */
static int is_drop_in(const struct dirent *dent) {
        return 0 == fnmatch(DROP_IN_PATTERN, dent->d_name, FNM_PATHNAME | FNM_PERIOD)
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
static const char * const *get_xdg_conf_basedirs() {
        static const char * const *xdg_bd_arr = NULL;
        if (!xdg_bd_arr) {
                char * xdg_basedirs;

                const char * const xcd_env = getenv("XDG_CONFIG_DIRS");
                const char * const xdg_config_dirs = xcd_env && strnlen(xcd_env, 1)
                                                     ? xcd_env
                                                     : XDG_CONFIG_DIRS_DEFAULT;

                /*
                 * Prepend XDG_CONFIG_HOME, most important first because
                 * XDG_CONFIG_DIRS is already ordered that way.
                 */
                xdg_basedirs = g_strconcat(g_get_user_config_dir(),
                                           ":",
                                           xdg_config_dirs,
                                           NULL);
                LOG_D("Config directories: '%s'", xdg_basedirs);

                xdg_bd_arr = (const char * const *) string_to_array(xdg_basedirs, ":");
                g_free(xdg_basedirs);
        }
        return xdg_bd_arr;
}

/** @brief Find all config files.
 *
 * Searches all XDG config base directories for config files and drop-ins and
 * puts them in a GQueue, @e least important last.
 *
 * @param config_files [in|out] A pointer to a GQueue of gchar* strings
 * representing config file paths
 *
 * Use g_queue_pop_tail() to retrieve paths in @e ascending order of
 * importance, or use g_queue_reverse() before iterating over it with
 * g_queue_for_each().
 *
 * Use g_free() to free the retrieved elements of the queue.
 *
 * Use g_queue_free() to free after emptying it or g_queue_free_full() for a
 * non-empty queue.
 */
static void get_conf_files(GQueue *config_files) {
        struct dirent **drop_ins;
        for (const char * const *d = get_xdg_conf_basedirs(); *d; d++) {
                /* absolute path to the base rc-file */
                gchar * const base_rc = BASE_RC(*d);
                /* absolute path to the corresponding drop-in directory */
                gchar * const drop_in_dir = DROP_IN_DIR(*d);

                int n = scandir(drop_in_dir, &drop_ins, is_drop_in, alphasort);
                /* reverse order to get most important first */
                while (n-- > 0) {
                        gchar * const drop_in = g_strconcat(drop_in_dir,
                                                            "/",
                                                            drop_ins[n]->d_name,
                                                            NULL);
                        free(drop_ins[n]);

                        if (is_readable_file(drop_in)) {
                                LOG_D("Adding drop-in '%s'", drop_in);
                                g_queue_push_tail(config_files, drop_in);
                        } else
                                g_free(drop_in);
                }
                g_free(drop_in_dir);

                /* base rc-file last, least important */
                if (is_readable_file(base_rc)) {
                        LOG_D("Adding base config '%s'", base_rc);
                        g_queue_push_tail(config_files, base_rc);
                } else
                        g_free(base_rc);
        }
        free(drop_ins);
}

FILE *fopen_conf(char * const path)
{
        FILE *f = NULL;
        char *real_path = string_to_path(strdup(path));

        if (is_readable_file(real_path) && (f = fopen(real_path, "r")))
                LOG_I(MSG_FOPEN_SUCCESS(path, f));
        else
                LOG_W(MSG_FOPEN_FAILURE(path));

        free(real_path);
        return f;
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

static void process_conf_file(const gpointer conf_fname, gpointer n_success) {
        const gchar * const p = conf_fname;

        LOG_D("Trying to open '%s'", p);
        /* Check for "-" here, so the file handling stays in one place */
        FILE *f = STR_EQ(p, "-") ? stdin : fopen_verbose(p);
        if (!f)
                return;

        LOG_I("Parsing config, fd: '%d'", fileno(f));
        struct ini *ini = load_ini_file(f);
        LOG_D("Closing config, fd: '%d'", fileno(f));
        fclose(f);

        LOG_D("Loading settings");
        save_settings(ini);

        LOG_D("Checking/correcting settings");
        check_and_correct_settings(&settings);

        finish_ini(ini);
        free(ini);

        ++(*(int *) n_success);
}

void load_settings(const char * const path) {
        settings_init();
        LOG_D("Setting defaults");
        set_defaults();

        GQueue *conf_files = g_queue_new();

        if (path) /** If @p path [in] was supplied it will be the only one tried. */
                g_queue_push_tail(conf_files, g_strdup(path));
        else
                get_conf_files(conf_files);

        /* Load all conf files and drop-ins, least important first. */
        g_queue_reverse(conf_files); /* so we can iterate from least to most important */
        int n_loaded_confs = 0;
        g_queue_foreach(conf_files, process_conf_file, &n_loaded_confs);
        g_queue_free_full(conf_files, g_free);

        if (0 == n_loaded_confs)
                LOG_W("No configuration file, using defaults");

        for (GSList *iter = rules; iter; iter = iter->next) {
                struct rule *r = iter->data;
                print_rule(r);
        }
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

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

#include "../config.h"

struct settings settings;

static enum urgency ini_get_urgency(const char *section, const char *key, const enum urgency def)
{
        enum urgency ret;
        char *c = ini_get_string(section, key, NULL);

        if (!string_parse_urgency(c, &ret)) {
                if (c)
                        LOG_W("Unknown urgency: '%s'", c);
                ret = def;
        }

        g_free(c);
        return ret;
}

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

void print_command(char **cmd, char *name) {
        for (int i = 0; cmd[i] != NULL; i++) {
                LOG_D("%s %i: %s", name, i, cmd[i]);
        }
}

void print_mouse_list(enum mouse_action *mouse, char *name) {
        for (int i = 0; mouse[i] != -1; i++) {
                LOG_D("%s %i: %i", name, i, mouse[i]);
        }
}

void print_notification_colors(struct notification_colors c, char* name) {
        LOG_D ("Color %s", name);
        LOG_D("%s, %s, %s, %s", c.fg, c.bg, c.frame, c.highlight);
}

void dump_settings(struct settings s){
        LOG_D("print_notifications: %i", s.print_notifications);
        LOG_D("per_monitor_dpi: %i", s.per_monitor_dpi);
        LOG_D("stack_duplicates: %i", s.stack_duplicates);
        LOG_D("hide_duplicate_count: %i", s.hide_duplicate_count);
        LOG_D("always_run_script: %i", s.always_run_script);
        LOG_D("force_xinerama: %i", s.force_xinerama);
        LOG_D("force_xwayland: %i", s.force_xwayland);
        LOG_D("progress_bar: %i", s.progress_bar);
        LOG_D("font %s", s.font);
        LOG_D("format: %s",s.format);
        LOG_D("icon_path: %s",s.icon_path);
        LOG_D("title: %s",s.title);
        LOG_D("class: %s",s.class);
        LOG_D("dmenu: %s",s.dmenu);
        LOG_D("frame_color: %s",s.frame_color);
        LOG_D("icons 1: %s", s.icons[0]);
        LOG_D("icons 2: %s", s.icons[1]);
        LOG_D("icons 3: %s", s.icons[2]);
        LOG_D("timeouts 1: %li", s.timeouts[0]);
        LOG_D("timeouts 2: %li", s.timeouts[1]);
        LOG_D("timeouts 3: %li", s.timeouts[2]);

        print_command(s.dmenu_cmd, "dmenu");
        print_command(s.browser_cmd, "browser");
        print_mouse_list(s.mouse_left_click, "left click");
        print_mouse_list(s.mouse_middle_click, "middle click");
        print_mouse_list(s.mouse_right_click, "right click");

        LOG_D("icon_position: %i", s.icon_position);
        LOG_D("vertical_alignment: %i", s.vertical_alignment);
        LOG_D("follow_mode: %i", s.f_mode);
        LOG_D("markup_mode: %i", s.markup);
        LOG_D("alignment: %i", s.align);
        LOG_D("ellipsize: %i", s.ellipsize);
        LOG_D("layer: %i", s.layer);

        print_notification_colors(s.colors_low, "low");
        print_notification_colors(s.colors_norm, "norm");
        print_notification_colors(s.colors_crit, "crit");
        LOG_D("sep color: %s, type %i", s.sep_color.sep_color, s.sep_color.type);

        /* settings that don't get printed below */
        /* struct geometry geometry; */
        /* gint64 idle_threshold; */
        /* gint64 show_age_threshold; */
        /* unsigned int transparency; */

        /* struct keyboard_shortcut close_ks; */
        /* struct keyboard_shortcut close_all_ks; */
        /* struct keyboard_shortcut history_ks; */
        /* struct keyboard_shortcut context_ks; */
        /* int shrink; */
        /* int sort; */
        /* int indicate_hidden; */
        /* int sticky_history; */
        /* int history_length; */
        /* int show_indicators; */
        /* int word_wrap; */
        /* int ignore_dbusclose; */
        /* int ignore_newline; */
        /* int line_height; */
        /* int notification_height; */
        /* int separator_height; */
        /* int padding; */
        /* int h_padding; */
        /* int text_icon_padding; */
        /* int frame_width; */
        /* int startup_notification; */
        /* int monitor; */
        /* int min_icon_size; */
        /* int max_icon_size; */
        /* int corner_radius; */
        /* int progress_bar_height; */
        /* int progress_bar_min_width; */
        /* int progress_bar_max_width; */
        /* int progress_bar_frame_width; */
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
        printf("\n");
        LOG_I("### Setting defaults");
        set_defaults();
        printf("\n");
        LOG_I("### Setting from config");
        save_settings();
        printf("\n");
        LOG_I("### Settings dump");
        dump_settings(settings);

        check_and_correct_settings(&settings);

        /* push hardcoded default rules into rules list */
        for (int i = 0; i < G_N_ELEMENTS(default_rules); i++) {
                rules = g_slist_insert(rules, &(default_rules[i]), -1);
        }

        const char *cur_section = NULL;
        for (;;) {
                cur_section = next_section(cur_section);
                if (!cur_section)
                        break;
                if (STR_EQ(cur_section, "global")
                    || STR_EQ(cur_section, "frame")
                    || STR_EQ(cur_section, "experimental")
                    || STR_EQ(cur_section, "shortcuts")
                    || STR_EQ(cur_section, "urgency_low")
                    || STR_EQ(cur_section, "urgency_normal")
                    || STR_EQ(cur_section, "urgency_critical"))
                        continue;

                /* check for existing rule with same name */
                struct rule *r = NULL;
                for (GSList *iter = rules; iter; iter = iter->next) {
                        struct rule *match = iter->data;
                        if (match->name &&
                            STR_EQ(match->name, cur_section))
                                r = match;
                }

                if (!r) {
                        r = rule_new();
                        rules = g_slist_insert(rules, r, -1);
                }

                r->name = g_strdup(cur_section);
                r->appname = ini_get_string(cur_section, "appname", r->appname);
                r->summary = ini_get_string(cur_section, "summary", r->summary);
                r->body = ini_get_string(cur_section, "body", r->body);
                r->icon = ini_get_string(cur_section, "icon", r->icon);
                r->category = ini_get_string(cur_section, "category", r->category);
                r->stack_tag = ini_get_string(cur_section, "stack_tag", r->stack_tag);
                r->timeout = ini_get_time(cur_section, "timeout", r->timeout);

                {
                        char *c = ini_get_string(
                                cur_section,
                                "markup", NULL
                        );

                        if (!string_parse_markup_mode(c, &r->markup)) {
                                if (c)
                                        LOG_W("Invalid markup mode value: %s", c);
                        }
                        g_free(c);
                }

                r->action_name = ini_get_string(cur_section, "action_name", NULL);
                r->urgency = ini_get_urgency(cur_section, "urgency", r->urgency);
                r->msg_urgency = ini_get_urgency(cur_section, "msg_urgency", r->msg_urgency);
                r->fg = ini_get_string(cur_section, "foreground", r->fg);
                r->bg = ini_get_string(cur_section, "background", r->bg);
                r->highlight = ini_get_string(cur_section, "highlight", r->highlight);
                r->fc = ini_get_string(cur_section, "frame_color", r->fc);
                r->format = ini_get_string(cur_section, "format", r->format);
                r->new_icon = ini_get_string(cur_section, "new_icon", r->new_icon);
                r->history_ignore = ini_get_bool(cur_section, "history_ignore", r->history_ignore);
                r->match_transient = ini_get_bool(cur_section, "match_transient", r->match_transient);
                r->set_transient = ini_get_bool(cur_section, "set_transient", r->set_transient);
                r->desktop_entry = ini_get_string(cur_section, "desktop_entry", r->desktop_entry);
                r->skip_display = ini_get_bool(cur_section, "skip_display", r->skip_display);
                {
                        char *c = ini_get_string(
                                cur_section,
                                "fullscreen", NULL
                        );

                        if (!string_parse_fullscreen(c, &r->fullscreen)) {
                                if (c)
                                        LOG_W("Invalid fullscreen value: %s", c);
                        }
                        g_free(c);
                }
                r->script = ini_get_path(cur_section, "script", NULL);
                r->set_stack_tag = ini_get_string(cur_section, "set_stack_tag", r->set_stack_tag);
        }

        if (config_file) {
                fclose(config_file);
                free_ini();
        }
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

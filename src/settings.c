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

        return f;
}

void load_settings(char *cmdline_config_path)
{

#ifndef STATIC_CONFIG
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
#else
        LOG_M("dunstrc parsing disabled. "
              "Using STATIC_CONFIG is deprecated behavior.");
#endif

        {
                char *loglevel = option_get_string(
                                "global",
                                "verbosity", "-verbosity", NULL,
                                "The verbosity to log (one of 'crit', 'warn', 'mesg', 'info', 'debug')"
                        );

                log_set_level_from_string(loglevel);

                g_free(loglevel);
        }

        settings.per_monitor_dpi = option_get_bool(
                "experimental",
                "per_monitor_dpi", NULL, false,
                ""
        );

        settings.force_xinerama = option_get_bool(
                "global",
                "force_xinerama", "-force_xinerama", false,
                "Force the use of the Xinerama extension"
        );

        settings.font = option_get_string(
                "global",
                "font", "-font/-fn", defaults.font,
                "The font dunst should use."
        );

        {
                // Check if allow_markup set
                if (ini_is_set("global", "allow_markup")) {
                        bool allow_markup = option_get_bool(
                                "global",
                                "allow_markup", NULL, false,
                                "Allow markup in notifications"
                        );

                        settings.markup = (allow_markup ? MARKUP_FULL : MARKUP_STRIP);
                        LOG_M("'allow_markup' is deprecated, please "
                              "use 'markup' instead.");
                }

                char *c = option_get_string(
                        "global",
                        "markup", "-markup", NULL,
                        "Specify how markup should be handled"
                );

                if (!string_parse_markup_mode(c, &settings.markup)) {
                        if (c)
                                LOG_W("Cannot parse markup mode value: '%s'", c);
                        if (!settings.markup)
                                settings.markup = defaults.markup;
                }
                g_free(c);
        }

        settings.format = option_get_string(
                "global",
                "format", "-format", defaults.format,
                "The format template for the notifications"
        );

        settings.sort = option_get_bool(
                "global",
                "sort", "-sort", defaults.sort,
                "Sort notifications by urgency and date?"
        );

        settings.indicate_hidden = option_get_bool(
                "global",
                "indicate_hidden", "-indicate_hidden", defaults.indicate_hidden,
                "Show how many notifications are hidden"
        );

        settings.word_wrap = option_get_bool(
                "global",
                "word_wrap", "-word_wrap", defaults.word_wrap,
                "Truncating long lines or do word wrap"
        );

        {
                char *c = option_get_string(
                        "global",
                        "ellipsize", "-ellipsize", NULL,
                        "Ellipsize truncated lines on the start/middle/end"
                );

                if (!string_parse_ellipsize(c, &settings.ellipsize)) {
                        if (c)
                                LOG_W("Unknown ellipsize value: '%s'", c);
                        settings.ellipsize = defaults.ellipsize;
                }
                g_free(c);
        }

        settings.ignore_newline = option_get_bool(
                "global",
                "ignore_newline", "-ignore_newline", defaults.ignore_newline,
                "Ignore newline characters in notifications"
        );

        settings.idle_threshold = option_get_time(
                "global",
                "idle_threshold", "-idle_threshold", defaults.idle_threshold,
                "Don't timeout notifications if user is longer idle than threshold"
        );

        settings.monitor = option_get_int(
                "global",
                "monitor", "-mon/-monitor", defaults.monitor,
                "On which monitor should the notifications be displayed"
        );

        {
                char *c = option_get_string(
                        "global",
                        "follow", "-follow", NULL,
                        "Follow mouse, keyboard or none?"
                );

                if (!string_parse_follow_mode(c, &settings.f_mode)) {
                        if (c)
                                LOG_W("Cannot parse follow mode: %s", c);
                        settings.f_mode = defaults.f_mode;
                }
                g_free(c);
        }

        settings.title = option_get_string(
                "global",
                "title", "-t/-title", defaults.title,
                "Define the title of windows spawned by dunst."
        );

        settings.class = option_get_string(
                "global",
                "class", "-c/-class", defaults.class,
                "Define the class of windows spawned by dunst."
        );

        {

                char *c = option_get_string(
                        "global",
                        "geometry", "-geom/-geometry", NULL,
                        "Geometry for the window"
                );

                if (c) {
                        // TODO: Implement own geometry parsing to get rid of
                        //       the include dependency on X11
                        settings.geometry = x_parse_geometry(c);
                        g_free(c);
                } else {
                        settings.geometry = defaults.geometry;
                }

        }

        settings.shrink = option_get_bool(
                "global",
                "shrink", "-shrink", defaults.shrink,
                "Shrink window if it's smaller than the width"
        );

        settings.line_height = option_get_int(
                "global",
                "line_height", "-lh/-line_height", defaults.line_height,
                "Add spacing between lines of text"
        );

        settings.notification_height = option_get_int(
                "global",
                "notification_height", "-nh/-notification_height", defaults.notification_height,
                "Define height of the window"
        );

        {
                char *c = option_get_string(
                        "global",
                        "alignment", "-align/-alignment", NULL,
                        "Text alignment left/center/right"
                );

                if (!string_parse_alignment(c, &settings.align)) {
                        if (c)
                                LOG_W("Unknown alignment value: '%s'", c);
                        settings.align = defaults.align;
                }

                g_free(c);
        }

        settings.show_age_threshold = option_get_time(
                "global",
                "show_age_threshold", "-show_age_threshold", defaults.show_age_threshold,
                "When should the age of the notification be displayed?"
        );

        settings.hide_duplicate_count = option_get_bool(
                "global",
                "hide_duplicate_count", "-hide_duplicate_count", false,
                "Hide the count of stacked notifications with the same content"
        );

        settings.sticky_history = option_get_bool(
                "global",
                "sticky_history", "-sticky_history", defaults.sticky_history,
                "Don't timeout notifications popped up from history"
        );

        settings.history_length = option_get_int(
                "global",
                "history_length", "-history_length", defaults.history_length,
                "Max amount of notifications kept in history"
        );

        settings.show_indicators = option_get_bool(
                "global",
                "show_indicators", "-show_indicators", defaults.show_indicators,
                "Show indicators for actions \"(A)\" and URLs \"(U)\""
        );

        settings.separator_height = option_get_int(
                "global",
                "separator_height", "-sep_height/-separator_height", defaults.separator_height,
                "height of the separator line"
        );

        settings.padding = option_get_int(
                "global",
                "padding", "-padding", defaults.padding,
                "Padding between text and separator"
        );

        settings.h_padding = option_get_int(
                "global",
                "horizontal_padding", "-horizontal_padding", defaults.h_padding,
                "horizontal padding"
        );

        settings.transparency = option_get_int(
                "global",
                "transparency", "-transparency", defaults.transparency,
                "Transparency. Range 0-100"
        );

        settings.corner_radius = option_get_int(
                "global",
                "corner_radius", "-corner_radius", defaults.corner_radius,
                "Window corner radius"
        );

        {
                char *c = option_get_string(
                        "global",
                        "separator_color", "-sep_color/-separator_color", "",
                        "Color of the separator line (or 'auto')"
                );

                if (!string_parse_sepcolor(c, &settings.sep_color)) {
                        settings.sep_color = defaults.sep_color;
                }
                g_free(c);
        }

        settings.stack_duplicates = option_get_bool(
                "global",
                "stack_duplicates", "-stack_duplicates", true,
                "Stack together notifications with the same content"
        );

        settings.startup_notification = option_get_bool(
                "global",
                "startup_notification", "-startup_notification", false,
                "print notification on startup"
        );

        settings.dmenu = option_get_path(
                "global",
                "dmenu", "-dmenu", defaults.dmenu,
                "path to dmenu"
        );

        {
                GError *error = NULL;
                if (!g_shell_parse_argv(settings.dmenu, NULL, &settings.dmenu_cmd, &error)) {
                        LOG_W("Unable to parse dmenu command: '%s'."
                              "dmenu functionality will be disabled.", error->message);
                        g_error_free(error);
                        settings.dmenu_cmd = NULL;
                }
        }


        settings.browser = option_get_path(
                "global",
                "browser", "-browser", defaults.browser,
                "path to browser"
        );

        {
                GError *error = NULL;
                if (!g_shell_parse_argv(settings.browser, NULL, &settings.browser_cmd, &error)) {
                        LOG_W("Unable to parse browser command: '%s'."
                              " URL functionality will be disabled.", error->message);
                        g_error_free(error);
                        settings.browser_cmd = NULL;
                }
        }

        {
                char *c = option_get_string(
                        "global",
                        "icon_position", "-icon_position", "off",
                        "Align icons left/right/off"
                );

                if (!string_parse_icon_position(c, &settings.icon_position)) {
                        if (c)
                                LOG_W("Unknown icon position: '%s'", c);
                        settings.icon_position = defaults.icon_position;
                }
                g_free(c);
        }

        {
                char *c = option_get_string(
                        "global",
                        "vertical_alignment", "-vertical_alignment", "center",
                        "Align icon and text top/center/bottom"
                );
                if (!string_parse_vertical_alignment(c, &settings.vertical_alignment)) {
                        if (c)
                                LOG_W("Unknown vertical alignment: '%s'", c);
                        settings.vertical_alignment = defaults.vertical_alignment;
                }
                g_free(c);

        }

        settings.min_icon_size = option_get_int(
                "global",
                "min_icon_size", "-min_icon_size", defaults.min_icon_size,
                "Scale smaller icons up to this size, set to 0 to disable. If max_icon_size also specified, that has the final say."
        );

        settings.max_icon_size = option_get_int(
                "global",
                "max_icon_size", "-max_icon_size", defaults.max_icon_size,
                "Scale larger icons down to this size, set to 0 to disable"
        );

        // restrict the icon size to a reasonable limit if we have a fixed width.
        // Otherwise the layout will be broken by too large icons.
        // See https://github.com/dunst-project/dunst/issues/540
        if (settings.geometry.width_set && settings.geometry.w != 0) {
                const int icon_size_limit = settings.geometry.w / 2;
                if (   settings.max_icon_size == 0
                    || settings.max_icon_size > icon_size_limit) {
                        if (settings.max_icon_size != 0) {
                                LOG_W("Max width was set to %d but got a max_icon_size of %d, too large to use. Setting max_icon_size=%d",
                                        settings.geometry.w, settings.max_icon_size, icon_size_limit);
                        } else {
                                LOG_I("Max width was set but max_icon_size is unlimited. Limiting icons to %d pixels", icon_size_limit);
                        }

                        settings.max_icon_size = icon_size_limit;
                }
        }

        // If the deprecated icon_folders option is used,
        // read it and generate its usage string.
        if (ini_is_set("global", "icon_folders") || cmdline_is_set("-icon_folders")) {
                settings.icon_path = option_get_string(
                        "global",
                        "icon_folders", "-icon_folders", defaults.icon_path,
                        "folders to default icons (deprecated, please use 'icon_path' instead)"
                );
                LOG_M("The option 'icon_folders' is deprecated, please use 'icon_path' instead.");
        }
        // Read value and generate usage string for icon_path.
        // If icon_path is set, override icon_folder.
        // if not, but icon_folder is set, use that instead of the compile time default.
        settings.icon_path = option_get_string(
                "global",
                "icon_path", "-icon_path",
                settings.icon_path ? settings.icon_path : defaults.icon_path,
                "paths to default icons"
        );

        {
                // Backwards compatibility with the legacy 'frame' section.
                if (ini_is_set("frame", "width")) {
                        settings.frame_width = option_get_int(
                                "frame",
                                "width", NULL, defaults.frame_width,
                                "Width of frame around the window"
                        );
                        LOG_M("The frame section is deprecated, width has "
                              "been renamed to frame_width and moved to "
                              "the global section.");
                }

                settings.frame_width = option_get_int(
                        "global",
                        "frame_width", "-frame_width",
                        settings.frame_width ? settings.frame_width : defaults.frame_width,
                        "Width of frame around the window"
                );

                if (ini_is_set("frame", "color")) {
                        settings.frame_color = option_get_string(
                                "frame",
                                "color", NULL, defaults.frame_color,
                                "Color of the frame around the window"
                        );
                        LOG_M("The frame section is deprecated, color "
                              "has been renamed to frame_color and moved "
                              "to the global section.");
                }

                settings.frame_color = option_get_string(
                        "global",
                        "frame_color", "-frame_color",
                        settings.frame_color ? settings.frame_color : defaults.frame_color,
                        "Color of the frame around the window"
                );

        }

        {
                char *c = option_get_string(
                        "global",
                        "mouse_left_click", "-left_click", NULL,
                        "Action of Left click event"
                );

                if (!string_parse_mouse_action(c, &settings.mouse_left_click)) {
                        if (c)
                                LOG_W("Unknown mouse action value: '%s'", c);
                        settings.mouse_left_click = defaults.mouse_left_click;
                }
                g_free(c);
        }

        {
                char *c = option_get_string(
                        "global",
                        "mouse_middle_click", "-mouse_middle_click", NULL,
                        "Action of middle click event"
                );

                if (!string_parse_mouse_action(c, &settings.mouse_middle_click)) {
                        if (c)
                                LOG_W("Unknown mouse action value: '%s'", c);
                        settings.mouse_middle_click = defaults.mouse_middle_click;
                }
                g_free(c);
        }

        {
                char *c = option_get_string(
                        "global",
                        "mouse_right_click", "-mouse_right_click", NULL,
                        "Action of right click event"
                );

                if (!string_parse_mouse_action(c, &settings.mouse_right_click)) {
                        if (c)
                                LOG_W("Unknown mouse action value: '%s'", c);
                        settings.mouse_right_click = defaults.mouse_right_click;
                }
                g_free(c);
        }

        settings.colors_low.bg = option_get_string(
                "urgency_low",
                "background", "-lb", defaults.colors_low.bg,
                "Background color for notifications with low urgency"
        );

        settings.colors_low.fg = option_get_string(
                "urgency_low",
                "foreground", "-lf", defaults.colors_low.fg,
                "Foreground color for notifications with low urgency"
        );

        settings.colors_low.frame = option_get_string(
                "urgency_low",
                "frame_color", "-lfr", settings.frame_color ? settings.frame_color : defaults.colors_low.frame,
                "Frame color for notifications with low urgency"
        );

        settings.timeouts[URG_LOW] = option_get_time(
                "urgency_low",
                "timeout", "-lto", defaults.timeouts[URG_LOW],
                "Timeout for notifications with low urgency"
        );

        settings.icons[URG_LOW] = option_get_string(
                "urgency_low",
                "icon", "-li", defaults.icons[URG_LOW],
                "Icon for notifications with low urgency"
        );

        settings.colors_norm.bg = option_get_string(
                "urgency_normal",
                "background", "-nb", defaults.colors_norm.bg,
                "Background color for notifications with normal urgency"
        );

        settings.colors_norm.fg = option_get_string(
                "urgency_normal",
                "foreground", "-nf", defaults.colors_norm.fg,
                "Foreground color for notifications with normal urgency"
        );

        settings.colors_norm.frame = option_get_string(
                "urgency_normal",
                "frame_color", "-nfr", settings.frame_color ? settings.frame_color : defaults.colors_norm.frame,
                "Frame color for notifications with normal urgency"
        );

        settings.timeouts[URG_NORM] = option_get_time(
                "urgency_normal",
                "timeout", "-nto", defaults.timeouts[URG_NORM],
                "Timeout for notifications with normal urgency"
        );

        settings.icons[URG_NORM] = option_get_string(
                "urgency_normal",
                "icon", "-ni", defaults.icons[URG_NORM],
                "Icon for notifications with normal urgency"
        );

        settings.colors_crit.bg = option_get_string(
                "urgency_critical",
                "background", "-cb", defaults.colors_crit.bg,
                "Background color for notifications with critical urgency"
        );

        settings.colors_crit.fg = option_get_string(
                "urgency_critical",
                "foreground", "-cf", defaults.colors_crit.fg,
                "Foreground color for notifications with ciritical urgency"
        );

        settings.colors_crit.frame = option_get_string(
                "urgency_critical",
                "frame_color", "-cfr", settings.frame_color ? settings.frame_color : defaults.colors_crit.frame,
                "Frame color for notifications with critical urgency"
        );

        settings.timeouts[URG_CRIT] = option_get_time(
                "urgency_critical",
                "timeout", "-cto", defaults.timeouts[URG_CRIT],
                "Timeout for notifications with critical urgency"
        );

        settings.icons[URG_CRIT] = option_get_string(
                "urgency_critical",
                "icon", "-ci", defaults.icons[URG_CRIT],
                "Icon for notifications with critical urgency"
        );

        settings.close_ks.str = option_get_string(
                "shortcuts",
                "close", "-key", defaults.close_ks.str,
                "Shortcut for closing one notification"
        );

        settings.close_all_ks.str = option_get_string(
                "shortcuts",
                "close_all", "-all_key", defaults.close_all_ks.str,
                "Shortcut for closing all notifications"
        );

        settings.history_ks.str = option_get_string(
                "shortcuts",
                "history", "-history_key", defaults.history_ks.str,
                "Shortcut to pop the last notification from history"
        );

        settings.context_ks.str = option_get_string(
                "shortcuts",
                "context", "-context_key", defaults.context_ks.str,
                "Shortcut for context menu"
        );

        settings.print_notifications = cmdline_get_bool(
                "-print", false,
                "Print notifications to cmdline (DEBUG)"
        );

        settings.always_run_script = option_get_bool(
                "global",
                "always_run_script", "-always_run_script", true,
                "Always run rule-defined scripts, even if the notification is suppressed with format = \"\"."
        );

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

                r->urgency = ini_get_urgency(cur_section, "urgency", r->urgency);
                r->msg_urgency = ini_get_urgency(cur_section, "msg_urgency", r->msg_urgency);
                r->fg = ini_get_string(cur_section, "foreground", r->fg);
                r->bg = ini_get_string(cur_section, "background", r->bg);
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

#ifndef STATIC_CONFIG
        if (config_file) {
                fclose(config_file);
                free_ini();
        }
#endif
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "settings.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#ifndef STATIC_CONFIG
#include <basedir.h>
#include <basedir_fs.h>
#endif

#include "rules.h" // put before config.h to fix missing include
#include "config.h"
#include "dunst.h"
#include "log.h"
#include "notification.h"
#include "option_parser.h"
#include "utils.h"
#include "x11/x.h"

settings_t settings;

static void parse_follow_mode(const char *mode)
{
        if (strcmp(mode, "mouse") == 0)
                settings.f_mode = FOLLOW_MOUSE;
        else if (strcmp(mode, "keyboard") == 0)
                settings.f_mode = FOLLOW_KEYBOARD;
        else if (strcmp(mode, "none") == 0)
                settings.f_mode = FOLLOW_NONE;
        else {
                LOG_W("Unknown follow mode: '%s'", mode);
                settings.f_mode = FOLLOW_NONE;
        }
}

static enum markup_mode parse_markup_mode(const char *mode)
{
        if (strcmp(mode, "strip") == 0) {
                return MARKUP_STRIP;
        } else if (strcmp(mode, "no") == 0) {
                return MARKUP_NO;
        } else if (strcmp(mode, "full") == 0 || strcmp(mode, "yes") == 0) {
                return MARKUP_FULL;
        } else {
                LOG_W("Unknown markup mode: '%s'", mode);
                return MARKUP_NO;
        }
}

static enum mouse_action parse_mouse_action(const char *action)
{
        if (strcmp(action, "none") == 0)
                return MOUSE_NONE;
        else if (strcmp(action, "do_action") == 0)
                return MOUSE_DO_ACTION;
        else if (strcmp(action, "close_current") == 0)
                return MOUSE_CLOSE_CURRENT;
        else if (strcmp(action, "close_all") == 0)
                return MOUSE_CLOSE_ALL;
        else {
                LOG_W("Unknown mouse action: '%s'", action);
                return MOUSE_NONE;
        }
}


static enum urgency ini_get_urgency(const char *section, const char *key, const int def)
{
        int ret = def;
        char *urg = ini_get_string(section, key, "");

        if (strlen(urg) > 0) {
                if (strcmp(urg, "low") == 0)
                        ret = URG_LOW;
                else if (strcmp(urg, "normal") == 0)
                        ret = URG_NORM;
                else if (strcmp(urg, "critical") == 0)
                        ret = URG_CRIT;
                else
                        LOG_W("Unknown urgency: '%s'", urg);
        }
        g_free(urg);
        return ret;
}

void load_settings(char *cmdline_config_path)
{

#ifndef STATIC_CONFIG
        xdgHandle xdg;
        FILE *config_file = NULL;

        xdgInitHandle(&xdg);

        if (cmdline_config_path) {
                if (0 == strcmp(cmdline_config_path, "-")) {
                        config_file = stdin;
                } else {
                        config_file = fopen(cmdline_config_path, "r");
                }

                if(!config_file) {
                        DIE("Cannot find config file: '%s'", cmdline_config_path);
                }
        }
        if (!config_file) {
                config_file = xdgConfigOpen("dunst/dunstrc", "r", &xdg);
        }
        if (!config_file) {
                /* Fall back to just "dunstrc", which was used before 2013-06-23
                 * (before v0.2). */
                config_file = xdgConfigOpen("dunstrc", "r", &xdg);
                if (!config_file) {
                        LOG_W("No dunstrc found.");
                        xdgWipeHandle(&xdg);
                }
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

                //Use markup if set
                //Use default if settings.markup not set yet
                //  (=>c empty&&!allow_markup)
                if (c) {
                        settings.markup = parse_markup_mode(c);
                } else if (!settings.markup) {
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
                        "ellipsize", "-ellipsize", "",
                        "Ellipsize truncated lines on the start/middle/end"
                );

                if (strlen(c) == 0) {
                        settings.ellipsize = defaults.ellipsize;
                } else if (strcmp(c, "start") == 0) {
                        settings.ellipsize = start;
                } else if (strcmp(c, "middle") == 0) {
                        settings.ellipsize = middle;
                } else if (strcmp(c, "end") == 0) {
                        settings.ellipsize = end;
                } else {
                        LOG_W("Unknown ellipsize value: '%s'", c);
                        settings.ellipsize = defaults.ellipsize;
                }
                g_free(c);
        }

        settings.log_path = option_get_string(
                "global",
                "log_path", "-log_path", defaults.log_path,
                "Log file location"
        );

        char *home = getenv("HOME");
        if (g_str_has_prefix(settings.log_path, "~/")) {
            settings.log_path = g_strconcat(home, settings.log_path + 1, NULL);
        }

        settings.log_notifications = option_get_bool(
                "global",
                "log_notifications", "-log_notifications", defaults.log_notifications,
                "Log all notifications and save them in a file"
        );

        settings.log_format = option_get_string(
                "global",
                "log_format", "-log_format", defaults.log_format,
                "How the log should be formatted"
        );

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
                        "follow", "-follow", "",
                        "Follow mouse, keyboard or none?"
                );

                if (strlen(c) > 0) {
                        parse_follow_mode(c);
                        g_free(c);
                }
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
                        "alignment", "-align/-alignment", "",
                        "Text alignment left/center/right"
                );

                if (strlen(c) > 0) {
                        if (strcmp(c, "left") == 0)
                                settings.align = left;
                        else if (strcmp(c, "center") == 0)
                                settings.align = center;
                        else if (strcmp(c, "right") == 0)
                                settings.align = right;
                        else
                                LOG_W("Unknown alignment value: '%s'", c);
                        g_free(c);
                }
        }

        settings.show_age_threshold = option_get_time(
                "global",
                "show_age_threshold", "-show_age_threshold", defaults.show_age_threshold,
                "When should the age of the notification be displayed?"
        );

        settings.hide_duplicate_count = option_get_bool(
                "global",
                "hide_duplicate_count", "-hide_duplicate_count", false,
                "Hide the count of merged notifications with the same content"
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

                if (strlen(c) > 0) {
                        if (strcmp(c, "auto") == 0)
                                settings.sep_color = SEP_AUTO;
                        else if (strcmp(c, "foreground") == 0)
                                settings.sep_color = SEP_FOREGROUND;
                        else if (strcmp(c, "frame") == 0)
                                settings.sep_color = SEP_FRAME;
                        else {
                                settings.sep_color = SEP_CUSTOM;
                                settings.sep_custom_color_str = g_strdup(c);
                        }
                }
                g_free(c);
        }

        settings.stack_duplicates = option_get_bool(
                "global",
                "stack_duplicates", "-stack_duplicates", true,
                "Merge multiple notifications with the same content"
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
                char *c = option_get_string(
                        "global",
                        "icon_position", "-icon_position", "off",
                        "Align icons left/right/off"
                );

                if (strlen(c) > 0) {
                        if (strcmp(c, "left") == 0)
                                settings.icon_position = icons_left;
                        else if (strcmp(c, "right") == 0)
                                settings.icon_position = icons_right;
                        else if (strcmp(c, "off") == 0)
                                settings.icon_position = icons_off;
                        else
                                LOG_W("Unknown icon position: '%s'", c);
                        g_free(c);
                }
        }

        settings.max_icon_size = option_get_int(
                "global",
                "max_icon_size", "-max_icon_size", defaults.max_icon_size,
                "Scale larger icons down to this size, set to 0 to disable"
        );

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

                if (c) {
                        settings.mouse_left_click = parse_mouse_action(c);
                } else {
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

                if (c) {
                        settings.mouse_middle_click = parse_mouse_action(c);
                } else {
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

                if (c) {
                        settings.mouse_right_click = parse_mouse_action(c);
                } else {
                        settings.mouse_right_click = defaults.mouse_right_click;
                }

                g_free(c);
        }

        settings.lowbgcolor = option_get_string(
                "urgency_low",
                "background", "-lb", defaults.lowbgcolor,
                "Background color for notifications with low urgency"
        );

        settings.lowfgcolor = option_get_string(
                "urgency_low",
                "foreground", "-lf", defaults.lowfgcolor,
                "Foreground color for notifications with low urgency"
        );

        settings.lowframecolor = option_get_string(
                "urgency_low",
                "frame_color", "-lfr", NULL,
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

        settings.normbgcolor = option_get_string(
                "urgency_normal",
                "background", "-nb", defaults.normbgcolor,
                "Background color for notifications with normal urgency"
        );

        settings.normfgcolor = option_get_string(
                "urgency_normal",
                "foreground", "-nf", defaults.normfgcolor,
                "Foreground color for notifications with normal urgency"
        );

        settings.normframecolor = option_get_string(
                "urgency_normal",
                "frame_color", "-nfr", NULL,
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

        settings.critbgcolor = option_get_string(
                "urgency_critical",
                "background", "-cb", defaults.critbgcolor,
                "Background color for notifications with critical urgency"
        );

        settings.critfgcolor = option_get_string(
                "urgency_critical",
                "foreground", "-cf", defaults.critfgcolor,
                "Foreground color for notifications with critical urgency"
        );

        settings.critframecolor = option_get_string(
                "urgency_critical",
                "frame_color", "-cfr", NULL,
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
                if (strcmp(cur_section, "global") == 0
                    || strcmp(cur_section, "frame") == 0
                    || strcmp(cur_section, "experimental") == 0
                    || strcmp(cur_section, "shortcuts") == 0
                    || strcmp(cur_section, "urgency_low") == 0
                    || strcmp(cur_section, "urgency_normal") == 0
                    || strcmp(cur_section, "urgency_critical") == 0)
                        continue;

                /* check for existing rule with same name */
                rule_t *r = NULL;
                for (GSList *iter = rules; iter; iter = iter->next) {
                        rule_t *match = iter->data;
                        if (match->name &&
                            strcmp(match->name, cur_section) == 0)
                                r = match;
                }

                if (!r) {
                        r = g_malloc(sizeof(rule_t));
                        rule_init(r);
                        rules = g_slist_insert(rules, r, -1);
                }

                r->name = g_strdup(cur_section);
                r->appname = ini_get_string(cur_section, "appname", r->appname);
                r->summary = ini_get_string(cur_section, "summary", r->summary);
                r->body = ini_get_string(cur_section, "body", r->body);
                r->icon = ini_get_string(cur_section, "icon", r->icon);
                r->category = ini_get_string(cur_section, "category", r->category);
                r->timeout = ini_get_time(cur_section, "timeout", r->timeout);

                {
                        char *c = ini_get_string(
                                cur_section,
                                "markup", NULL
                        );

                        if (c) {
                                r->markup = parse_markup_mode(c);
                                g_free(c);
                        }
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
                {
                        char *c = ini_get_string(
                                cur_section,
                                "fullscreen", NULL
                        );

                        r->fullscreen = parse_enum_fullscreen(c, r->fullscreen);
                        g_free(c);
                }
                r->script = ini_get_path(cur_section, "script", NULL);
        }

#ifndef STATIC_CONFIG
        if (config_file) {
                fclose(config_file);
                free_ini();
                xdgWipeHandle(&xdg);
        }
#endif
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

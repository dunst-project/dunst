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

void load_settings(const char *cmdline_config_path)
{

#ifndef STATIC_CONFIG
        xdgHandle xdg;
        FILE *config_file = NULL;

        xdgInitHandle(&xdg);

        if (cmdline_config_path != NULL) {
                if (0 == strcmp(cmdline_config_path, "-")) {
                        config_file = stdin;
                } else {
                        config_file = fopen(cmdline_config_path, "r");
                }

                if(!config_file) {
                        DIE("Cannot find config file: '%s'", cmdline_config_path);
                }
        }
        if (config_file == NULL) {
                config_file = xdgConfigOpen("dunst/dunstrc", "r", &xdg);
        }
        if (config_file == NULL) {
                /* Fall back to just "dunstrc", which was used before 2013-06-23
                 * (before v0.2). */
                config_file = xdgConfigOpen("dunstrc", "r", &xdg);
                if (config_file == NULL) {
                        LOG_W("No dunstrc found.");
                        xdgWipeHandle(&xdg);
                }
        }

        load_ini_file(config_file);
#else
        LOG_M("dunstrc parsing disabled. "
              "Using STATIC_CONFIG is deprecated behavior.");
#endif

        settings.log_level = string_parse_loglevel(option_get_string(
                "global",
                "verbosity", "-verbosity", NULL,
                "The verbosity to log (one of 'crit', 'warn', 'mesg', 'info', 'debug')"
        ), defaults.log_level);


        settings.per_monitor_dpi = string_parse_bool(option_get_string(
                "experimental", "per_monitor_dpi",
                NULL,
                NULL,
                ""
        ), false);

        settings.force_xinerama = string_parse_bool(option_get_string(
                "global", "force_xinerama",
                "-force_xinerama",
                NULL,
                "Force the use of the Xinerama extension"
        ), false);

        settings.font = g_strdup(option_get_string(
                "global", "font",
                "-font/-fn",
                defaults.font,
                "The font dunst should use."
        ));

        // Check if allow_markup set
        if (ini_is_set("global", "allow_markup")) {
                settings.markup = parse_markup_mode(option_get_string(
                        "global", "allow_markup",
                        NULL,
                        NULL,
                        "Allow markup in notifications"
                ), MARKUP_STRIP);
                LOG_M("'allow_markup' is deprecated, please "
                      "use 'markup' instead.");
        }

        settings.markup = parse_markup_mode(option_get_string(
                "global", "markup",
                "-markup",
                NULL,
                "Specify how markup should be handled"
        ), defaults.markup);

        settings.format = g_strdup(option_get_string(
                "global", "format",
                "-format",
                defaults.format,
                "The format template for the notifications"
        ));

        settings.sort = string_parse_bool(option_get_string(
                "global", "sort",
                "-sort",
                NULL,
                "Sort notifications by urgency and date?"
        ), defaults.sort);

        settings.indicate_hidden = string_parse_bool(option_get_string(
                "global", "indicate_hidden",
                "-indicate_hidden",
                NULL,
                "Show how many notificaitons are hidden?"
        ), defaults.indicate_hidden);

        settings.word_wrap = string_parse_bool(option_get_string(
                "global", "word_wrap",
                "-word_wrap",
                NULL,
                "Truncating long lines or do word wrap"
        ), defaults.word_wrap);

        settings.ellipsize = parse_ellipsize(option_get_string(
                "global", "ellipsize",
                "-ellipsize",
                NULL,
                "Ellipsize truncated lines on the start/middle/end"
        ), defaults.ellipsize);

        settings.ignore_newline = string_parse_bool(option_get_string(
                "global", "ignore_newline",
                "-ignore_newline",
                NULL,
                "Ignore newline characters in notifications"
        ), defaults.ignore_newline);

        settings.idle_threshold = string_to_time(option_get_string(
                "global", "idle_threshold",
                "-idle_threshold",
                NULL,
                "Don't timeout notifications if user is longer idle than threshold"
        ), defaults.idle_threshold);

        settings.monitor = string_parse_int(option_get_string(
                "global", "monitor",
                "-mon/-monitor",
                NULL,
                "On which monitor should the notifications be displayed"
        ), defaults.monitor);

        settings.f_mode = parse_follow_mode(option_get_string(
                "global", "follow",
                "-follow",
                NULL,
                "Follow mouse, keyboard or none?"
        ), defaults.f_mode);

        settings.title = g_strdup(option_get_string(
                "global", "title",
                "-t/-title",
                defaults.title,
                "Define the title of windows spawned by dunst."
        ));

        settings.class = g_strdup(option_get_string(
                "global", "class",
                "-c/-class",
                defaults.class,
                "Define the class of windows spawned by dunst."
        ));

        {

                const char *c = option_get_string(
                        "global", "geometry",
                        "-geom/-geometry",
                        NULL,
                        "Geometry for the window"
                );

                if (c) {
                        // TODO: Implement own geometry parsing to get rid of
                        //       the include dependency on X11
                        settings.geometry = x_parse_geometry(c);
                } else {
                        settings.geometry = defaults.geometry;
                }

        }

        settings.shrink = string_parse_bool(option_get_string(
                "global", "shrink",
                "-shrink",
                NULL,
                "Shrink window if it's smaller than the width"
        ), defaults.shrink);

        settings.line_height = string_parse_int(option_get_string(
                "global", "line_height",
                "-lh/-line_height",
                NULL,
                "Add spacing between lines of text"
        ), defaults.line_height);

        settings.notification_height = string_parse_int(option_get_string(
                "global", "notification_height",
                "-nh/-notification_height",
                NULL,
                "Define height of the window"
        ), defaults.notification_height);

        settings.align = parse_alignment(option_get_string(
                "global", "alignment",
                "-align/-alignment", NULL,
                "Text alignment left/center/right"
        ), defaults.align);

        settings.show_age_threshold = string_to_time(option_get_string(
                "global", "show_age_threshold",
                "-show_age_threshold",
                NULL,
                "When should the age of the notification be displayed?"
        ), defaults.show_age_threshold);

        settings.hide_duplicate_count = string_parse_bool(option_get_string(
                "global", "hide_duplicate_count",
                "-hide_duplicate_count",
                NULL,
                "Hide the count of merged notifications with the same content"
        ), false);

        settings.sticky_history = string_parse_bool(option_get_string(
                "global", "sticky_history",
                "-sticky_history",
                NULL,
                "Don't timeout notifications popped up from history"
        ), defaults.sticky_history);

        settings.history_length = string_parse_int(option_get_string(
                "global", "history_length",
                "-history_length",
                NULL,
                "Max amount of notifications kept in history"
        ), defaults.history_length);

        settings.show_indicators = string_parse_bool(option_get_string(
                "global", "show_indicators",
                "-show_indicators",
                NULL,
                "Show indicators for actions \"(A)\" and URLs \"(U)\""
        ), defaults.show_indicators);

        settings.separator_height = string_parse_int(option_get_string(
                "global", "separator_height",
                "-sep_height/-separator_height",
                NULL,
                "height of the separator line"
        ), defaults.separator_height);

        settings.padding = string_parse_int(option_get_string(
                "global", "padding",
                "-padding",
                NULL,
                "Padding between text and separator"
        ), defaults.padding);

        settings.h_padding = string_parse_int(option_get_string(
                "global", "horizontal_padding",
                "-horizontal_padding",
                NULL,
                "horizontal padding"
        ), defaults.h_padding);

        settings.transparency = string_parse_int(option_get_string(
                "global", "transparency",
                "-transparency",
                NULL,
                "Transparency. range 0-100"
        ), defaults.transparency);

        settings.corner_radius = string_parse_int(option_get_string(
                "global", "corner_radius",
                "-corner_radius",
                NULL,
                "Window corner radius"
        ), defaults.corner_radius);

        settings.sep_color = parse_sepcolor(option_get_string(
                "global", "separator_color",
                "-sep_color/-separator_color",
                NULL,
                "Color of the separator line (or 'auto')"
        ), defaults.sep_color);

        settings.stack_duplicates = string_parse_bool(option_get_string(
                "global", "stack_duplicates",
                "-stack_duplicates",
                NULL,
                "Merge multiple notifications with the same content"
        ), true);

        settings.startup_notification = string_parse_bool(option_get_string(
                "global", "startup_notification",
                "-startup_notification",
                NULL,
                "print notification on startup"
        ), false);

        settings.dmenu = string_to_path(option_get_string(
                "global", "dmenu",
                "-dmenu",
                defaults.dmenu,
                "path to dmenu"
        ));

        settings.browser = string_to_path(option_get_string(
                "global", "browser",
                "-browser",
                defaults.browser,
                "path to browser"
        ));

        settings.icon_position = parse_icon_position(option_get_string(
                "global", "icon_position",
                "-icon_position",
                NULL,
                "Align icons left/right/off"
        ), defaults.icon_position);

        settings.max_icon_size = string_parse_int(option_get_string(
                "global", "max_icon_size",
                "-max_icon_size",
                NULL,
                "Scale larger icons down to this size, set to 0 to disable"
        ), defaults.max_icon_size);

        // If the deprecated icon_folders option is used,
        // read it and generate its usage string.
        if (ini_is_set("global", "icon_folders") || cmdline_is_set("-icon_folders")) {
                settings.icon_path = g_strdup(option_get_string(
                        "global", "icon_folders",
                        "-icon_folders",
                        defaults.icon_path,
                        "folders to default icons (deprecated, please use 'icon_path' instead)"
                ));
                LOG_M("The option 'icon_folders' is deprecated, please use 'icon_path' instead.");
        }
        // Read value and generate usage string for icon_path.
        // If icon_path is set, override icon_folder.
        // if not, but icon_folder is set, use that instead of the compile time default.
        settings.icon_path = g_strdup(option_get_string(
                "global", "icon_path",
                "-icon_path",
                settings.icon_path ? settings.icon_path : defaults.icon_path,
                "paths to default icons"
        ));

        // Backwards compatibility with the legacy 'frame' section.
        if (ini_is_set("frame", "width")) {
                settings.frame_width = string_parse_int(option_get_string(
                        "frame", "width",
                        NULL,
                        NULL,
                        "Width of frame around the window"
                ), defaults.frame_width);
                LOG_M("The frame section is deprecated, width has "
                      "been renamed to frame_width and moved to "
                      "the global section.");
        }

        settings.frame_width = string_parse_int(option_get_string(
                "global", "frame_width",
                "-frame_width",
                NULL,
                "Width of frame around the window"
        ), settings.frame_width ? settings.frame_width : defaults.frame_width);

        if (ini_is_set("frame", "color")) {
                settings.frame_color = g_strdup(option_get_string(
                        "frame", "color",
                        NULL,
                        defaults.frame_color,
                        "Color of the frame around the window"
                ));
                LOG_M("The frame section is deprecated, color "
                      "has been renamed to frame_color and moved "
                      "to the global section.");
        }

        settings.frame_color = g_strdup(option_get_string(
                "global", "frame_color",
                "-frame_color",
                settings.frame_color ? settings.frame_color : defaults.frame_color,
                "Color of the frame around the window"
        ));

        settings.lowbgcolor = g_strdup(option_get_string(
                "urgency_low", "background",
                "-lb",
                defaults.lowbgcolor,
                "Background color for notifications with low urgency"
        ));

        settings.lowfgcolor = g_strdup(option_get_string(
                "urgency_low", "foreground",
                "-lf",
                defaults.lowfgcolor,
                "Foreground color for notifications with low urgency"
        ));

        settings.lowframecolor = g_strdup(option_get_string(
                "urgency_low", "frame_color",
                "-lfr",
                NULL,
                "Frame color for notifications with low urgency"
        ));

        settings.timeouts[URG_LOW] = string_to_time(option_get_string(
                "urgency_low", "timeout",
                "-lto",
                NULL,
                "Timeout for notifications with low urgency"
        ), defaults.timeouts[URG_LOW]);

        settings.icons[URG_LOW] = g_strdup(option_get_string(
                "urgency_low", "icon",
                "-li",
                defaults.icons[URG_LOW],
                "Icon for notifications with low urgency"
        ));

        settings.normbgcolor = g_strdup(option_get_string(
                "urgency_normal", "background",
                "-nb",
                defaults.normbgcolor,
                "Background color for notifications with normal urgency"
        ));

        settings.normfgcolor = g_strdup(option_get_string(
                "urgency_normal", "foreground",
                "-nf",
                defaults.normfgcolor,
                "Foreground color for notifications with normal urgency"
        ));

        settings.normframecolor = g_strdup(option_get_string(
                "urgency_normal", "frame_color",
                "-nfr",
                NULL,
                "Frame color for notifications with normal urgency"
        ));

        settings.timeouts[URG_NORM] = string_to_time(option_get_string(
                "urgency_normal", "timeout",
                "-nto",
                NULL,
                "Timeout for notifications with normal urgency"
        ), defaults.timeouts[URG_NORM]);

        settings.icons[URG_NORM] = g_strdup(option_get_string(
                "urgency_normal", "icon",
                "-ni",
                defaults.icons[URG_NORM],
                "Icon for notifications with normal urgency"
        ));

        settings.critbgcolor = g_strdup(option_get_string(
                "urgency_critical", "background",
                "-cb",
                defaults.critbgcolor,
                "Background color for notifications with critical urgency"
        ));

        settings.critfgcolor = g_strdup(option_get_string(
                "urgency_critical", "foreground",
                "-cf",
                defaults.critfgcolor,
                "Foreground color for notifications with ciritical urgency"
        ));

        settings.critframecolor = g_strdup(option_get_string(
                "urgency_critical", "frame_color",
                "-cfr",
                NULL,
                "Frame color for notifications with critical urgency"
        ));

        settings.timeouts[URG_CRIT] = string_to_time(option_get_string(
                "urgency_critical", "timeout",
                "-cto",
                NULL,
                "Timeout for notifications with critical urgency"
        ), defaults.timeouts[URG_CRIT]);

        settings.icons[URG_CRIT] = g_strdup(option_get_string(
                "urgency_critical", "icon",
                "-ci",
                defaults.icons[URG_CRIT],
                "Icon for notifications with critical urgency"
        ));

        settings.close_ks.str = g_strdup(option_get_string(
                "shortcuts", "close",
                "-key",
                defaults.close_ks.str,
                "Shortcut for closing one notification"
        ));

        settings.close_all_ks.str = g_strdup(option_get_string(
                "shortcuts", "close_all",
                "-all_key",
                defaults.close_all_ks.str,
                "Shortcut for closing all notifications"
        ));

        settings.history_ks.str = g_strdup(option_get_string(
                "shortcuts", "history",
                "-history_key",
                defaults.history_ks.str,
                "Shortcut to pop the last notification from history"
        ));

        settings.context_ks.str = g_strdup(option_get_string(
                "shortcuts", "context",
                "-context_key",
                defaults.context_ks.str,
                "Shortcut for context menu"
        ));

        settings.print_notifications = string_parse_bool(cmdline_get_string(
                "-print",
                NULL,
                "Print notifications to cmdline (DEBUG)"
        ), false);

        settings.always_run_script = string_parse_bool(option_get_string(
                "global", "always_run_script",
                "-always_run_script",
                NULL,
                "Always run rule-defined scripts, even if the notification is suppressed with format = \"\"."
        ), true);

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

                if (r == NULL) {
                        r = g_malloc(sizeof(rule_t));
                        rule_init(r);
                        rules = g_slist_insert(rules, r, -1);
                }

                r->name = g_strdup(cur_section);
                r->appname = g_strdup(ini_get_string(cur_section, "appname", r->appname));
                r->summary = g_strdup(ini_get_string(cur_section, "summary", r->summary));
                r->body = g_strdup(ini_get_string(cur_section, "body", r->body));
                r->icon = g_strdup(ini_get_string(cur_section, "icon", r->icon));
                r->category = g_strdup(ini_get_string(cur_section, "category", r->category));
                r->timeout = string_to_time(ini_get_string(cur_section, "timeout", NULL), -1);
                r->markup = parse_markup_mode(ini_get_string(cur_section, "markup", NULL), MARKUP_NULL);
                r->urgency = parse_urgency(ini_get_string(cur_section, "urgency", NULL), URG_NONE);
                r->msg_urgency = parse_urgency(ini_get_string(cur_section, "msg_urgency", NULL), URG_NONE);
                r->fg = g_strdup(ini_get_string(cur_section, "foreground", r->fg));
                r->bg = g_strdup(ini_get_string(cur_section, "background", r->bg));
                r->fc = g_strdup(ini_get_string(cur_section, "frame_color", r->fc));
                r->format = g_strdup(ini_get_string(cur_section, "format", r->format));
                r->new_icon = g_strdup(ini_get_string(cur_section, "new_icon", r->new_icon));
                r->history_ignore = string_parse_bool(ini_get_string(cur_section, "history_ignore", NULL), r->history_ignore);
                r->match_transient = string_parse_bool(ini_get_string(cur_section, "match_transient", NULL), r->match_transient);
                r->set_transient = string_parse_bool(ini_get_string(cur_section, "set_transient", NULL), r->set_transient);
                r->fullscreen = parse_enum_fullscreen(ini_get_string(cur_section,"fullscreen", NULL), r->fullscreen);
                r->script = string_to_path(ini_get_string(cur_section, "script", NULL));
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

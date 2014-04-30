/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include <glib.h>

#ifndef STATIC_CONFIG
#include <basedir.h>
#include <basedir_fs.h>
#endif

#include "dunst.h"
#include "rules.h"
#include "option_parser.h"
#include "settings.h"
#include "config.h"
#include "utils.h"

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
                fprintf(stderr, "Warning: unknown follow mode: \"%s\"\n", mode);
                settings.f_mode = FOLLOW_NONE;
        }

}

static int ini_get_urgency(char *section, char *key, int def)
{
        int ret = def;
        char *urg = ini_get_string(section, key, "");

        if (strlen(urg) > 0) {
                if (strcmp(urg, "low") == 0)
                        ret = LOW;
                else if (strcmp(urg, "normal") == 0)
                        ret = NORM;
                else if (strcmp(urg, "critical") == 0)
                        ret = CRIT;
                else
                        fprintf(stderr,
                                "unknown urgency: %s, ignoring\n",
                                urg);
                free(urg);
        }
        return ret;
}

void load_settings(char *cmdline_config_path)
{

#ifndef STATIC_CONFIG
        xdgHandle xdg;
        FILE *config_file = NULL;

        xdgInitHandle(&xdg);

        if (cmdline_config_path != NULL) {
                config_file = fopen(cmdline_config_path, "r");
        }
        if (config_file == NULL) {
                config_file = xdgConfigOpen("dunst/dunstrc", "r", &xdg);
        }
        if (config_file == NULL) {
                /* Fall back to just "dunstrc", which was used before 2013-06-23
                 * (before v0.2). */
                config_file = xdgConfigOpen("dunstrc", "r", &xdg);
                if (config_file == NULL) {
                        puts("no dunstrc found -> skipping\n");
                        xdgWipeHandle(&xdg);
                }
        }

        load_ini_file(config_file);
#endif

        settings.font =
            option_get_string("global", "font", "-fn", font,
                              "The font dunst should use.");
        settings.allow_markup =
                option_get_bool("global", "allow_markup", "-markup", allow_markup,
                                "Allow markups.");
        settings.format =
            option_get_string("global", "format", "-format", format,
                              "The format template for the notifictions");
        settings.sort =
            option_get_bool("global", "sort", "-sort", sort,
                            "Sort notifications by urgency and date?");
        settings.indicate_hidden =
            option_get_bool("global", "indicate_hidden", "-indicate_hidden",
                            indicate_hidden,
                            "Show how many notificaitons are hidden?");
        settings.word_wrap =
            option_get_bool("global", "word_wrap", "-word_wrap", word_wrap,
                            "Truncating long lines or do word wrap");

        settings.ignore_newline =
            option_get_bool("global", "ignore_newline", "-ignore_newline",
                            ignore_newline,
                            "Ignore newline characters in notifications");
        settings.idle_threshold =
            option_get_int("global", "idle_threshold", "-idle_threshold",
                           idle_threshold,
                           "Don't timeout notifications if user is longer idle than threshold");
        settings.monitor =
            option_get_int("global", "monitor", "-mon", monitor,
                           "On which monitor should the notifications be displayed");
        {
                char *c = option_get_string("global", "follow", "-follow", "",
                                            "Follow mouse, keyboard or none?");
                if (strlen(c) > 0) {
                        parse_follow_mode(c);
                        free(c);
                }
        }
        settings.title =
            option_get_string("global", "title", "-t/-title", title,
                              "Define the title of windows spawned by dunst.");
        settings.class =
            option_get_string("global", "class", "-c/-class", class,
                              "Define the class of windows spawned by dunst.");
        settings.geom =
            option_get_string("global", "geometry", "-geom/-geometry", geom,
                              "Geometry for the window");
        settings.shrink =
            option_get_bool("global", "shrink", "-shrink",
                            shrink,
                            "Shrink window if it's smaller than the width");
        settings.line_height =
            option_get_int("global", "line_height", "-lh/-line_height",
                           line_height,
                           "Add additional padding above and beneath text");
        settings.bounce_freq =
            option_get_double("global", "bounce_freq", "-bounce_freq",
                              bounce_freq,
                              "Make long text bounce from side to side");
        {
                char *c = option_get_string("global", "alignment",
                                            "-align/-alignment", "",
                                            "Align notifications left/center/right");
                if (strlen(c) > 0) {
                        if (strcmp(c, "left") == 0)
                                settings.align = left;
                        else if (strcmp(c, "center") == 0)
                                settings.align = center;
                        else if (strcmp(c, "right") == 0)
                                settings.align = right;
                        else
                                fprintf(stderr,
                                        "Warning: unknown allignment\n");
                        free(c);
                }
        }
        settings.show_age_threshold =
            option_get_int("global", "show_age_threshold",
                           "-show_age_threshold", show_age_threshold,
                           "When should the age of the notification be displayed?");
        settings.sticky_history =
            option_get_bool("global", "sticky_history", "-sticky_history",
                            sticky_history,
                            "Don't timeout notifications popped up from history");
        settings.show_indicators =
            option_get_bool("global", "show_indicators", "-show_indicators",
			    show_indicators,
			    "Show indicators for actions \"(A)\" and URLs \"(U)\"");
        settings.separator_height =
            option_get_int("global", "separator_height",
                           "-sep_height/-separator_height", separator_height,
                           "height of the separator line");
        settings.padding =
            option_get_int("global", "padding", "-padding", padding,
                           "Padding between text and separator");
        settings.h_padding =
            option_get_int("global", "horizontal_padding",
                           "-horizontal_padding", h_padding,
                           "horizontal padding");
        settings.transparency =
            option_get_int("global", "transparency", "-transparency",
                           transparency, "Transparency. range 0-100");
        {
                char *c = option_get_string("global", "separator_color",
                                            "-sep_color/-separator_color", "",
                                            "Color of the separator line (or 'auto')");
                if (strlen(c) > 0) {
                        if (strcmp(c, "auto") == 0)
                                settings.sep_color = AUTO;
                        else if (strcmp(c, "foreground") == 0)
                                settings.sep_color = FOREGROUND;
                        else if (strcmp(c, "frame") == 0)
                                settings.sep_color = FRAME;
                        else {
                                settings.sep_color = CUSTOM;
                                settings.sep_custom_color_str = g_strdup(c);
                        }
                        free(c);
                }
        }

        settings.stack_duplicates = option_get_bool("global", "stack_duplicates",
                        "-stack_duplicates", true, "Merge multiple notifications with the same content");

        settings.startup_notification =
            option_get_bool("global", "startup_notification",
                            "-startup_notification", false,
                            "print notification on startup");

        settings.dmenu =
            option_get_string("global", "dmenu", "-dmenu", dmenu,
                              "path to dmenu");
        settings.dmenu_cmd = string_to_argv(settings.dmenu);

        settings.browser =
            option_get_string("global", "browser", "-browser", browser,
                              "path to browser");

        {
                char *c = option_get_string("global", "icon_position",
                                            "-icon_position", "off",
                                            "Align icons left/right/off");
                if (strlen(c) > 0) {
                        if (strcmp(c, "left") == 0)
                                settings.icon_position = icons_left;
                        else if (strcmp(c, "right") == 0)
                                settings.icon_position = icons_right;
                        else if (strcmp(c, "off") == 0)
                                settings.icon_position = icons_off;
                        else
                                fprintf(stderr,
                                        "Warning: unknown icon position: %s\n", c);
                        free(c);
                }
        }

        settings.icon_folders =
            option_get_string("global", "icon_folders", "-icon_folders", icon_folders,
                              "paths to default icons");

        settings.frame_width =
            option_get_int("frame", "width", "-frame_width", frame_width,
                           "Width of frame around window");

        settings.frame_color =
            option_get_string("frame", "color", "-frame_color", frame_color,
                              "Color of the frame around window");

        settings.lowbgcolor =
            option_get_string("urgency_low", "background", "-lb", lowbgcolor,
                              "Background color for notifcations with low urgency");
        settings.lowfgcolor =
            option_get_string("urgency_low", "foreground", "-lf", lowfgcolor,
                              "Foreground color for notifications with low urgency");
        settings.timeouts[LOW] =
            option_get_int("urgency_low", "timeout", "-lto", timeouts[LOW],
                           "Timeout for notifications with low urgency");
        settings.icons[LOW] =
            option_get_string("urgency_low", "icon", "-li", icons[LOW],
                              "Icon for notifications with low urgency");
        settings.normbgcolor =
            option_get_string("urgency_normal", "background", "-nb",
                              normbgcolor,
                              "Background color for notifications with normal urgency");
        settings.normfgcolor =
            option_get_string("urgency_normal", "foreground", "-nf",
                              normfgcolor,
                              "Foreground color for notifications with normal urgency");
        settings.timeouts[NORM] =
            option_get_int("urgency_normal", "timeout", "-nto", timeouts[NORM],
                           "Timeout for notifications with normal urgency");
        settings.icons[NORM] =
            option_get_string("urgency_normal", "icon", "-ni", icons[NORM],
                              "Icon for notifications with normal urgency");
        settings.critbgcolor =
            option_get_string("urgency_critical", "background", "-cb",
                              critbgcolor,
                              "Background color for notifications with critical urgency");
        settings.critfgcolor =
            option_get_string("urgency_critical", "foreground", "-cf",
                              critfgcolor,
                              "Foreground color for notifications with ciritical urgency");
        settings.timeouts[CRIT] =
            option_get_int("urgency_critical", "timeout", "-cto", timeouts[CRIT],
                           "Timeout for notifications with critical urgency");
        settings.icons[CRIT] =
            option_get_string("urgency_critical", "icon", "-ci", icons[CRIT],
                              "Icon for notifications with critical urgency");

        settings.close_ks.str =
            option_get_string("shortcuts", "close", "-key", close_ks.str,
                              "Shortcut for closing one notification");
        settings.close_all_ks.str =
            option_get_string("shortcuts", "close_all", "-all_key",
                              close_all_ks.str,
                              "Shortcut for closing all notifications");
        settings.history_ks.str =
            option_get_string("shortcuts", "history", "-history_key",
                              history_ks.str,
                              "Shortcut to pop the last notification from history");

        settings.context_ks.str =
            option_get_string("shortcuts", "context", "-context_key",
                              context_ks.str, "Shortcut for context menu");

        settings.print_notifications =
            cmdline_get_bool("-print", false,
                             "Print notifications to cmdline (DEBUG)");

        /* push hardcoded default rules into rules list */
        for (int i = 0; i < LENGTH(default_rules); i++) {
                rules = g_slist_insert(rules, &(default_rules[i]), -1);
        }

        char *cur_section = NULL;
        for (;;) {
                cur_section = next_section(cur_section);
                if (!cur_section)
                        break;
                if (strcmp(cur_section, "global") == 0
                    || strcmp(cur_section, "shortcuts") == 0
                    || strcmp(cur_section, "urgency_low") == 0
                    || strcmp(cur_section, "urgency_normal") == 0
                    || strcmp(cur_section, "urgency_critical") == 0)
                        continue;

                /* check for existing rule with same name */
                rule_t *r = NULL;
                for (GSList * iter = rules; iter; iter = iter->next) {
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
                r->appname = ini_get_string(cur_section, "appname", r->appname);
                r->summary = ini_get_string(cur_section, "summary", r->summary);
                r->body = ini_get_string(cur_section, "body", r->body);
                r->icon = ini_get_string(cur_section, "icon", r->icon);
                r->timeout = ini_get_int(cur_section, "timeout", r->timeout);
                r->urgency = ini_get_urgency(cur_section, "urgency", r->urgency);
                r->msg_urgency = ini_get_urgency(cur_section, "msg_urgency", r->msg_urgency);
                r->fg = ini_get_string(cur_section, "foreground", r->fg);
                r->bg = ini_get_string(cur_section, "background", r->bg);
                r->format = ini_get_string(cur_section, "format", r->format);
                r->new_icon = ini_get_string(cur_section, "new_icon", r->new_icon);
				r->script = ini_get_string(cur_section, "script", NULL);
        }

#ifndef STATIC_CONFIG
        if (config_file) {
                fclose(config_file);
                free_ini();
                xdgWipeHandle(&xdg);
        }
#endif
}
/* vim: set ts=8 sw=8 tw=0: */

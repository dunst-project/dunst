/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_SETTING_DATA_H
#define DUNST_SETTING_DATA_H
#include <stddef.h>

#include "option_parser.h"
#include "rules.h"

struct string_to_enum_def {
        const char* string;
        const int enum_value;
};

struct setting {
      /**
       * A string with the setting key as found in the config file.
       */
      char *name;

      /**
       * A string with the ini section where the variable is allowed. This
       * section should be part of the special_sections array.
       *
       * Example:
       *        .section = "global",
       */
      char *section;

      /**
       * A string with a short description of the config variable. This is
       * currently not used, but it may be used to generate help messages.
       */
      char *description;

      // IDEA: Add long description to generate man page from this. This could
      // also be useful for an extended help text.

      /**
       * Enum of the setting type. Every setting type is parsed differently in
       * option_parser.c.
       */
      enum setting_type type;

      /**
       * A string with the default value of the setting. This should be the
       * same as what it would be in the config file, as this is parsed by the
       * same parser.
       * default_value is unused when the setting is only a rule (value == NULL).
       *
       * Example:
       *        .default_value = "10s", // 10 seconds of time
       */
      char *default_value;

      /**
       * (nullable)
       * A pointer to the corresponding setting in the setting struct. Make
       * sure to always take the address, even if it's already a pointer in the
       * settings struct.
       * If value is NULL, the setting is interpreted as a rule.
       *
       * Example:
       *        .value = &settings.font,
       */
      void *value;


      /**
       * (nullable)
       * Function pointer for the parser - to be used in case of enums or other
       * special settings. If the parse requires extra data, it should be given
       * with parser_data. This allows for one generic parser for, for example,
       * enums, instead of a parser for every enum.
       *
       * @param data The required data for parsing the value. See parser_data.
       * @param cfg_value The string representing the value of the config
       * variable
       * @param ret A pointer to the return value. This casted by the parser to
       * the right type.
       */
      int (*parser)(const void* data, const char *cfg_value, void* ret);

      /**
       * (nullable)
       * A pointer to the data required for the parser to parse this setting.
       */
      const void* parser_data; // This is passed to the parser function

      /**
       * The offset of this setting in the rule struct, if it exists. Zero is
       * being interpreted as if no rule exists for this setting.
       *
       * Example:
       *        .rule_offset = offsetof(struct rule, *member*);
       */
      size_t rule_offset;
};

static const struct rule empty_rule = {
        .name            = "empty",
        .appname         = NULL,
        .summary         = NULL,
        .body            = NULL,
        .icon            = NULL,
        .category        = NULL,
        .msg_urgency     = URG_NONE,
        .timeout         = -1,
        .urgency         = URG_NONE,
        .markup          = MARKUP_NULL,
        .history_ignore  = -1,
        .match_transient = -1,
        .set_transient   = -1,
        .skip_display    = -1,
        .new_icon        = NULL,
        .fg              = NULL,
        .bg              = NULL,
        .format          = NULL,
        .script          = NULL,
};


#ifndef ZWLR_LAYER_SHELL_V1_LAYER_ENUM
#define ZWLR_LAYER_SHELL_V1_LAYER_ENUM
// Needed for compiling without wayland dependency
const enum zwlr_layer_shell_v1_layer {
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND = 0,
        ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM = 1,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP = 2,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY = 3,
};
#endif /* ZWLR_LAYER_SHELL_V1_LAYER_ENUM */

enum list_type {
        INVALID_LIST = 0,
        MOUSE_LIST = 1,
        ORIGIN_LIST = 2,
        OFFSET_LIST = 3,
};

#define ENUM_END {NULL, 0}
static const struct string_to_enum_def verbosity_enum_data[] = {
        {"critical", G_LOG_LEVEL_CRITICAL },
        {"crit", G_LOG_LEVEL_CRITICAL },
        {"warning", G_LOG_LEVEL_WARNING },
        {"warn", G_LOG_LEVEL_WARNING },
        {"message", G_LOG_LEVEL_MESSAGE },
        {"mesg", G_LOG_LEVEL_MESSAGE },
        {"info", G_LOG_LEVEL_INFO },
        {"debug", G_LOG_LEVEL_DEBUG },
        {"deb", G_LOG_LEVEL_DEBUG },
        ENUM_END,
};

static const struct string_to_enum_def boolean_enum_data[] = {
        {"True", true },
        {"true", true },
        {"On", true },
        {"on", true },
        {"Yes", true },
        {"yes", true },
        {"1", true },
        {"False", false },
        {"false", false },
        {"Off", false },
        {"off", false },
        {"No", false },
        {"no", false },
        {"0", false },
        {"n", false },
        {"y", false },
        {"N", false },
        {"Y", true },
        ENUM_END,
};

static const struct string_to_enum_def horizontal_alignment_enum_data[] = {
        {"left",   ALIGN_LEFT },
        {"center", ALIGN_CENTER },
        {"right",  ALIGN_RIGHT },
        ENUM_END,
};

static const struct string_to_enum_def ellipsize_enum_data[] = {
        {"start",  ELLIPSE_START },
        {"middle", ELLIPSE_MIDDLE },
        {"end",    ELLIPSE_END },
        ENUM_END,
};

static struct string_to_enum_def follow_mode_enum_data[] = {
        {"mouse",    FOLLOW_MOUSE },
        {"keyboard", FOLLOW_KEYBOARD },
        {"none",     FOLLOW_NONE },
        ENUM_END,
};

static const struct string_to_enum_def fullscreen_enum_data[] = {
        {"show",     FS_SHOW },
        {"delay",    FS_DELAY },
        {"pushback", FS_PUSHBACK },
        ENUM_END,
};

static const struct string_to_enum_def icon_position_enum_data[] = {
        {"left",  ICON_LEFT },
        {"right", ICON_RIGHT },
        {"off",   ICON_OFF },
        ENUM_END,
};

static const struct string_to_enum_def vertical_alignment_enum_data[] = {
        {"top",     VERTICAL_TOP },
        {"center",  VERTICAL_CENTER },
        {"bottom",  VERTICAL_BOTTOM },
        ENUM_END,
};

static const struct string_to_enum_def markup_mode_enum_data[] = {
        {"strip", MARKUP_STRIP },
        {"no",    MARKUP_NO },
        {"full",  MARKUP_FULL },
        {"yes",   MARKUP_FULL },
        ENUM_END,
};

static const struct string_to_enum_def mouse_action_enum_data[] = {
        {"none",           MOUSE_NONE },
        {"do_action",      MOUSE_DO_ACTION },
        {"close_current",  MOUSE_CLOSE_CURRENT },
        {"close_all",      MOUSE_CLOSE_ALL },
        ENUM_END,
};

static const struct string_to_enum_def sep_color_enum_data[] = {
        {"auto",        SEP_AUTO },
        {"foreground",  SEP_FOREGROUND },
        {"frame",       SEP_FRAME },
        ENUM_END,
};

static const struct string_to_enum_def urgency_enum_data[] = {
        {"low",      URG_LOW },
        {"normal",   URG_NORM },
        {"critical", URG_CRIT },
        ENUM_END,
};

static const struct string_to_enum_def layer_enum_data[] = {
        {"bottom",  ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM },
        {"top",     ZWLR_LAYER_SHELL_V1_LAYER_TOP },
        {"overlay", ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY },
        ENUM_END,
};

static const struct string_to_enum_def origin_enum_data[] = {

        { "top", ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP },
        { "bottom", ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM },
        { "left", ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT },
        { "right", ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT },
        ENUM_END,
};

static const struct setting allowed_settings[] = {
        // match rules below
        {
                .name = "appname",
                .section = "*",
                .description = "The name of the application as reported by the client. Be aware that the name can often differ depending on the locale used.",
                .type = TYPE_STRING,
                .default_value = "*", // default_value is not used for rules
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, appname),
        },
        {
                .name = "body",
                .section = "*",
                .description = "The body of the notification",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, body),
        },
        {
                .name = "category",
                .section = "*",
                .description = "The category of the notification as defined by the notification spec. See https://developer.gnome.org/notification-spec/#categories",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, category),
        },
        {
                .name = "desktop_entry",
                .section = "*",
                .description = "GLib based applications export their desktop-entry name. In comparison to the appname, the desktop-entry won't get localized.",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, desktop_entry),
        },
        {
                .name = "icon",
                .section = "*",
                .description = "The icon of the notification in the form of a file path. Can be empty if no icon is available or a raw icon is used instead.",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, icon),
        },
        {
                .name = "match_transient",
                .section = "*",
                .description = "Match if the notification has been declared as transient by the client or by some other rule.",
                .type = TYPE_CUSTOM,
                .default_value = "*",
                .value = NULL,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
                .rule_offset = offsetof(struct rule, match_transient),
        },
        {
                .name = "msg_urgency",
                .section = "*",
                .description = "Matches the urgency of the notification as set by the client or by some other rule.",
                .type = TYPE_CUSTOM,
                .default_value = "*",
                .value = NULL,
                .parser = string_parse_enum,
                .parser_data = urgency_enum_data,
                .rule_offset = offsetof(struct rule, msg_urgency),
        },
        {
                .name = "stack_tag",
                .section = "*",
                .description = "Matches the stack tag of the notification as set by the client or by some other rule.",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, stack_tag),
        },
        {
                .name = "summary",
                .section = "*",
                .description = "summary text of the notification",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, summary),
        },

        // modifying rules below
        {
                .name = "script",
                .section = "*",
                .description = "script",
                .type = TYPE_PATH,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, script),
        },
        {
                .name = "background",
                .section = "*",
                .description = "The background color of the notification.",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, bg),
        },
        {
                .name = "foreground",
                .section = "*",
                .description = "The foreground color of the notification.",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, fg),
        },
        {
                .name = "highlight",
                .section = "*",
                .description = "The highlight color of the notification.",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, highlight),
        },
        {
                .name = "format",
                .section = "global",
                .description = "The format template for the notifications",
                .type = TYPE_STRING,
                .default_value = "<b>%s</b>\n%b",
                .value = &settings.format,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, format),
        },
        {
                .name = "fullscreen",
                .section = "*",
                .description = "This attribute specifies how notifications are handled if a fullscreen window is focused. One of show, delay, or pushback.",
                .type = TYPE_CUSTOM,
                .default_value = "show",
                .value = NULL,
                .parser = string_parse_enum,
                .parser_data = fullscreen_enum_data,
                .rule_offset = offsetof(struct rule, fullscreen),
        },
        {
                .name = "new_icon",
                .section = "*",
                .description = "Updates the icon of the notification, it should be a path to a valid image.",
                .type = TYPE_PATH,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, new_icon),
        },
        {
                .name = "set_stack_tag",
                .section = "*",
                .description = "Sets the stack tag for the notification, notifications with the same (non-empty) stack tag will replace each-other so only the newest one is visible.",
                .type = TYPE_STRING,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, stack_tag),
        },
        {
                .name = "set_transient",
                .section = "*",
                .description = "Sets whether the notification is considered transient.  Transient notifications will bypass the idle_threshold setting.",
                .type = TYPE_CUSTOM,
                .default_value = "*",
                .value = NULL,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
                .rule_offset = offsetof(struct rule, set_transient),
        },
        {
                .name = "timeout",
                .section = "*",
                .description = "Don't timeout notifications if user is longer idle than threshold",
                .type = TYPE_TIME,
                .default_value = "*",
                .value = NULL,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, timeout),
        },
        {
                .name = "urgency",
                .section = "*",
                .description = "This sets the notification urgency.",
                .type = TYPE_CUSTOM,
                .default_value = "*",
                .value = NULL,
                .parser = string_parse_enum,
                .parser_data = urgency_enum_data,
                .rule_offset = offsetof(struct rule, urgency),
        },
        {
                .name = "skip_display",
                .section = "*",
                .description = "Setting this to true will prevent the notification from being displayed initially but will be saved in history for later viewing.",
                .type = TYPE_CUSTOM,
                .default_value = "*",
                .value = NULL,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
                .rule_offset = offsetof(struct rule, skip_display),
        },
        {
                .name = "frame",
                .section = "*",
                .description = "Setting this to true will prevent the notification from being displayed initially but will be saved in history for later viewing.",
                .type = TYPE_CUSTOM,
                .default_value = "*",
                .value = NULL,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
                .rule_offset = offsetof(struct rule, skip_display),
        },
        {
                .name = "frame_color",
                .section = "*",
                .description = "Color of the frame around the window",
                .type = TYPE_STRING,
                .default_value = "#888888",
                .value = &settings.frame_color,
                .parser = NULL,
                .parser_data = NULL,
                .rule_offset = offsetof(struct rule, fc),
        },

        // other settings below
        {
                .name = "per_monitor_dpi",
                .section = "experimental",
                .description = "Use a different DPI per monitor",
                .type = TYPE_CUSTOM,
                .default_value = "false",
                .value = &settings.per_monitor_dpi,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "force_xinerama",
                .section = "global",
                .description = "Force the use of the Xinerama extension",
                .type = TYPE_CUSTOM,
                .default_value = "false",
                .value = &settings.force_xinerama,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "force_xwayland",
                .section = "global",
                .description = "Force the use of the xwayland output",
                .type = TYPE_CUSTOM,
                .default_value = "false",
                .value = &settings.force_xwayland,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "font",
                .section = "global",
                .description = "The font dunst should use.",
                .type = TYPE_STRING,
                .default_value = "Monospace 8",
                .value = &settings.font,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "sort",
                .section = "global",
                .description = "Sort notifications by urgency and date?",
                .type = TYPE_CUSTOM,
                .default_value = "true",
                .value = &settings.sort,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "indicate_hidden",
                .section = "global",
                .description = "Show how many notifications are hidden",
                .type = TYPE_CUSTOM,
                .default_value = "true",
                .value = &settings.indicate_hidden,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "word_wrap",
                .section = "global",
                .description = "Truncating long lines or do word wrap",
                .type = TYPE_CUSTOM,
                .default_value = "true",
                .value = &settings.word_wrap,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "ignore_dbusclose",
                .section = "global",
                .description = "Ignore dbus CloseNotification events",
                .type = TYPE_CUSTOM,
                .default_value = "false",
                .value = &settings.ignore_dbusclose,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "ignore_newline",
                .section = "global",
                .description = "Ignore newline characters in notifications",
                .type = TYPE_CUSTOM,
                .default_value = "false",
                .value = &settings.ignore_newline,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "idle_threshold",
                .section = "global",
                .description = "Don't timeout notifications if user is longer idle than threshold",
                .type = TYPE_TIME,
                .default_value = "0",
                .value = &settings.idle_threshold,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "monitor",
                .section = "global",
                .description = "On which monitor should the notifications be displayed",
                .type = TYPE_INT,
                .default_value = "0",
                .value = &settings.monitor,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "title",
                .section = "global",
                .description = "Define the title of windows spawned by dunst.",
                .type = TYPE_STRING,
                .default_value = "Dunst",
                .value = &settings.title,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "class",
                .section = "global",
                .description = "Define the class of windows spawned by dunst.",
                .type = TYPE_STRING,
                .default_value = "Dunst",
                .value = &settings.class,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "shrink",
                .section = "global",
                .description = "Shrink window if it's smaller than the width",
                .type = TYPE_CUSTOM,
                .default_value = "false",
                .value = &settings.shrink,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "line_height",
                .section = "global",
                .description = "Add spacing between lines of text",
                .type = TYPE_INT,
                .default_value = "0",
                .value = &settings.line_height,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "notification_height",
                .section = "global",
                .description = "Define height of the window",
                .type = TYPE_INT,
                .default_value = "0",
                .value = &settings.notification_height,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "show_age_threshold",
                .section = "global",
                .description = "When should the age of the notification be displayed?",
                .type = TYPE_TIME,
                .default_value = "60",
                .value = &settings.show_age_threshold,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "hide_duplicate_count",
                .section = "global",
                .description = "Hide the count of stacked notifications with the same content",
                .type = TYPE_CUSTOM,
                .default_value = "false",
                .value = &settings.hide_duplicate_count,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "sticky_history",
                .section = "global",
                .description = "Don't timeout notifications popped up from history",
                .type = TYPE_CUSTOM,
                .default_value = "true",
                .value = &settings.sticky_history,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "history_length",
                .section = "global",
                .description = "Max amount of notifications kept in history",
                .type = TYPE_INT,
                .default_value = "20",
                .value = &settings.history_length,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "show_indicators",
                .section = "global",
                .description = "Show indicators for actions \"(A)\" and URLs \"(U)\"",
                .type = TYPE_CUSTOM,
                .default_value = "true",
                .value = &settings.show_indicators,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "separator_height",
                .section = "global",
                .description = "height of the separator line",
                .type = TYPE_INT,
                .default_value = "2",
                .value = &settings.separator_height,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "padding",
                .section = "global",
                .description = "Padding between text and separator",
                .type = TYPE_INT,
                .default_value = "8",
                .value = &settings.padding,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "horizontal_padding",
                .section = "global",
                .description = "horizontal padding",
                .type = TYPE_INT,
                .default_value = "8",
                .value = &settings.h_padding,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "text_icon_padding",
                .section = "global",
                .description = "Padding between text and icon",
                .type = TYPE_INT,
                .default_value = "0",
                .value = &settings.text_icon_padding,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "transparency",
                .section = "global",
                .description = "Transparency. Range 0-100",
                .type = TYPE_INT,
                .default_value = "0",
                .value = &settings.transparency,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "corner_radius",
                .section = "global",
                .description = "Window corner radius",
                .type = TYPE_INT,
                .default_value = "0",
                .value = &settings.corner_radius,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "progress_bar_height",
                .section = "global",
                .description = "Height of the progress bar",
                .type = TYPE_INT,
                .default_value = "10",
                .value = &settings.progress_bar_height,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "progress_bar_min_width",
                .section = "global",
                .description = "Minimum width of the progress bar",
                .type = TYPE_INT,
                .default_value = "150",
                .value = &settings.progress_bar_min_width,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "progress_bar_max_width",
                .section = "global",
                .description = "Maximum width of the progress bar",
                .type = TYPE_INT,
                .default_value = "300",
                .value = &settings.progress_bar_max_width,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "progress_bar_frame_width",
                .section = "global",
                .description = "Frame width of the progress bar",
                .type = TYPE_INT,
                .default_value = "1",
                .value = &settings.progress_bar_frame_width,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "progress_bar",
                .section = "global",
                .description = "Show the progress bar",
                .type = TYPE_CUSTOM,
                .default_value = "true",
                .value = &settings.progress_bar,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "stack_duplicates",
                .section = "global",
                .description = "Stack together notifications with the same content",
                .type = TYPE_CUSTOM,
                .default_value = "true",
                .value = &settings.stack_duplicates,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        {
                .name = "dmenu",
                .section = "global",
                .description = "path to dmenu",
                .type = TYPE_PATH,
                .default_value = "/usr/bin/dmenu -p dunst",
                .value = &settings.dmenu,
                .parser = NULL,
                .parser_data = &settings.dmenu_cmd,
        },
        {
                .name = "browser",
                .section = "global",
                .description = "path to browser",
                .type = TYPE_PATH,
                .default_value = "/usr/bin/firefox --new-tab",
                .value = &settings.browser,
                .parser = NULL,
                .parser_data = &settings.browser_cmd,
        },
        {
                .name = "min_icon_size",
                .section = "global",
                .description = "Scale smaller icons up to this size, set to 0 to disable. If max_icon_size also specified, that has the final say.",
                .type = TYPE_INT,
                .default_value = "0",
                .value = &settings.min_icon_size,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "max_icon_size",
                .section = "global",
                .description = "Scale larger icons down to this size, set to 0 to disable",
                .type = TYPE_INT,
                .default_value = "32",
                .value = &settings.max_icon_size,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "always_run_script",
                .section = "global",
                .description = "Always run rule-defined scripts, even if the notification is suppressed with format = \"\".",
                .type = TYPE_CUSTOM,
                .default_value = "true",
                .value = &settings.always_run_script,
                .parser = string_parse_bool,
                .parser_data = boolean_enum_data,
        },
        // manual extractions below
        {
                .name = "markup",
                .section = "global",
                .description = "Specify how markup should be handled",
                .type = TYPE_CUSTOM,
                .default_value = "full",
                .value = &settings.markup,
                .parser = string_parse_enum,
                .parser_data = markup_mode_enum_data,
        },
        {
                .name = "ellipsize",
                .section = "global",
                .description = "Ellipsize truncated lines on the start/middle/end",
                .type = TYPE_CUSTOM,
                .default_value = "middle",
                .value = &settings.ellipsize,
                .parser = string_parse_enum,
                .parser_data = ellipsize_enum_data,
        },
        {
                .name = "follow",
                .section = "global",
                .description = "Follow mouse, keyboard or none?",
                .type = TYPE_CUSTOM,
                .default_value = "mouse",
                .value = &settings.f_mode,
                .parser = string_parse_enum,
                .parser_data = follow_mode_enum_data,
        },
        {
                .name = "geometry",
                .section = "global",
                .description = "Geometry for the window",
                .type = TYPE_GEOMETRY,
                .default_value = "300x5-30+20",
                .value = &settings.geometry,
                .parser = NULL, // TODO replace this setting with some other format and get rid of x11 compile time dependency
                .parser_data = NULL,
        },
        {
                .name = "alignment",
                .section = "global",
                .description = "Text alignment left/center/right",
                .type = TYPE_CUSTOM,
                .default_value = "left",
                .value = &settings.align,
                .parser = string_parse_enum,
                .parser_data = horizontal_alignment_enum_data,
        },
        {
                .name = "separator_color",
                .section = "global",
                .description = "Color of the separator line (or 'auto')",
                .type = TYPE_CUSTOM,
                .default_value = "frame",
                .value = &settings.sep_color,
                .parser = string_parse_sepcolor,
                .parser_data = sep_color_enum_data,
        },
        {
                .name = "icon_position",
                .section = "global",
                .description = "Align icons left/right/off",
                .type = TYPE_CUSTOM,
                .default_value = "left",
                .value = &settings.icon_position,
                .parser = string_parse_enum,
                .parser_data = icon_position_enum_data,
        },
        {
                .name = "vertical_alignment",
                .section = "global",
                .description = "Align icon and text top/center/bottom",
                .type = TYPE_CUSTOM,
                .default_value = "center",
                .value = &settings.vertical_alignment,
                .parser = string_parse_enum,
                .parser_data = vertical_alignment_enum_data,
        },
        {
                .name = "layer",
                .section = "global",
                .description = "Select the layer where notifications should be placed",
                .type = TYPE_CUSTOM,
                .default_value = "overlay",
                .value = &settings.layer,
                .parser = string_parse_enum,
                .parser_data = layer_enum_data,
        },
        {
                .name = "mouse_left_click",
                .section = "global",
                .description = "Action of left click event",
                .type = TYPE_LIST,
                .default_value = "close_current",
                .value = &settings.mouse_left_click,
                .parser = NULL,
                .parser_data = GINT_TO_POINTER(MOUSE_LIST),
        },
        {
                .name = "mouse_middle_click",
                .section = "global",
                .description = "Action of middle click event",
                .type = TYPE_LIST,
                .default_value = "do_action, close_current",
                .value = &settings.mouse_middle_click,
                .parser = NULL,
                .parser_data = GINT_TO_POINTER(MOUSE_LIST),
        },
        {
                .name = "mouse_right_click",
                .section = "global",
                .description = "Action of right click event",
                .type = TYPE_LIST,
                .default_value = "close_all",
                .value = &settings.mouse_right_click,
                .parser = NULL,
                .parser_data = GINT_TO_POINTER(MOUSE_LIST),
        },
        {
                .name = "icon_path",
                .section = "global",
                .description = "paths to default icons",
                .type = TYPE_STRING,
                .default_value = "/usr/share/icons/gnome/16x16/status/:/usr/share/icons/gnome/16x16/devices/",
                .value = &settings.icon_path,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "frame_width",
                .section = "global",
                .description = "Width of frame around the window",
                .type = TYPE_INT,
                .default_value = "3",
                .value = &settings.frame_width,
                .parser = NULL,
                .parser_data = NULL,
        },

        // These are only used for setting defaults, since there is a rule
        // above doing the same.
        // TODO it's probably better to create an array with default rules.
        {
                .name = "background",
                .section = "urgency_low",
                .description = "Background color for notifications with low urgency",
                .type = TYPE_STRING,
                .default_value = "#222222",
                .value = &settings.colors_low.bg,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "foreground",
                .section = "urgency_low",
                .description = "Foreground color for notifications with low urgency",
                .type = TYPE_STRING,
                .default_value = "#888888",
                .value = &settings.colors_low.fg,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "highlight",
                .section = "urgency_low",
                .description = "Highlight color for notifications with low urgency",
                .type = TYPE_STRING,
                .default_value = "#7f7fff",
                .value = &settings.colors_low.highlight,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "frame_color",
                .section = "urgency_low",
                .description = "Frame color for notifications with low urgency",
                .type = TYPE_STRING,
                .default_value = "#888888",
                .value = &settings.colors_low.frame,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "timeout",
                .section = "urgency_low",
                .description = "Timeout for notifications with low urgency",
                .type = TYPE_TIME,
                .default_value = "10", // in seconds
                .value = &settings.timeouts[URG_LOW],
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "icon",
                .section = "urgency_low",
                .description = "Icon for notifications with low urgency",
                .type = TYPE_STRING,
                .default_value = "dialog-information",
                .value = &settings.icons[URG_LOW],
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "background",
                .section = "urgency_normal",
                .description = "Background color for notifications with normal urgency",
                .type = TYPE_STRING,
                .default_value = "#285577",
                .value = &settings.colors_norm.bg,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "foreground",
                .section = "urgency_normal",
                .description = "Foreground color for notifications with normal urgency",
                .type = TYPE_STRING,
                .default_value = "#ffffff",
                .value = &settings.colors_norm.fg,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "highlight",
                .section = "urgency_normal",
                .description = "Highlight color for notifications with normal urgency",
                .type = TYPE_STRING,
                .default_value = "#1745d1",
                .value = &settings.colors_norm.highlight,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "frame_color",
                .section = "urgency_normal",
                .description = "Frame color for notifications with normal urgency",
                .type = TYPE_STRING,
                .default_value = "#888888",
                .value = &settings.colors_norm.frame,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "timeout",
                .section = "urgency_normal",
                .description = "Timeout for notifications with normal urgency",
                .type = TYPE_TIME,
                .default_value = "10",
                .value = &settings.timeouts[URG_NORM],
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "icon",
                .section = "urgency_normal",
                .description = "Icon for notifications with normal urgency",
                .type = TYPE_STRING,
                .default_value = "dialog-information",
                .value = &settings.icons[URG_NORM],
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "background",
                .section = "urgency_critical",
                .description = "Background color for notifications with critical urgency",
                .type = TYPE_STRING,
                .default_value = "#ffffff",
                .value = &settings.colors_crit.bg,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "foreground",
                .section = "urgency_critical",
                .description = "Foreground color for notifications with ciritical urgency",
                .type = TYPE_STRING,
                .default_value = "#ffffff",
                .value = &settings.colors_crit.fg,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "highlight",
                .section = "urgency_critical",
                .description = "Highlight color for notifications with ciritical urgency",
                .type = TYPE_STRING,
                .default_value = "#ff6666",
                .value = &settings.colors_crit.highlight,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "frame_color",
                .section = "urgency_critical",
                .description = "Frame color for notifications with critical urgency",
                .type = TYPE_STRING,
                .default_value = "#ff0000",
                .value = &settings.colors_crit.frame,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "timeout",
                .section = "urgency_critical",
                .description = "Timeout for notifications with critical urgency",
                .type = TYPE_TIME,
                .default_value = "0",
                .value = &settings.timeouts[URG_CRIT],
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "icon",
                .section = "urgency_critical",
                .description = "Icon for notifications with critical urgency",
                .type = TYPE_STRING,
                .default_value = "dialog-warning",
                .value = &settings.icons[URG_CRIT],
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "origin",
                .section = "global",
                .description = "Specifies the where the notification is positioned before offsetting.",
                .type = TYPE_LIST,
                .default_value = "top, right",
                .value = &settings.origin,
                .parser = NULL,
                .parser_data = GINT_TO_POINTER(ORIGIN_LIST),
        },
        {
                .name = "width",
                .section = "global",
                .description = "The width of the notification.",
                .type = TYPE_LENGTH,
                .default_value = "300",
                .value = &settings.width,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "height",
                .section = "global",
                .description = "The height of the notification.",
                .type = TYPE_LENGTH,
                .default_value = "(0, 500)",
                .value = &settings.height,
                .parser = NULL,
                .parser_data = NULL,
        },
        {
                .name = "offset",
                .section = "global",
                .description = "The offset of the notification from the origin.",
                .type = TYPE_LIST,
                .default_value = "10x50",
                .value = &settings.offset,
                .parser = NULL,
                .parser_data = GINT_TO_POINTER(OFFSET_LIST),
        },
        {
                .name = "notification_limit",
                .section = "global",
                .description = "Maximum number of notifications allowed",
                .type = TYPE_INT,
                .default_value = "0",
                .value = &settings.notification_limit,
                .parser = NULL,
                .parser_data = NULL,
        },

};
#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

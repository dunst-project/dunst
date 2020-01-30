/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_SETTINGS_H
#define DUNST_SETTINGS_H

#include <stdbool.h>

#include "markup.h"
#include "notification.h"
#include "x11/x.h"

enum alignment { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };
enum ellipsize { ELLIPSE_START, ELLIPSE_MIDDLE, ELLIPSE_END };
enum icon_position { ICON_LEFT, ICON_RIGHT, ICON_OFF };
enum vertical_alignment { VERTICAL_TOP, VERTICAL_CENTER, VERTICAL_BOTTOM };
enum separator_color { SEP_FOREGROUND, SEP_AUTO, SEP_FRAME, SEP_CUSTOM };
enum follow_mode { FOLLOW_NONE, FOLLOW_MOUSE, FOLLOW_KEYBOARD };
enum mouse_action { MOUSE_NONE, MOUSE_DO_ACTION, MOUSE_CLOSE_CURRENT, MOUSE_CLOSE_ALL };

struct separator_color_data {
        enum separator_color type;
        char *sep_color;
};

struct geometry {
        int x;
        int y;
        unsigned int w;
        unsigned int h;
        bool negative_x;
        bool negative_y;
        bool negative_width;
        bool width_set;
};

struct settings {
        bool print_notifications;
        bool per_monitor_dpi;
        enum markup_mode markup;
        bool stack_duplicates;
        bool hide_duplicate_count;
        char *font;
        struct notification_colors colors_low;
        struct notification_colors colors_norm;
        struct notification_colors colors_crit;
        char *format;
        gint64 timeouts[3];
        char *icons[3];
        unsigned int transparency;
        struct geometry geometry;
        char *title;
        char *class;
        int shrink;
        int sort;
        int indicate_hidden;
        gint64 idle_threshold;
        gint64 show_age_threshold;
        enum alignment align;
        int sticky_history;
        int history_length;
        int show_indicators;
        int word_wrap;
        enum ellipsize ellipsize;
        int ignore_newline;
        int line_height;
        int notification_height;
        int separator_height;
        int padding;
        int h_padding;
        struct separator_color_data sep_color;
        int frame_width;
        char *frame_color;
        int startup_notification;
        int monitor;
        char *dmenu;
        char **dmenu_cmd;
        char *browser;
        char **browser_cmd;
        enum icon_position icon_position;
        enum vertical_alignment vertical_alignment;
        int min_icon_size;
        int max_icon_size;
        char *icon_path;
        enum follow_mode f_mode;
        bool always_run_script;
        struct keyboard_shortcut close_ks;
        struct keyboard_shortcut close_all_ks;
        struct keyboard_shortcut history_ks;
        struct keyboard_shortcut context_ks;
        bool force_xinerama;
        int corner_radius;
        enum mouse_action mouse_left_click;
        enum mouse_action mouse_middle_click;
        enum mouse_action mouse_right_click;
};

extern struct settings settings;

void load_settings(char *cmdline_config_path);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

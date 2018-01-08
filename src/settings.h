/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_SETTINGS_H
#define DUNST_SETTINGS_H

#include <stdbool.h>

#include "x11/x.h"

enum alignment { left, center, right };
enum ellipsize { start, middle, end };
enum icon_position_t { icons_left, icons_right, icons_off };
enum follow_mode { FOLLOW_NONE, FOLLOW_MOUSE, FOLLOW_KEYBOARD };
enum markup_mode { MARKUP_NULL, MARKUP_NO, MARKUP_STRIP, MARKUP_FULL };
enum separator_color { SEP_FOREGROUND, SEP_AUTO, SEP_FRAME, SEP_CUSTOM };
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

typedef struct _settings {
        bool print_notifications;
        bool per_monitor_dpi;
        enum markup_mode markup;
        bool stack_duplicates;
        bool hide_duplicate_count;
        char *font;
        char *normbgcolor;
        char *normfgcolor;
        char *normframecolor;
        char *critbgcolor;
        char *critfgcolor;
        char *critframecolor;
        char *lowbgcolor;
        char *lowfgcolor;
        char *lowframecolor;
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
        enum icon_position_t icon_position;
        int max_icon_size;
        char *icon_path;
        enum follow_mode f_mode;
        bool always_run_script;
        keyboard_shortcut close_ks;
        keyboard_shortcut close_all_ks;
        keyboard_shortcut history_ks;
        keyboard_shortcut context_ks;
        bool force_xinerama;
        int corner_radius;
} settings_t;

extern settings_t settings;

void load_settings(const char *cmdline_config_path);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

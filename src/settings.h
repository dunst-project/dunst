/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_SETTINGS_H
#define DUNST_SETTINGS_H

#include <stdbool.h>

#include "x11/x.h"

enum alignment { left, center, right };
enum icon_position_t { icons_left, icons_right, icons_off };
enum separator_color { FOREGROUND, AUTO, FRAME, CUSTOM };
enum follow_mode { FOLLOW_NONE, FOLLOW_MOUSE, FOLLOW_KEYBOARD };
enum markup_mode { MARKUP_NULL, MARKUP_NO, MARKUP_STRIP, MARKUP_FULL };

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
        int timeouts[3];
        char *icons[3];
        unsigned int transparency;
        char *geom;
        char *title;
        char *class;
        int shrink;
        int sort;
        int indicate_hidden;
        int idle_threshold;
        int show_age_threshold;
        enum alignment align;
        float bounce_freq;
        int sticky_history;
        int history_length;
        int show_indicators;
        int verbosity;
        int word_wrap;
        int ignore_newline;
        int line_height;
        int notification_height;
        int separator_height;
        int padding;
        int h_padding;
        enum separator_color sep_color;
        char *sep_custom_color_str;
        char *sep_color_str;
        int frame_width;
        char *frame_color;
        int startup_notification;
        int monitor;
        char *dmenu;
        char **dmenu_cmd;
        char *browser;
        enum icon_position_t icon_position;
        int max_icon_size;
        char *icon_folders;
        enum follow_mode f_mode;
        bool always_run_script;
        keyboard_shortcut close_ks;
        keyboard_shortcut close_all_ks;
        keyboard_shortcut history_ks;
        keyboard_shortcut context_ks;
} settings_t;

extern settings_t settings;

void load_settings(char *cmdline_config_path);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

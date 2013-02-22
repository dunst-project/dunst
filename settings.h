/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#pragma once

typedef struct _settings {
        bool print_notifications;
        char *font;
        char *normbgcolor;
        char *normfgcolor;
        char *critbgcolor;
        char *critfgcolor;
        char *lowbgcolor;
        char *lowfgcolor;
        char *format;
        int timeouts[3];
        unsigned int transparency;
        char *geom;
        int sort;
        int indicate_hidden;
        int idle_threshold;
        int show_age_threshold;
        enum alignment align;
        float bounce_freq;
        int sticky_history;
        int verbosity;
        int word_wrap;
        int ignore_newline;
        int line_height;
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
        enum follow_mode f_mode;
        keyboard_shortcut close_ks;
        keyboard_shortcut close_all_ks;
        keyboard_shortcut history_ks;
        keyboard_shortcut context_ks;
} settings_t;

extern settings_t settings;

void load_settings(char *cmdline_config_path);

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_SETTINGS_H
#define DUNST_SETTINGS_H

#include <stdbool.h>

#ifdef ENABLE_WAYLAND
#include "wayland/protocols/wlr-layer-shell-unstable-v1-client-header.h"
#endif

#include "notification.h"
#include "x11/x.h"

#define LIST_END (-1)

enum alignment { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };
enum vertical_alignment { VERTICAL_TOP, VERTICAL_CENTER, VERTICAL_BOTTOM };
enum separator_color { SEP_FOREGROUND, SEP_AUTO, SEP_FRAME, SEP_CUSTOM };
enum follow_mode { FOLLOW_NONE, FOLLOW_MOUSE, FOLLOW_KEYBOARD };
enum mouse_action { MOUSE_NONE, MOUSE_DO_ACTION, MOUSE_CLOSE_CURRENT,
        MOUSE_CLOSE_ALL, MOUSE_CONTEXT, MOUSE_CONTEXT_ALL, MOUSE_OPEN_URL,
        MOUSE_ACTION_END = LIST_END /* indicates the end of a list of mouse actions */};
#ifndef ZWLR_LAYER_SHELL_V1_LAYER_ENUM
#define ZWLR_LAYER_SHELL_V1_LAYER_ENUM
// Needed for compiling without wayland dependency
enum zwlr_layer_shell_v1_layer {
	ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND = 0,
	ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM = 1,
	ZWLR_LAYER_SHELL_V1_LAYER_TOP = 2,
	ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY = 3,
};
#endif /* ZWLR_LAYER_SHELL_V1_LAYER_ENUM */

#ifndef ZWLR_LAYER_SURFACE_V1_ANCHOR_ENUM
#define ZWLR_LAYER_SURFACE_V1_ANCHOR_ENUM
enum zwlr_layer_surface_v1_anchor {
	/**
	 * the top edge of the anchor rectangle
	 */
	ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP = 1,
	/**
	 * the bottom edge of the anchor rectangle
	 */
	ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM = 2,
	/**
	 * the left edge of the anchor rectangle
	 */
	ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT = 4,
	/**
	 * the right edge of the anchor rectangle
	 */
	ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT = 8,
};
#endif /* ZWLR_LAYER_SURFACE_V1_ANCHOR_ENUM */

enum origin_values {
        ORIGIN_TOP_LEFT = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT,
        ORIGIN_TOP_CENTER = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
        ORIGIN_TOP_RIGHT = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        ORIGIN_BOTTOM_LEFT = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT,
        ORIGIN_BOTTOM_CENTER = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
        ORIGIN_BOTTOM_RIGHT = ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        ORIGIN_LEFT_CENTER = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT,
        ORIGIN_RIGHT_CENTER = ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        ORIGIN_CENTER = 0,
};

// TODO make a TYPE_CMD, instead of using TYPE_PATH for settings like dmenu and browser
enum setting_type { TYPE_MIN = 0, TYPE_INT, TYPE_DOUBLE, TYPE_STRING,
        TYPE_PATH, TYPE_TIME, TYPE_LIST, TYPE_CUSTOM, TYPE_LENGTH,
        TYPE_DEPRECATED, TYPE_MAX = TYPE_DEPRECATED + 1 }; // to be implemented

struct separator_color_data {
        enum separator_color type;
        char *sep_color;
};

struct length {
        int min;
        int max;
};

struct position {
        int x;
        int y;
};

struct settings {
        bool print_notifications;
        bool per_monitor_dpi;
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
        int ignore_dbusclose;
        int ignore_newline;
        int line_height;
        int separator_height;
        int padding;
        int h_padding;
        int text_icon_padding;
        struct separator_color_data sep_color;
        int frame_width;
        char *frame_color;
        int startup_notification;
        int monitor;
        double scale;
        char *dmenu;
        char **dmenu_cmd;
        char *browser;
        char **browser_cmd;
        enum vertical_alignment vertical_alignment;
        char **icon_theme; // experimental
        bool enable_recursive_icon_lookup; // experimental
        bool enable_regex; // experimental
        char *icon_path;
        enum follow_mode f_mode;
        bool always_run_script;
        struct keyboard_shortcut close_ks;
        struct keyboard_shortcut close_all_ks;
        struct keyboard_shortcut history_ks;
        struct keyboard_shortcut context_ks;
        bool force_xinerama;
        bool force_xwayland;
        int corner_radius;
        enum mouse_action *mouse_left_click;
        enum mouse_action *mouse_middle_click;
        enum mouse_action *mouse_right_click;
        int progress_bar_height;
        int progress_bar_min_width;
        int progress_bar_max_width;
        int progress_bar_frame_width;
        int progress_bar_corner_radius;
        bool progress_bar;
        enum zwlr_layer_shell_v1_layer layer;
        enum origin_values origin;
        struct length width;
        int height;
        struct position offset;
        int notification_limit;
        int gap_size;
};

extern struct settings settings;

void load_settings(const char * const path);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

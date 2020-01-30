/* see example dunstrc for additional explanations about these options */

struct settings defaults = {

.font = "-*-terminus-medium-r-*-*-16-*-*-*-*-*-*-*",
.markup = MARKUP_NO,
.colors_norm.bg = "#1793D1",
.colors_norm.fg = "#DDDDDD",
.colors_crit.bg = "#ffaaaa",
.colors_crit.fg = "#000000",
.colors_low.bg = "#aaaaff",
.colors_low.fg = "#000000",
.format = "%s %b",         /* default format */

.timeouts = { S2US(10), S2US(10), S2US(0) }, /* low, normal, critical */
.icons = { "dialog-information", "dialog-information", "dialog-warning" }, /* low, normal, critical */

.transparency = 0,           /* transparency */
.geometry = { .x = 0,        /* geometry */
              .y = 0,
              .w = 0,
              .h = 0,
              .negative_x = 0,
              .negative_y = 0,
              .negative_width = 0,
              .width_set = 0
            },
.title = "Dunst",            /* the title of dunst notification windows */
.class = "Dunst",            /* the class of dunst notification windows */
.shrink = false,             /* shrinking */
.sort = true,                /* sort messages by urgency */
.indicate_hidden = true,     /* show count of hidden messages */
.idle_threshold = 0,         /* don't timeout notifications when idle for x seconds */
.show_age_threshold = -1,    /* show age of notification, when notification is older than x seconds */
.align = ALIGN_LEFT,         /* text alignment ALIGN_[LEFT|CENTER|RIGHT] */
.vertical_alignment = VERTICAL_CENTER,  /* vertical content alignment VERTICAL_[TOP|CENTER|BOTTOM] */
.sticky_history = true,
.history_length = 20,        /* max amount of notifications kept in history */
.show_indicators = true,
.word_wrap = false,
.ellipsize = ELLIPSE_MIDDLE,
.ignore_newline = false,
.line_height = 0,            /* if line height < font height, it will be raised to font height */
.notification_height = 0,    /* if notification height < font height and padding, it will be raised */
.corner_radius = 0,

.separator_height = 2,       /* height of the separator line between two notifications */
.padding = 0,
.h_padding = 0,              /* horizontal padding */
.sep_color = {SEP_AUTO},     /* SEP_AUTO, SEP_FOREGROUND, SEP_FRAME, SEP_CUSTOM */

.frame_width = 0,
.frame_color = "#888888",

/* show a notification on startup
 * This is mainly for crash detection since dbus restarts dunst
 * automatically after a crash, so crashes might get unnotices otherwise
 * */
.startup_notification = false,

/* monitor to display notifications on */
.monitor = 0,

/* path to dmenu */
.dmenu = "/usr/bin/dmenu",

.browser = "/usr/bin/firefox",

.min_icon_size = 0,
.max_icon_size = 0,

/* paths to default icons */
.icon_path = "/usr/share/icons/gnome/16x16/status/:/usr/share/icons/gnome/16x16/devices/",


/* follow focus to different monitor and display notifications there?
 * possible values:
 * FOLLOW_NONE
 * FOLLOW_MOUSE
 * FOLLOW_KEYBOARD
 *
 *  everything else than FOLLOW_NONE overrides 'monitor'
 */
.f_mode = FOLLOW_NONE,

/* keyboard shortcuts
 * use for example "ctrl+shift+space"
 * use "none" to disable
 */
.close_ks = {.str = "none",
        .code = 0,.sym = NoSymbol,.is_valid = false
},                              /* ignore this */

.close_all_ks = {.str = "none",
        .code = 0,.sym = NoSymbol,.is_valid = false
},                              /* ignore this */

.history_ks = {.str = "none",
        .code = 0,.sym = NoSymbol,.is_valid = false
},                              /* ignore this */

.context_ks = {.str = "none",
        .code = 0,.sym = NoSymbol,.is_valid = false
},                              /* ignore this */

.mouse_left_click = MOUSE_CLOSE_CURRENT,

.mouse_middle_click = MOUSE_DO_ACTION,

.mouse_right_click = MOUSE_CLOSE_ALL,

};

struct rule default_rules[] = {
        /* name can be any unique string. It is used to identify
         * the rule in dunstrc to override it there
         */

        /* an empty rule with no effect */
        {
                .name = "empty",
                .appname         = NULL,
                .summary         = NULL,
                .body            = NULL,
                .icon            = NULL,
                .category        = NULL,
                .msg_urgency     = -1,
                .timeout         = -1,
                .urgency         = -1,
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
        }
};

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

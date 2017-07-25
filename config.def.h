/* see example dunstrc for additional explanations about these options */

char *font = "-*-terminus-medium-r-*-*-16-*-*-*-*-*-*-*";
char *markup = "no";
char *normbgcolor = "#1793D1";
char *normfgcolor = "#DDDDDD";
char *critbgcolor = "#ffaaaa";
char *critfgcolor = "#000000";
char *lowbgcolor = "#aaaaff";
char *lowfgcolor = "#000000";
char *format = "%s %b";         /* default format */

int timeouts[] = { 10, 10, 0 }; /* low, normal, critical */
char *icons[] = { "info", "info", "emblem-important" }; /* low, normal, critical */

unsigned int transparency = 0;  /* transparency */
char *geom = "0x0";             /* geometry */
char *title = "Dunst";          /* the title of dunst notification windows */
char *class = "Dunst";          /* the class of dunst notification windows */
int shrink = false;             /* shrinking */
int sort = true;                /* sort messages by urgency */
int indicate_hidden = true;     /* show count of hidden messages */
int idle_threshold = 0;         /* don't timeout notifications when idle for x seconds */
int show_age_threshold = -1;    /* show age of notification, when notification is older than x seconds */
enum alignment align = left;    /* text alignment [left/center/right] */
int sticky_history = true;
int history_length = 20;          /* max amount of notifications kept in history */
int show_indicators = true;
int word_wrap = false;
int ignore_newline = false;
int line_height = 0;            /* if line height < font height, it will be raised to font height */
int notification_height = 0;    /* if notification height < font height and padding, it will be raised */
int min_notification_width = 0; /* if window width < min notification width, it will be raised */

int separator_height = 2;       /* height of the separator line between two notifications */
int padding = 0;
int h_padding = 0;              /* horizontal padding */
enum separator_color sep_color = AUTO;  /* AUTO, FOREGROUND, FRAME, CUSTOM */
char *sep_custom_color_str = NULL;      /* custom color if sep_color is set to CUSTOM */

int frame_width = 0;
char *frame_color = "#888888";
/* show a notification on startup
 * This is mainly for crash detection since dbus restarts dunst
 * automatically after a crash, so crashes might get unnotices otherwise
 * */
int startup_notification = false;

/* monitor to display notifications on */
int monitor = 0;

/* path to dmenu */
char *dmenu = "/usr/bin/dmenu";

char *browser = "/usr/bin/firefox";

int max_icon_size = 0;

/* paths to default icons */
char *icon_folders = "/usr/share/icons/gnome/16x16/status/:/usr/share/icons/gnome/16x16/devices/";

/* follow focus to different monitor and display notifications there?
 * possible values:
 * FOLLOW_NONE
 * FOLLOW_MOUSE
 * FOLLOW_KEYBOARD
 *
 *  everything else than FOLLOW_NONE overrides 'monitor'
 */
enum follow_mode f_mode = FOLLOW_NONE;

/* keyboard shortcuts
 * use for example "ctrl+shift+space"
 * use "none" to disable
 */
keyboard_shortcut close_ks = {.str = "none",
        .code = 0,.sym = NoSymbol,.is_valid = false
};                              /* ignore this */

keyboard_shortcut close_all_ks = {.str = "none",
        .code = 0,.sym = NoSymbol,.is_valid = false
};                              /* ignore this */

keyboard_shortcut history_ks = {.str = "none",
        .code = 0,.sym = NoSymbol,.is_valid = false
};                              /* ignore this */

keyboard_shortcut context_ks = {.str = "none",
        .code = 0,.sym = NoSymbol,.is_valid = false
};                              /* ignore this */

rule_t default_rules[] = {
        /* name can be any unique string. It is used to identify the rule in dunstrc to override it there */

        /*   name,    appname,        summary,         body,  icon, category, msg_urgency, timeout,  urgency,  markup,       history_ignore,  new_icon, fg,   bg,        format,  script */
        {    "empty", NULL,           NULL,            NULL,  NULL, NULL,     -1,          -1,       -1,       MARKUP_NULL,  false,           NULL,     NULL, NULL,      NULL,    NULL},
        /* { "rule1", "notify-send",  NULL,            NULL,  NULL, NULL,     -1,          -1,       -1,       MARKUP_NULL,  false,           NULL,     NULL, NULL,      "%s %b", NULL}, */
        /* { "rule2", "Pidgin",       "*says*,         NULL,  NULL, NULL,     -1,          -1,       CRITICAL, MARKUP_NULL,  false,           NULL,     NULL, NULL,      NULL,    NULL}, */
        /* { "rule3", "Pidgin",       "*signed on*",   NULL,  NULL, NULL,     -1,          -1,       LOW,      MARKUP_NULL,  false,           NULL,     NULL, NULL,      NULL,    NULL}, */
        /* { "rule4", "Pidgin",       "*signed off*",  NULL,  NULL, NULL,     -1,          -1,       LOW,      MARKUP_NULL,  false,           NULL,     NULL, NULL,      NULL,    NULL}, */
        /* { "rule5", NULL,           "*foobar*",      NULL,  NULL, NULL,     -1,          -1,       -1,       MARKUP_NULL,  false,           NULL,     NULL, "#00FF00", NULL,    NULL}, */
};

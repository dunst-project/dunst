/* see example dunstrc for additional explanations about these options */

char *font = "-*-terminus-medium-r-*-*-16-*-*-*-*-*-*-*";
char *normbgcolor = "#1793D1";
char *normfgcolor = "#DDDDDD";
char *critbgcolor = "#ffaaaa";
char *critfgcolor = "#000000";
char *lowbgcolor = "#aaaaff";
char *lowfgcolor = "#000000";
char *format = "%s %b";         /* default format */

int timeouts[] = { 10, 10, 0 }; /* low, normal, critical */

char *geom = "0x0";             /* geometry */
int sort = True;                /* sort messages by urgency */
int indicate_hidden = True;     /* show count of hidden messages */
int idle_threshold = 0;         /* don't timeout notifications when idle for x seconds */
int show_age_threshold = -1;    /* show age of notification, when notification is older than x seconds */
enum alignment align = left;    /* text alignment [left/center/right] */
int sticky_history = True;
int verbosity = 0;
int word_wrap = False;
int line_height = 0;   /* if line height < font height, it will be raised to font height */

int separator_enabled = False;
float separator_total_height = 1; /* height of the complete separator */
float separator_width = 0.7; /* width in relation to window width */
float separator_height = 0.1; /* height in relation to line_height */
enum separator_color sep_color = AUTO; /* AUTO or FOREGROUND */


/* monitor to display notifications on */
int monitor = 0;

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
                       .code = 0, .sym = NoSymbol,.is_valid = False}; /* ignore this */

keyboard_shortcut close_all_ks = {.str = "none",
                       .code = 0, .sym = NoSymbol,.is_valid = False}; /* ignore this */

keyboard_shortcut history_ks = {.str = "none",
                       .code = 0, .sym = NoSymbol,.is_valid = False}; /* ignore this */

rule_t default_rules[] = {
     /* name can be any unique string. It is used to identify the rule in dunstrc to override it there */

     /*   name,     appname,       summary,         body,  icon,  timeout,  urgency,  fg,    bg, format */
        { "empty", NULL,           NULL,            NULL,  NULL,  -1,       -1,       NULL,  NULL, NULL, },
     /* { "rule1", "notify-send",  NULL,            NULL,  NULL,  -1,       -1,       NULL,  NULL, "%s %b" }, */
     /* { "rule2", "Pidgin",       "*says*,         NULL,  NULL,  -1,       CRITICAL, NULL,  NULL, NULL    }, */
     /* { "rule3", "Pidgin",       "*signed on*",   NULL,  NULL,  -1,       LOW,      NULL,  NULL, NULL    }, */
     /* { "rule4", "Pidgin",       "*signed off*",  NULL,  NULL,  -1,       LOW,      NULL,  NULL, NULL    }, */
     /* { "rule5", NULL,           "*foobar*",      NULL,  NULL,  -1,       -1,       NULL,  "#00FF00", NULL, }, */
 };

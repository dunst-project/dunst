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

/* keyboard shortcuts */
keyboard_shortcut close_ks = {.str = "ctrl+space",
                       .code = 0, .sym = NoSymbol,.is_valid = False}; /* ignore this */

keyboard_shortcut close_all_ks = {.str = "ctrl+shift+space",
                       .code = 0, .sym = NoSymbol,.is_valid = False}; /* ignore this */

keyboard_shortcut history_ks = {.str = "ctrl+grave",
                       .code = 0, .sym = NoSymbol,.is_valid = False}; /* ignore this */

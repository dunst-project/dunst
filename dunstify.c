#include <glib.h>
#include <libnotify/notify.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

static gchar *appname = "dunstify";
static gchar *summary = NULL;
static gchar *body = NULL;
static NotifyUrgency urgency = NOTIFY_URGENCY_NORMAL;
static gchar *urgency_str = NULL;
static gchar **hint_strs = NULL;
static gchar **action_strs = NULL;
static gint timeout = NOTIFY_EXPIRES_DEFAULT;
static gchar *icon = NULL;
static gchar *raw_icon_path = NULL;
static gboolean capabilities = false;
static gboolean serverinfo = false;
static gboolean printid = false;
static guint32 replace_id = 0;
static guint32 close_id = 0;
static gboolean block = false;

static GOptionEntry entries[] =
{
    { "appname",      'a', 0, G_OPTION_ARG_STRING,       &appname,        "Name of your application", "NAME" },
    { "urgency",      'u', 0, G_OPTION_ARG_STRING,       &urgency_str,    "The urgency of this notification", "URG" },
    { "hints",        'h', 0, G_OPTION_ARG_STRING_ARRAY, &hint_strs,      "User specified hints", "HINT" },
    { "action",       'A', 0, G_OPTION_ARG_STRING_ARRAY, &action_strs,    "Actions the user can invoke", "ACTION" },
    { "timeout",      't', 0, G_OPTION_ARG_INT,          &timeout,        "The time in milliseconds until the notification expires", "TIMEOUT" },
    { "icon",         'i', 0, G_OPTION_ARG_STRING,       &icon,           "An Icon that should be displayed with the notification", "ICON" },
    { "raw_icon",     'I', 0, G_OPTION_ARG_STRING,       &raw_icon_path,  "Path to the icon to be sent as raw image data", "RAW_ICON"},
    { "capabilities", 'c', 0, G_OPTION_ARG_NONE,         &capabilities,   "Print the server capabilities and exit", NULL},
    { "serverinfo",   's', 0, G_OPTION_ARG_NONE,         &serverinfo,     "Print server information and exit", NULL},
    { "printid",      'p', 0, G_OPTION_ARG_NONE,         &printid,        "Print id, which can be used to update/replace this notification", NULL},
    { "replace",      'r', 0, G_OPTION_ARG_INT,          &replace_id,     "Set id of this notification.", "ID"},
    { "close",        'C', 0, G_OPTION_ARG_INT,          &close_id,       "Close the notification with the specified ID", "ID"},
    { "block",        'b', 0, G_OPTION_ARG_NONE,         &block,          "Block until notification is closed and print close reason", NULL},
    { NULL }
};

void die(int exit_value)
{
    if (notify_is_initted())
        notify_uninit();
    exit(exit_value);
}

void print_capabilities(void)
{
    GList *caps = notify_get_server_caps();
    for (GList *iter = caps; iter; iter = iter->next) {
        if (strlen(iter->data) > 0) {
            g_print("%s\n", (char *)iter->data);
        }
    }
}

void print_serverinfo(void)
{
    char *name;
    char *vendor;
    char *version;
    char *spec_version;

    if (!notify_get_server_info(&name, &vendor, &version, &spec_version)) {
        g_printerr("Unable to get server information");
        exit(1);
    }

    g_print("name:%s\nvendor:%s\nversion:%s\nspec_version:%s\n", name,
                                                                 vendor,
                                                                 version,
                                                                 spec_version);
}

/*
 * Glib leaves the option terminator "--" in the argv after parsing in some
 * cases. This function gets the specified argv element ignoring the first
 * terminator.
 *
 * See https://docs.gtk.org/glib/method.OptionContext.parse.html for details
 */
char *get_argv(char *argv[], int index)
{
    for (int i = 0; i <= index; i++) {
        if (strcmp(argv[i], "--") == 0) {
            return argv[index + 1];
        }
    }
    return argv[index];
}

/* Count the number of arguments in argv excluding the terminator "--" */
int count_args(char *argv[], int argc) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0)
            return argc - 1;
    }

    return argc;
}

void parse_commandline(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;

    context = g_option_context_new("SUMMARY BODY");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)){
        g_printerr("Invalid commandline: %s\n", error->message);
        exit(1);
    }

    g_option_context_free(context);

    if (capabilities) {
        print_capabilities();
        die(0);
    }

    if (serverinfo) {
        print_serverinfo();
        die(0);
    }

    if (*appname == '\0') {
        g_printerr("Provided appname was empty\n");
        die(1);
    }

    int n_args = count_args(argv, argc);
    if (n_args < 2 && close_id < 1) {
        g_printerr("I need at least a summary\n");
        die(1);
    } else if (n_args < 2) {
        summary = g_strdup("These are not the summaries you are looking for");
    } else {
        summary = g_strdup(get_argv(argv, 1));
    }

    if (n_args > 2) {
        body = g_strcompress(get_argv(argv, 2));
    }

    if (urgency_str) {
        switch (urgency_str[0]) {
            case 'l':
            case 'L':
            case '0':
                urgency = NOTIFY_URGENCY_LOW;
                break;
            case 'n':
            case 'N':
            case '1':
                urgency = NOTIFY_URGENCY_NORMAL;
                break;
            case 'c':
            case 'C':
            case '2':
                urgency = NOTIFY_URGENCY_CRITICAL;
                break;
            default:
                g_printerr("Unknown urgency: %s\n", urgency_str);
                g_printerr("Assuming normal urgency\n");
                break;
        }
    }
}

typedef struct _NotifyNotificationPrivate
{
        guint32         id;
        char           *app_name;
        char           *summary;
        char           *body;

        /* NULL to use icon data. Anything else to have server lookup icon */
        char           *icon_name;

        /*
         * -1   = use server default
         *  0   = never timeout
         *  > 0 = Number of milliseconds before we timeout
         */
        gint            timeout;

        GSList         *actions;
        GHashTable     *action_map;
        GHashTable     *hints;

        gboolean        has_nondefault_actions;
        gboolean        updates_pending;

        gulong          proxy_signal_handler;

        gint            closed_reason;
} knickers;

int get_id(NotifyNotification *n)
{
    knickers *kn = n->priv;

    /* I'm sorry for taking a peek */
    return kn->id;
}

void put_id(NotifyNotification *n, guint32 id)
{
    knickers *kn = n->priv;

    /* And know I'm putting stuff into
     * your knickers. I'm sorry.
     * I'm so sorry.
     * */

    kn->id = id;
}

void actioned(NotifyNotification *n, char *a, gpointer foo)
{
    notify_notification_close(n, NULL);
    g_print("%s\n", a);
    die(0);
}

void closed(NotifyNotification *n, gpointer foo)
{
    g_print("%d\n", notify_notification_get_closed_reason(n));
    die(0);
}

void add_action(NotifyNotification *n, char *str)
{
    char *action = str;
    char *label = strchr(str, ',');

    if (!label || *(label+1) == '\0') {
        g_printerr("Malformed action. Expected \"action,label\", got \"%s\"", str);
        return;
    }

    *label = '\0';
    label++;

    notify_notification_add_action(n, action, label, actioned, NULL, NULL);
}

void add_hint(NotifyNotification *n, char *str)
{
    char *type = str;
    char *name = strchr(str, ':');
    if (!name || *(name+1) == '\0') {
        g_printerr("Malformed hint. Expected \"type:name:value\", got \"%s\"", str);
        return;
    }
    *name = '\0';
    name++;
    char *value = strchr(name, ':');
    if (!value || *(value+1) == '\0') {
        g_printerr("Malformed hint. Expected \"type:name:value\", got \"%s\"", str);
        return;
    }
    *value = '\0';
    value++;

    if (strcmp(type, "int") == 0)
        notify_notification_set_hint_int32(n, name, atoi(value));
    else if (strcmp(type, "double") == 0)
        notify_notification_set_hint_double(n, name, atof(value));
    else if (strcmp(type, "string") == 0)
        notify_notification_set_hint_string(n, name, value);
    else if (strcmp(type, "byte") == 0) {
        gint h_byte = g_ascii_strtoull(value, NULL, 10);
        if (h_byte < 0 || h_byte > 0xFF)
            g_printerr("Not a byte: \"%s\"", value);
        else
            notify_notification_set_hint_byte(n, name, (guchar) h_byte);
    } else
        g_printerr("Malformed hint. Expected a type of int, double, string or byte, got %s\n", type);

}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    #if !GLIB_CHECK_VERSION(2,35,0)
        g_type_init();
    #endif
    parse_commandline(argc, argv);

    if (!notify_init(appname)) {
        g_printerr("Unable to initialize libnotify\n");
        die(1);
    }

    NotifyNotification *n;
    n = notify_notification_new(summary, body, icon);
    notify_notification_set_timeout(n, timeout);
    notify_notification_set_urgency(n, urgency);

    GError *err = NULL;

    if (raw_icon_path) {
            GdkPixbuf *raw_icon = gdk_pixbuf_new_from_file(raw_icon_path, &err);

            if(err) {
                g_printerr("Unable to get raw icon: %s\n", err->message);
                die(1);
            }

            notify_notification_set_image_from_pixbuf(n, raw_icon);
    }

    if (close_id > 0) {
        put_id(n, close_id);
        notify_notification_close(n, &err);
        if (err) {
            g_printerr("Unable to close notification: %s\n", err->message);
            die(1);
        }
        die(0);
    }

    if (replace_id > 0) {
        put_id(n, replace_id);
    }

    GMainLoop *l = NULL;

    if (block || action_strs) {
        l = g_main_loop_new(NULL, false);
        g_signal_connect(n, "closed", G_CALLBACK(closed), NULL);
    }

    if (action_strs)
        for (int i = 0; action_strs[i]; i++) {
            add_action(n, action_strs[i]);
        }

    if (hint_strs)
        for (int i = 0; hint_strs[i]; i++) {
            add_hint(n, hint_strs[i]);
        }


    notify_notification_show(n, &err);
    if (err) {
        g_printerr("Unable to send notification: %s\n", err->message);
        die(1);
    }

    if (printid)
        g_print("%d\n", get_id(n));

    if (block || action_strs)
        g_main_loop_run(l);

    g_object_unref(G_OBJECT (n));

    die(0);
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

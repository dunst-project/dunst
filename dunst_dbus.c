/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include <dbus/dbus.h>

#include "dunst.h"
#include "list.h"

#include "dunst_dbus.h"

DBusError dbus_err;
DBusConnection *dbus_conn;
dbus_uint32_t dbus_serial = 0;

static void _extract_basic(int type, DBusMessageIter * iter, void *target)
{
        int iter_type = dbus_message_iter_get_arg_type(iter);
        if (iter_type == type) {
                dbus_message_iter_get_basic(iter, target);
        }
}

static void
_extract_hint(const char *name, const char *hint_name,
              DBusMessageIter * hint, void *target)
{

        DBusMessageIter hint_value;

        if (!strcmp(hint_name, name)) {
                dbus_message_iter_next(hint);
                dbus_message_iter_recurse(hint, &hint_value);
                do {
                        dbus_message_iter_get_basic(&hint_value, target);
                } while (dbus_message_iter_next(hint));
        }
}

static const char *introspect = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<node name=\"/org/freedesktop/Notifications\">"
    "    <interface name=\"org.freedesktop.Notifications\">"
    "        "
    "        <method name=\"GetCapabilities\">"
    "            <arg direction=\"out\" name=\"capabilities\" type=\"as\"/>"
    "        </method>"
    "        <method name=\"Notify\">"
    "            <arg direction=\"in\" name=\"app_name\" type=\"s\"/>"
    "            <arg direction=\"in\" name=\"replaces_id\" type=\"u\"/>"
    "            <arg direction=\"in\" name=\"app_icon\" type=\"s\"/>"
    "            <arg direction=\"in\" name=\"summary\" type=\"s\"/>"
    "            <arg direction=\"in\" name=\"body\" type=\"s\"/>"
    "            <arg direction=\"in\" name=\"actions\" type=\"as\"/>"
    "            <arg direction=\"in\" name=\"hints\" type=\"a{sv}\"/>"
    "            <arg direction=\"in\" name=\"expire_timeout\" type=\"i\"/>"
    "            <arg direction=\"out\" name=\"id\" type=\"u\"/>"
    "        </method>"
    "        "
    "        <method name=\"CloseNotification\">"
    "            <arg direction=\"in\" name=\"id\" type=\"u\"/>"
    "        </method>"
    "        <method name=\"GetServerInformation\">"
    "            <arg direction=\"out\" name=\"name\" type=\"s\"/>"
    "            <arg direction=\"out\" name=\"vendor\" type=\"s\"/>"
    "            <arg direction=\"out\" name=\"version\" type=\"s\"/>"
    "            <arg direction=\"out\" name=\"spec_version\" type=\"s\"/>"
    "        </method>"
    "        <signal name=\"NotificationClosed\">"
    "            <arg name=\"id\" type=\"u\"/>"
    "            <arg name=\"reason\" type=\"u\"/>"
    "        </signal>"
    "        <signal name=\"ActionInvoked\">"
    "            <arg name=\"id\" type=\"u\"/>"
    "            <arg name=\"action_key\" type=\"s\"/>"
    "        </signal>"
    "    </interface>"
    "    "
    "    <interface name=\"org.xfce.Notifyd\">"
    "        <method name=\"Quit\"/>" "    </interface>" "</node>";

void dbus_introspect(DBusMessage * dmsg)
{
        DBusMessage *reply;
        DBusMessageIter args;

        reply = dbus_message_new_method_return(dmsg);

        dbus_message_iter_init_append(reply, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &introspect);
        dbus_connection_send(dbus_conn, reply, &dbus_serial);

        dbus_message_unref(reply);
}

void initdbus(void)
{
        int ret;
        dbus_error_init(&dbus_err);
        dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, &dbus_err);
        if (dbus_error_is_set(&dbus_err)) {
                fprintf(stderr, "Connection Error (%s)\n", dbus_err.message);
                dbus_error_free(&dbus_err);
        }
        if (dbus_conn == NULL) {
                fprintf(stderr, "dbus_con == NULL\n");
                exit(EXIT_FAILURE);
        }

        ret = dbus_bus_request_name(dbus_conn, "org.freedesktop.Notifications",
                                    DBUS_NAME_FLAG_REPLACE_EXISTING, &dbus_err);
        if (dbus_error_is_set(&dbus_err)) {
                fprintf(stderr, "Name Error (%s)\n", dbus_err.message);
        }
        if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
                fprintf(stderr,
                        "There's already another notification-daemon running\n");
                exit(EXIT_FAILURE);
        }

        dbus_bus_add_match(dbus_conn,
                           "type='signal',interface='org.freedesktop.Notifications'",
                           &dbus_err);
        if (dbus_error_is_set(&dbus_err)) {
                fprintf(stderr, "Match error (%s)\n", dbus_err.message);
                exit(EXIT_FAILURE);
        }
}

void dbus_poll(int timeout)
{
        DBusMessage *dbus_msg;

        dbus_connection_read_write(dbus_conn, timeout);

        dbus_msg = dbus_connection_pop_message(dbus_conn);
        /* we don't have a new message */
        if (dbus_msg == NULL) {
                return;
        }

        if (dbus_message_is_method_call
            (dbus_msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
                dbus_introspect(dbus_msg);
        }

        if (dbus_message_is_method_call(dbus_msg,
                                        "org.freedesktop.Notifications",
                                        "Notify")) {
                notify(dbus_msg);
        }
        if (dbus_message_is_method_call(dbus_msg,
                                        "org.freedesktop.Notifications",
                                        "GetCapabilities")) {
                getCapabilities(dbus_msg);
        }
        if (dbus_message_is_method_call(dbus_msg,
                                        "org.freedesktop.Notifications",
                                        "GetServerInformation")) {
                getServerInformation(dbus_msg);
        }
        if (dbus_message_is_method_call(dbus_msg,
                                        "org.freedesktop.Notifications",
                                        "CloseNotification")) {
                closeNotification(dbus_msg);
        }
        dbus_message_unref(dbus_msg);
}

void getCapabilities(DBusMessage * dmsg)
{
        DBusMessage *reply;
        DBusMessageIter args;
        DBusMessageIter subargs;

        const char *caps[1] = { "body" };
        dbus_serial++;

        reply = dbus_message_new_method_return(dmsg);
        if (!reply) {
                return;
        }

        dbus_message_iter_init_append(reply, &args);

        if (!dbus_message_iter_open_container
            (&args, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &subargs)
            || !dbus_message_iter_append_basic(&subargs, DBUS_TYPE_STRING, caps)
            || !dbus_message_iter_close_container(&args, &subargs)
            || !dbus_connection_send(dbus_conn, reply, &dbus_serial)) {
                fprintf(stderr, "Unable to reply");
                return;
        }

        dbus_connection_flush(dbus_conn);
        dbus_message_unref(reply);
}

void closeNotification(DBusMessage * dmsg)
{
        DBusMessage *reply;
        DBusMessageIter args;
        int id = 0;

        reply = dbus_message_new_method_return(dmsg);
        if (!reply) {
                return;
        }
        dbus_message_iter_init(dmsg, &args);

        _extract_basic(DBUS_TYPE_UINT32, &args, &id);

        close_notification_by_id(id, 3);

        dbus_connection_send(dbus_conn, reply, &dbus_serial);
        dbus_connection_flush(dbus_conn);
}

void getServerInformation(DBusMessage * dmsg)
{
        DBusMessage *reply;
        DBusMessageIter args;
        char *param = "";
        const char *info[4] = { "dunst", "dunst", "2011", "2011" };

        if (!dbus_message_iter_init(dmsg, &args)) {
        } else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) {
        } else {
                dbus_message_iter_get_basic(&args, &param);
        }

        reply = dbus_message_new_method_return(dmsg);

        dbus_message_iter_init_append(reply, &args);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[0])
            || !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING,
                                               &info[1])
            || !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING,
                                               &info[2])
            || !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING,
                                               &info[3])) {
                fprintf(stderr, "Unable to fill arguments");
                return;
        }

        dbus_serial++;
        if (!dbus_connection_send(dbus_conn, reply, &dbus_serial)) {
                fprintf(stderr, "Out Of Memory!\n");
                exit(EXIT_FAILURE);
        }
        dbus_connection_flush(dbus_conn);

        dbus_message_unref(reply);
}

void notificationClosed(notification * n, int reason)
{
        DBusMessage *dmsg;
        DBusMessageIter args;
        int id;

        if (n == NULL) {
                return;
        }

        id = n->id;

        dmsg =
            dbus_message_new_signal("/org/freedesktop/Notifications",
                                    "org.freedesktop.Notifications",
                                    "NotificationClosed");
        dbus_message_iter_init_append(dmsg, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &id);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &reason);

        dbus_message_set_destination(dmsg, n->dbus_client);

        dbus_connection_send(dbus_conn, dmsg, &dbus_serial);

        dbus_message_unref(dmsg);

        dbus_connection_flush(dbus_conn);
}

void notify(DBusMessage * dmsg)
{
        DBusMessage *reply;
        DBusMessageIter args;
        DBusMessageIter hints;
        DBusMessageIter hint;
        char *hint_name;

        int i;
        int id;
        const char *appname = NULL;
        const char *summary = NULL;
        const char *body = NULL;
        const char *icon = NULL;
        const char *fgcolor = NULL;
        const char *bgcolor = NULL;
        int urgency = 1;
        notification *n = malloc(sizeof(notification));
        dbus_uint32_t replaces_id = 0;
        dbus_int32_t expires = -1;

        dbus_serial++;
        dbus_message_iter_init(dmsg, &args);

        _extract_basic(DBUS_TYPE_STRING, &args, &appname);

        dbus_message_iter_next(&args);
        _extract_basic(DBUS_TYPE_UINT32, &args, &replaces_id);

        dbus_message_iter_next(&args);
        _extract_basic(DBUS_TYPE_STRING, &args, &icon);

        dbus_message_iter_next(&args);
        _extract_basic(DBUS_TYPE_STRING, &args, &summary);

        dbus_message_iter_next(&args);
        _extract_basic(DBUS_TYPE_STRING, &args, &body);

        dbus_message_iter_next(&args);
        dbus_message_iter_next(&args);

        dbus_message_iter_recurse(&args, &hints);
        dbus_message_iter_next(&args);

        _extract_basic(DBUS_TYPE_INT32, &args, &expires);

        while (dbus_message_iter_get_arg_type(&hints) != DBUS_TYPE_INVALID) {
                dbus_message_iter_recurse(&hints, &hint);
                while (dbus_message_iter_get_arg_type(&hint) !=
                       DBUS_TYPE_INVALID) {
                        if (dbus_message_iter_get_arg_type(&hint) !=
                            DBUS_TYPE_STRING) {
                                dbus_message_iter_next(&hint);
                                continue;
                        }
                        dbus_message_iter_get_basic(&hint, &hint_name);
                        _extract_hint("urgency", hint_name, &hint, &urgency);
                        _extract_hint("fgcolor", hint_name, &hint, &fgcolor);
                        _extract_hint("bgcolor", hint_name, &hint, &bgcolor);
                        dbus_message_iter_next(&hint);
                }
                dbus_message_iter_next(&hints);
        }

        if (expires > 0) {
                /* do some rounding */
                expires = (expires + 500) / 1000;
                if (expires < 1) {
                        expires = 1;
                }
        }
        n->appname = appname != NULL ? strdup(appname) : "";
        n->summary = summary != NULL ? strdup(summary) : "";
        n->body = body != NULL ? strdup(body) : "";
        n->icon = icon != NULL ? strdup(icon) : "";
        n->timeout = expires;
        n->urgency = urgency;
        n->dbus_client = strdup(dbus_message_get_sender(dmsg));
        for (i = 0; i < ColLast; i++) {
                n->color_strings[i] = NULL;
        }
        n->color_strings[ColFG] = fgcolor == NULL ? NULL : strdup(fgcolor);
        n->color_strings[ColBG] = bgcolor == NULL ? NULL : strdup(bgcolor);
        id = init_notification(n, replaces_id);
        map_win();

        reply = dbus_message_new_method_return(dmsg);

        dbus_message_iter_init_append(reply, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &id);
        dbus_connection_send(dbus_conn, reply, &dbus_serial);

        dbus_message_unref(reply);
}

/* vim: set ts=8 sw=8 tw=0: */

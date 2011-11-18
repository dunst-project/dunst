#include <dbus/dbus.h>

#include "dunst.h"

#define DBUS_POLL_TIMEOUT 1000

DBusError dbus_err;
DBusConnection *dbus_conn;
dbus_uint32_t dbus_serial = 0;


void
initdbus(void) {
    int ret;
    dbus_error_init(&dbus_err);
    dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, &dbus_err);
    if(dbus_error_is_set(&dbus_err)) {
        fprintf(stderr, "Connection Error (%s)\n", dbus_err.message);
        dbus_error_free(&dbus_err);
    }
    if(dbus_conn == NULL) {
        fprintf(stderr, "dbus_con == NULL\n");
        exit(EXIT_FAILURE);
    }

    ret = dbus_bus_request_name(dbus_conn, "org.freedesktop.Notifications",
            DBUS_NAME_FLAG_REPLACE_EXISTING, &dbus_err);
    if(dbus_error_is_set(&dbus_err)) {
        fprintf(stderr, "Name Error (%s)\n", dbus_err.message);
    }
    if(DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
        fprintf(stderr, "There's already another notification-daemon running\n");
        exit(EXIT_FAILURE);
    }

    dbus_bus_add_match(dbus_conn,
            "type='signal',interface='org.freedesktop.Notifications'",
            &dbus_err);
    if(dbus_error_is_set(&dbus_err)) {
        fprintf(stderr, "Match error (%s)\n", dbus_err.message);
        exit(EXIT_FAILURE);
    }
}

void
dbus_poll(void) {
    DBusMessage *dbus_msg;

    /* make timeout smaller if we are displaying a message
     * to improve responsivness for mouse clicks
     */
    if(msgqueue == NULL) {
        dbus_connection_read_write(dbus_conn, DBUS_POLL_TIMEOUT);
    } else {
        dbus_connection_read_write(dbus_conn, 100);
    }

    dbus_msg = dbus_connection_pop_message(dbus_conn);
    /* we don't have a new message */
    if(dbus_msg == NULL) {
        return;
    }

    if(dbus_message_is_method_call(dbus_msg,
                "org.freedesktop.Notifications","Notify")) {
        notify(dbus_msg);
    }
    if(dbus_message_is_method_call(dbus_msg,
                "org.freedesktop.Notifications", "GetCapabilities")) {
        getCapabilities(dbus_msg);
    }
    if(dbus_message_is_method_call(dbus_msg,
                "org.freedesktop.Notifications", "GetServerInformation")) {
        getServerInformation(dbus_msg);
    }
    if(dbus_message_is_method_call(dbus_msg,
                "org.freedesktop.Notifications", "CloseNotification")) {
        closeNotification(dbus_msg);
    }
    dbus_message_unref(dbus_msg);
}

void
getCapabilities(DBusMessage *dmsg) {
    DBusMessage* reply;
    DBusMessageIter args;
    DBusMessageIter subargs;

    const char *caps[1] = {"body"};
    dbus_serial++;

    reply = dbus_message_new_method_return(dmsg);
    if(!reply) {
        return;
    }

    dbus_message_iter_init_append(reply, &args);

    if(!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &subargs )
     || !dbus_message_iter_append_basic(&subargs, DBUS_TYPE_STRING, caps)
     || !dbus_message_iter_close_container(&args, &subargs)
     || !dbus_connection_send(dbus_conn, reply, &dbus_serial)) {
        fprintf(stderr, "Unable to reply");
        return;
    }

    dbus_connection_flush(dbus_conn);
    dbus_message_unref(reply);
}

void
closeNotification(DBusMessage *dmsg) {
    DBusMessage *reply;
    reply = dbus_message_new_method_return(dmsg);
    if(!reply) {
        return;
    }
    dbus_connection_send(dbus_conn, reply, &dbus_serial);
    dbus_connection_flush(dbus_conn);
}

void
getServerInformation(DBusMessage *dmsg) {
    DBusMessage *reply;
    DBusMessageIter args;
    char *param = "";
    const char *info[4] = {"dunst", "dunst", "2011", "2011"};


    if (!dbus_message_iter_init(dmsg, &args)) {
    }
    else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) {
    }
    else {
       dbus_message_iter_get_basic(&args, &param);
    }



    reply = dbus_message_new_method_return(dmsg);

    dbus_message_iter_init_append(reply, &args);
    if(!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[0])
    || !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[1])
    || !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[2])
    || !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[3])) {
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


static void
_extract_basic(int type, DBusMessageIter *iter, void *target) {
    int iter_type = dbus_message_iter_get_arg_type(iter);
    if (iter_type != type) {
        dunst_printf(DEBUG, "Invalid dbus notification: expected type %d but got %d.\n",
                type, iter_type);
    } else {
        dbus_message_iter_get_basic(iter, target);
    }
}

static void
_extract_hint(const char *name, const char *hint_name,
        DBusMessageIter *hint, void *target) {

    DBusMessageIter hint_value;

    if(!strcmp(hint_name, name)) {
        dunst_printf(DEBUG, "%s found\n", name);
        dbus_message_iter_next(hint);
        dbus_message_iter_recurse(hint, &hint_value);
        do {
            dbus_message_iter_get_basic(&hint_value, target);
        } while(dbus_message_iter_next(hint));
    }
}

void
notify(DBusMessage *dmsg) {
    DBusMessage *reply;
    DBusMessageIter args;
    DBusMessageIter hints;
    DBusMessageIter hint;
    char *hint_name;

    int i;
    int id = 23;
    const char *appname = NULL;
    const char *summary = NULL;
    const char *body = NULL;
    const char *icon = NULL;
    const char *fgcolor = NULL;
    const char *bgcolor = NULL;
    int urgency = 1;
    msg_queue_t *msg = malloc(sizeof(msg_queue_t));
    dbus_uint32_t nid=0;
    dbus_int32_t expires=-1;

    dbus_serial++;
    dunst_printf(DEBUG, "new dbus message\n");
    dbus_message_iter_init(dmsg, &args);

    dunst_printf(DEBUG, "extracting appname\n");
    _extract_basic(DBUS_TYPE_STRING, &args, &appname);

    dbus_message_iter_next( &args );
    dunst_printf(DEBUG, "extracting nid\n");
    _extract_basic(DBUS_TYPE_UINT32, &args, &nid);

    dbus_message_iter_next( &args );
    dunst_printf(DEBUG, "extracting icon\n");
    _extract_basic(DBUS_TYPE_STRING, &args, &icon);

    dbus_message_iter_next( &args );
    dunst_printf(DEBUG, "extracting summary\n");
    _extract_basic(DBUS_TYPE_STRING, &args, &summary);

    dbus_message_iter_next( &args );
    dunst_printf(DEBUG, "extracting body\n");
    _extract_basic(DBUS_TYPE_STRING, &args, &body);

    dbus_message_iter_next( &args );
    dbus_message_iter_next( &args );

    dunst_printf(DEBUG, "extracting hints\n");
    dbus_message_iter_recurse(&args, &hints);
    dbus_message_iter_next( &args );

    dunst_printf(DEBUG, "extracting expires\n");
    _extract_basic(DBUS_TYPE_INT32, &args, &expires);


    dunst_printf(DEBUG, "extracting hints\n");
    do {
        dbus_message_iter_recurse(&hints, &hint);
        do {
            if(dbus_message_iter_get_arg_type(&hint) != DBUS_TYPE_STRING) {
                continue;
            }
            dbus_message_iter_get_basic(&hint, &hint_name);
            _extract_hint("urgency", hint_name, &hint, &urgency);
            _extract_hint("fgcolor", hint_name, &hint, &fgcolor);
            _extract_hint("bgcolor", hint_name, &hint, &bgcolor);
        } while(dbus_message_iter_next(&hint));
    } while(dbus_message_iter_next(&hints));


    if(expires > 0) {
        /* do some rounding */
        expires = (expires+500)/1000;
        if(expires < 1) {
            expires = 1;
        }
    }
    msg->appname = strdup(appname);
    msg->summary = strdup(summary);
    msg->body = strdup(body);
    msg->icon = strdup(icon);
    msg->timeout = expires;
    msg->urgency = urgency;
    for(i = 0; i < ColLast; i++) {
        msg->color_strings[i] = NULL;
    }
    msg->color_strings[ColFG] = fgcolor == NULL ? NULL : strdup(fgcolor);
    msg->color_strings[ColBG] = bgcolor == NULL ? NULL : strdup(bgcolor);
    msgqueue = append(msgqueue, msg);
    drawmsg();

    reply = dbus_message_new_method_return(dmsg);

    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &id);
    dbus_connection_send(dbus_conn, reply, &dbus_serial);

    dbus_message_unref(reply);
}

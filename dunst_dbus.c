#include <dbus/dbus.h>

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
    fprintf(stderr, "closeNotification to be implemented\n");
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


void
notify(DBusMessage *dmsg) {
    DBusMessage *reply;
    DBusMessageIter args;
    DBusMessageIter hints;
    DBusMessageIter hint;
    DBusMessageIter hint_value;
    char *hint_name;

    int id = 23;
    const char *appname;
    const char *summary;
    const char *body;
    const char *icon;
    int urgency = 1;
    char *msg;
    dbus_uint32_t nid=0;
    dbus_int32_t expires=-1;

    dbus_serial++;
    dbus_message_iter_init(dmsg, &args);
    dbus_message_iter_get_basic(&args, &appname);
    dbus_message_iter_next( &args );
    dbus_message_iter_get_basic(&args, &nid);
    dbus_message_iter_next( &args );
    dbus_message_iter_get_basic(&args, &icon);
    dbus_message_iter_next( &args );
    dbus_message_iter_get_basic(&args, &summary);
    dbus_message_iter_next( &args );
    dbus_message_iter_get_basic(&args, &body);
    dbus_message_iter_next( &args );
    dbus_message_iter_next( &args );
    dbus_message_iter_recurse(&args, &hints);
    dbus_message_iter_next( &args );
    dbus_message_iter_get_basic(&args, &expires);

    do {
        dbus_message_iter_recurse(&hints, &hint);
        do {
            /* 115 == dbus urgency type thingy... i hate this shit */
            if(dbus_message_iter_get_arg_type(&hint) != 115) {
                continue;
            }
            dbus_message_iter_get_basic(&hint, &hint_name);
            if(!strcmp(hint_name, "urgency")) {
                dbus_message_iter_next(&hint);
                dbus_message_iter_recurse(&hint, &hint_value);
                do {
                    dbus_message_iter_get_basic(&hint_value, &urgency);
                } while(dbus_message_iter_next(&hint));

            }
        } while(dbus_message_iter_next(&hint));
    } while(dbus_message_iter_next(&hints));





    msg = string_replace("%a", appname, strdup(format));
    msg = string_replace("%s", summary, msg);
    msg = string_replace("%i", icon, msg);
    msg = string_replace("%I", basename(icon), msg);
    msg = string_replace("%b", body, msg);


    if(expires > 0) {
        expires = expires/1000;
    }
    msgqueue = append(msgqueue, msg, expires, urgency);
    drawmsg();

    reply = dbus_message_new_method_return(dmsg);

    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &id);
    dbus_connection_send(dbus_conn, reply, &dbus_serial);
    dbus_message_unref(reply);
}

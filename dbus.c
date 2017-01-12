/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <gio/gio.h>
#include "dunst.h"
#include "dbus.h"
#include "notification.h"
#include "utils.h"
#include "settings.h"

GDBusConnection *dbus_conn;

static GDBusNodeInfo *introspection_data = NULL;

static const char *introspection_xml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<node name=\"/org/freedesktop/Notifications\">"
    "    <interface name=\"org.freedesktop.Notifications\">"

    "        <method name=\"GetCapabilities\">"
    "            <arg direction=\"out\" name=\"capabilities\"    type=\"as\"/>"
    "        </method>"

    "        <method name=\"Notify\">"
    "            <arg direction=\"in\"  name=\"app_name\"        type=\"s\"/>"
    "            <arg direction=\"in\"  name=\"replaces_id\"     type=\"u\"/>"
    "            <arg direction=\"in\"  name=\"app_icon\"        type=\"s\"/>"
    "            <arg direction=\"in\"  name=\"summary\"         type=\"s\"/>"
    "            <arg direction=\"in\"  name=\"body\"            type=\"s\"/>"
    "            <arg direction=\"in\"  name=\"actions\"         type=\"as\"/>"
    "            <arg direction=\"in\"  name=\"hints\"           type=\"a{sv}\"/>"
    "            <arg direction=\"in\"  name=\"expire_timeout\"  type=\"i\"/>"
    "            <arg direction=\"out\" name=\"id\"              type=\"u\"/>"
    "        </method>"

    "        <method name=\"CloseNotification\">"
    "            <arg direction=\"in\"  name=\"id\"              type=\"u\"/>"
    "        </method>"

    "        <method name=\"GetServerInformation\">"
    "            <arg direction=\"out\" name=\"name\"            type=\"s\"/>"
    "            <arg direction=\"out\" name=\"vendor\"          type=\"s\"/>"
    "            <arg direction=\"out\" name=\"version\"         type=\"s\"/>"
    "            <arg direction=\"out\" name=\"spec_version\"    type=\"s\"/>"
    "        </method>"

    "        <signal name=\"NotificationClosed\">"
    "            <arg name=\"id\"         type=\"u\"/>"
    "            <arg name=\"reason\"     type=\"u\"/>"
    "        </signal>"

    "        <signal name=\"ActionInvoked\">"
    "            <arg name=\"id\"         type=\"u\"/>"
    "            <arg name=\"action_key\" type=\"s\"/>"
    "        </signal>"
    "   </interface>"
    "</node>";

static void onGetCapabilities(GDBusConnection * connection,
                              const gchar * sender,
                              const GVariant * parameters,
                              GDBusMethodInvocation * invocation);
static void onNotify(GDBusConnection * connection,
                     const gchar * sender,
                     GVariant * parameters, GDBusMethodInvocation * invocation);
static void onCloseNotification(GDBusConnection * connection,
                                const gchar * sender,
                                GVariant * parameters,
                                GDBusMethodInvocation * invocation);
static void onGetServerInformation(GDBusConnection * connection,
                                   const gchar * sender,
                                   const GVariant * parameters,
                                   GDBusMethodInvocation * invocation);

void handle_method_call(GDBusConnection * connection,
                        const gchar * sender,
                        const gchar * object_path,
                        const gchar * interface_name,
                        const gchar * method_name,
                        GVariant * parameters,
                        GDBusMethodInvocation * invocation, gpointer user_data)
{
        if (g_strcmp0(method_name, "GetCapabilities") == 0) {
                onGetCapabilities(connection, sender, parameters, invocation);
        } else if (g_strcmp0(method_name, "Notify") == 0) {
                onNotify(connection, sender, parameters, invocation);
        } else if (g_strcmp0(method_name, "CloseNotification") == 0) {
                onCloseNotification(connection, sender, parameters, invocation);
        } else if (g_strcmp0(method_name, "GetServerInformation") == 0) {
                onGetServerInformation(connection, sender, parameters,
                                       invocation);
        } else {
                printf("WARNING: sender: %s; unknown method_name: %s\n", sender,
                       method_name);
        }
}

static void onGetCapabilities(GDBusConnection * connection,
                              const gchar * sender,
                              const GVariant * parameters,
                              GDBusMethodInvocation * invocation)
{
        GVariantBuilder *builder;
        GVariant *value;

        builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        g_variant_builder_add(builder, "s", "actions");
        g_variant_builder_add(builder, "s", "body");
        g_variant_builder_add(builder, "s", "body-markup");
        value = g_variant_new("(as)", builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, value);

        g_dbus_connection_flush(connection, NULL, NULL, NULL);
        g_variant_unref(value);
}

static void onNotify(GDBusConnection * connection,
                     const gchar * sender,
                     GVariant * parameters, GDBusMethodInvocation * invocation)
{

        gchar *appname = NULL;
        guint replaces_id = 0;
        gchar *icon = NULL;
        gchar *summary = NULL;
        gchar *body = NULL;
        Actions *actions = malloc(sizeof(Actions));
        if(actions == NULL) {
                die("Unable to allocate memory", EXIT_FAILURE);
        }
        gint timeout = -1;

        /* hints */
        gint urgency = 1;
        gint progress = -1;
        gchar *fgcolor = NULL;
        gchar *bgcolor = NULL;
        gchar *category = NULL;

        actions->actions = NULL;
        actions->count = 0;

        {
                GVariantIter *iter = g_variant_iter_new(parameters);
                GVariant *content;
                GVariant *dict_value;
                int idx = 0;
                while ((content = g_variant_iter_next_value(iter))) {

                        switch (idx) {
                        case 0:
                                if (g_variant_is_of_type
                                    (content, G_VARIANT_TYPE_STRING))
                                        appname =
                                            g_variant_dup_string(content, NULL);
                                break;
                        case 1:
                                if (g_variant_is_of_type
                                    (content, G_VARIANT_TYPE_UINT32))
                                        replaces_id =
                                            g_variant_get_uint32(content);
                                break;
                        case 2:
                                if (g_variant_is_of_type
                                    (content, G_VARIANT_TYPE_STRING))
                                        icon =
                                            g_variant_dup_string(content, NULL);
                                break;
                        case 3:
                                if (g_variant_is_of_type
                                    (content, G_VARIANT_TYPE_STRING))
                                        summary =
                                            g_variant_dup_string(content, NULL);
                                break;
                        case 4:
                                if (g_variant_is_of_type
                                    (content, G_VARIANT_TYPE_STRING))
                                        body =
                                            g_variant_dup_string(content, NULL);
                                break;
                        case 5:
                                if (g_variant_is_of_type
                                    (content, G_VARIANT_TYPE_STRING_ARRAY))
                                        actions->actions =
                                            g_variant_dup_strv(content,
                                                               &(actions->
                                                                 count));
                                break;
                        case 6:
                                if (g_variant_is_of_type
                                    (content, G_VARIANT_TYPE_DICTIONARY)) {

                                        dict_value =
                                            g_variant_lookup_value(content,
                                                                   "urgency",
                                                                   G_VARIANT_TYPE_BYTE);
                                        if (dict_value)
                                                urgency =
                                                    g_variant_get_byte
                                                    (dict_value);

                                        dict_value =
                                            g_variant_lookup_value(content,
                                                                   "fgcolor",
                                                                   G_VARIANT_TYPE_STRING);
                                        if (dict_value)
                                                fgcolor =
                                                    g_variant_dup_string
                                                    (dict_value, NULL);

                                        dict_value =
                                            g_variant_lookup_value(content,
                                                                   "bgcolor",
                                                                   G_VARIANT_TYPE_STRING);
                                        if (dict_value)
                                                bgcolor =
                                                    g_variant_dup_string
                                                    (dict_value, NULL);

                                        dict_value =
                                                g_variant_lookup_value(content,
                                                                "category",
                                                                G_VARIANT_TYPE_STRING);

                                        if (dict_value) {
                                                category =
                                                        g_variant_dup_string(
                                                                        dict_value, NULL);
                                        }

                                        dict_value =
                                                g_variant_lookup_value(content,
                                                                "value",
                                                                G_VARIANT_TYPE_INT32);

                                        if (dict_value) {
                                                progress =
                                                        g_variant_get_int32(dict_value);
                                        } else {
                                                dict_value =
                                                        g_variant_lookup_value(content,
                                                                        "value",
                                                                        G_VARIANT_TYPE_UINT32);

                                                if (dict_value)
                                                        progress =
                                                                g_variant_get_uint32(dict_value);
                                        }
                                }
                                break;
                        case 7:
                                if (g_variant_is_of_type
                                    (content, G_VARIANT_TYPE_INT32))
                                        timeout = g_variant_get_int32(content);
                                break;
                        }
                        g_variant_unref(content);
                        idx++;
                }

                g_variant_iter_free(iter);
        }

        fflush(stdout);

        if (timeout > 0) {
                /* do some rounding */
                timeout = (timeout + 500) / 1000;
                if (timeout < 1) {
                        timeout = 1;
                }
        }

        notification *n = malloc(sizeof(notification));
        if(n == NULL) {
                die("Unable to allocate memory", EXIT_FAILURE);
        }
        n->appname = appname;
        n->summary = summary;
        n->body = body;
        n->icon = icon;
        n->timeout = timeout;
        n->allow_markup = settings.allow_markup;
        n->plain_text = settings.plain_text;
        n->progress = (progress < 0 || progress > 100) ? 0 : progress + 1;
        n->urgency = urgency;
        n->category = category;
        n->dbus_client = strdup(sender);
        if (actions->count > 0) {
                n->actions = actions;
        } else {
                n->actions = NULL;
                g_strfreev(actions->actions);
                free(actions);
        }

        for (int i = 0; i < ColLast; i++) {
                n->color_strings[i] = NULL;
        }
        n->color_strings[ColFG] = fgcolor;
        n->color_strings[ColBG] = bgcolor;

        int id = notification_init(n, replaces_id);
        wake_up();

        GVariant *reply = g_variant_new("(u)", id);
        g_dbus_method_invocation_return_value(invocation, reply);
        g_dbus_connection_flush(connection, NULL, NULL, NULL);

        run(NULL);
}

static void onCloseNotification(GDBusConnection * connection,
                                const gchar * sender,
                                GVariant * parameters,
                                GDBusMethodInvocation * invocation)
{
        guint32 id;
        g_variant_get(parameters, "(u)", &id);
        notification_close_by_id(id, 3);
        g_dbus_method_invocation_return_value(invocation, NULL);
        g_dbus_connection_flush(connection, NULL, NULL, NULL);
}

static void onGetServerInformation(GDBusConnection * connection,
                                   const gchar * sender,
                                   const GVariant * parameters,
                                   GDBusMethodInvocation * invocation)
{
        GVariant *value;

        value = g_variant_new("(ssss)", "dunst", "knopwob", VERSION, "1.2");
        g_dbus_method_invocation_return_value(invocation, value);

        g_dbus_connection_flush(connection, NULL, NULL, NULL);
}

void notificationClosed(notification * n, int reason)
{
        if (!dbus_conn) {
                printf("DEBUG: notificationClosed but not (yet) connected\n");
                return;
        }

        GVariant *body = g_variant_new("(uu)", n->id, reason);
        GError *err = NULL;

        g_dbus_connection_emit_signal(dbus_conn,
                                      n->dbus_client,
                                      "/org/freedesktop/Notifications",
                                      "org.freedesktop.Notifications",
                                      "NotificationClosed", body, &err);

        if (err) {
                printf("notificationClosed ERROR\n");
        }

}

void actionInvoked(notification * n, const char *identifier)
{
        GVariant *body = g_variant_new("(us)", n->id, identifier);
        GError *err = NULL;

        g_dbus_connection_emit_signal(dbus_conn,
                                      n->dbus_client,
                                      "/org/freedesktop/Notifications",
                                      "org.freedesktop.Notifications",
                                      "ActionInvoked", body, &err);

        if (err) {
                printf("ActionInvoked ERROR\n");
        }
}

static const GDBusInterfaceVTable interface_vtable = {
        handle_method_call
};

static void on_bus_acquired(GDBusConnection * connection,
                            const gchar * name, gpointer user_data)
{
        guint registration_id;

        registration_id = g_dbus_connection_register_object(connection,
                                                            "/org/freedesktop/Notifications",
                                                            introspection_data->
                                                            interfaces[0],
                                                            &interface_vtable,
                                                            NULL, NULL, NULL);

        if (registration_id <= 0) {
                fprintf(stderr, "Unable to register\n");
                exit(1);
        }
}

static void on_name_acquired(GDBusConnection * connection,
                             const gchar * name, gpointer user_data)
{
        dbus_conn = connection;
}

static void on_name_lost(GDBusConnection * connection,
                         const gchar * name, gpointer user_data)
{
        fprintf(stderr, "Name Lost. Is Another notification daemon running?\n");
        exit(1);
}

int initdbus(void)
{
        guint owner_id;

        #if !GLIB_CHECK_VERSION(2,35,0)
                g_type_init();
        #endif

        introspection_data = g_dbus_node_info_new_for_xml(introspection_xml,
                                                          NULL);

        owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                  "org.freedesktop.Notifications",
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                  on_bus_acquired,
                                  on_name_acquired, on_name_lost, NULL, NULL);

        return owner_id;
}

void dbus_tear_down(int owner_id)
{
        if (introspection_data)
                g_dbus_node_info_unref(introspection_data);

        g_bus_unown_name(owner_id);
}

/* vim: set ts=8 sw=8 tw=0: */

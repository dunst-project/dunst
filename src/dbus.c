/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#include "dbus.h"

#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

#include "dunst.h"
#include "log.h"
#include "notification.h"
#include "queues.h"
#include "settings.h"
#include "utils.h"

#define FDN_PATH "/org/freedesktop/Notifications"
#define FDN_IFAC "org.freedesktop.Notifications"
#define FDN_NAME "org.freedesktop.Notifications"

GDBusConnection *dbus_conn;

static GDBusNodeInfo *introspection_data = NULL;

static const char *introspection_xml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<node name=\""FDN_PATH"\">"
    "    <interface name=\""FDN_IFAC"\">"

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

static void on_get_capabilities(GDBusConnection *connection,
                                const gchar *sender,
                                const GVariant *parameters,
                                GDBusMethodInvocation *invocation);
static void on_notify(GDBusConnection *connection,
                      const gchar *sender,
                      GVariant *parameters,
                      GDBusMethodInvocation *invocation);
static void on_close_notification(GDBusConnection *connection,
                                  const gchar *sender,
                                  GVariant *parameters,
                                  GDBusMethodInvocation *invocation);
static void on_get_server_information(GDBusConnection *connection,
                                      const gchar *sender,
                                      const GVariant *parameters,
                                      GDBusMethodInvocation *invocation);
static RawImage *get_raw_image_from_data_hint(GVariant *icon_data);

void handle_method_call(GDBusConnection *connection,
                        const gchar *sender,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *method_name,
                        GVariant *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer user_data)
{
        if (g_strcmp0(method_name, "GetCapabilities") == 0) {
                on_get_capabilities(connection, sender, parameters, invocation);
        } else if (g_strcmp0(method_name, "Notify") == 0) {
                on_notify(connection, sender, parameters, invocation);
        } else if (g_strcmp0(method_name, "CloseNotification") == 0) {
                on_close_notification(connection, sender, parameters, invocation);
        } else if (g_strcmp0(method_name, "GetServerInformation") == 0) {
                on_get_server_information(connection, sender, parameters, invocation);
        } else {
                LOG_M("Unknown method name: '%s' (sender: '%s').",
                      method_name,
                      sender);
        }
}

static void on_get_capabilities(GDBusConnection *connection,
                                const gchar *sender,
                                const GVariant *parameters,
                                GDBusMethodInvocation *invocation)
{
        GVariantBuilder *builder;
        GVariant *value;

        builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        g_variant_builder_add(builder, "s", "actions");
        g_variant_builder_add(builder, "s", "body");
        g_variant_builder_add(builder, "s", "body-hyperlinks");

        if (settings.markup != MARKUP_NO)
                g_variant_builder_add(builder, "s", "body-markup");

        value = g_variant_new("(as)", builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, value);

        g_dbus_connection_flush(connection, NULL, NULL, NULL);
}

static notification *dbus_message_to_notification(const gchar *sender, GVariant *parameters)
{

        gchar *appname = NULL;
        guint replaces_id = 0;
        gchar *icon = NULL;
        gchar *summary = NULL;
        gchar *body = NULL;
        Actions *actions = g_malloc0(sizeof(Actions));
        gint timeout = -1;

        /* hints */
        gint urgency = 1;
        gint progress = -1;
        gboolean transient = 0;
        gchar *fgcolor = NULL;
        gchar *bgcolor = NULL;
        gchar *category = NULL;
        RawImage *raw_icon = NULL;

        {
                GVariantIter *iter = g_variant_iter_new(parameters);
                GVariant *content;
                GVariant *dict_value;
                int idx = 0;
                while ((content = g_variant_iter_next_value(iter))) {

                        switch (idx) {
                        case 0:
                                if (g_variant_is_of_type(content, G_VARIANT_TYPE_STRING))
                                        appname = g_variant_dup_string(content, NULL);
                                break;
                        case 1:
                                if (g_variant_is_of_type(content, G_VARIANT_TYPE_UINT32))
                                        replaces_id = g_variant_get_uint32(content);
                                break;
                        case 2:
                                if (g_variant_is_of_type(content, G_VARIANT_TYPE_STRING))
                                        icon = g_variant_dup_string(content, NULL);
                                break;
                        case 3:
                                if (g_variant_is_of_type(content, G_VARIANT_TYPE_STRING))
                                        summary = g_variant_dup_string(content, NULL);
                                break;
                        case 4:
                                if (g_variant_is_of_type(content, G_VARIANT_TYPE_STRING))
                                        body = g_variant_dup_string(content, NULL);
                                break;
                        case 5:
                                if (g_variant_is_of_type(content, G_VARIANT_TYPE_STRING_ARRAY))
                                        actions->actions = g_variant_dup_strv(content, &(actions->count));
                                break;
                        case 6:
                                if (g_variant_is_of_type(content, G_VARIANT_TYPE_DICTIONARY)) {

                                        dict_value = g_variant_lookup_value(content, "urgency", G_VARIANT_TYPE_BYTE);
                                        if (dict_value) {
                                                urgency = g_variant_get_byte(dict_value);
                                                g_variant_unref(dict_value);
                                        }

                                        dict_value = g_variant_lookup_value(content, "fgcolor", G_VARIANT_TYPE_STRING);
                                        if (dict_value) {
                                                fgcolor = g_variant_dup_string(dict_value, NULL);
                                                g_variant_unref(dict_value);
                                        }

                                        dict_value = g_variant_lookup_value(content, "bgcolor", G_VARIANT_TYPE_STRING);
                                        if (dict_value) {
                                                bgcolor = g_variant_dup_string(dict_value, NULL);
                                                g_variant_unref(dict_value);
                                        }

                                        dict_value = g_variant_lookup_value(content, "category", G_VARIANT_TYPE_STRING);
                                        if (dict_value) {
                                                category = g_variant_dup_string(dict_value, NULL);
                                                g_variant_unref(dict_value);
                                        }

                                        dict_value = g_variant_lookup_value(content, "image-path", G_VARIANT_TYPE_STRING);
                                        if (dict_value) {
                                                g_free(icon);
                                                icon = g_variant_dup_string(dict_value, NULL);
                                                g_variant_unref(dict_value);
                                        }

                                        dict_value = g_variant_lookup_value(content, "image-data", G_VARIANT_TYPE("(iiibiiay)"));
                                        if (!dict_value)
                                                dict_value = g_variant_lookup_value(content, "image_data", G_VARIANT_TYPE("(iiibiiay)"));
                                        if (!dict_value)
                                                dict_value = g_variant_lookup_value(content, "icon_data", G_VARIANT_TYPE("(iiibiiay)"));
                                        if (dict_value) {
                                                raw_icon = get_raw_image_from_data_hint(dict_value);
                                                g_variant_unref(dict_value);
                                        }

                                        /* Check for transient hints
                                         *
                                         * According to the spec, the transient hint should be boolean.
                                         * But notify-send does not support hints of type 'boolean'.
                                         * So let's check for int and boolean until notify-send is fixed.
                                         */
                                        if((dict_value = g_variant_lookup_value(content, "transient", G_VARIANT_TYPE_BOOLEAN))) {
                                                transient = g_variant_get_boolean(dict_value);
                                                g_variant_unref(dict_value);
                                        } else if((dict_value = g_variant_lookup_value(content, "transient", G_VARIANT_TYPE_UINT32))) {
                                                transient = g_variant_get_uint32(dict_value) > 0;
                                                g_variant_unref(dict_value);
                                        } else if((dict_value = g_variant_lookup_value(content, "transient", G_VARIANT_TYPE_INT32))) {
                                                transient = g_variant_get_int32(dict_value) > 0;
                                                g_variant_unref(dict_value);
                                        }

                                        if((dict_value = g_variant_lookup_value(content, "value", G_VARIANT_TYPE_INT32))) {
                                                progress = g_variant_get_int32(dict_value);
                                                g_variant_unref(dict_value);
                                        } else if((dict_value = g_variant_lookup_value(content, "value", G_VARIANT_TYPE_UINT32))) {
                                                progress = g_variant_get_uint32(dict_value);
                                                g_variant_unref(dict_value);
                                        }
                                }
                                break;
                        case 7:
                                if (g_variant_is_of_type(content, G_VARIANT_TYPE_INT32))
                                        timeout = g_variant_get_int32(content);
                                break;
                        }
                        g_variant_unref(content);
                        idx++;
                }

                g_variant_iter_free(iter);
        }

        fflush(stdout);

        notification *n = notification_create();

        n->id = replaces_id;
        n->appname = appname;
        n->summary = summary;
        n->body = body;
        n->icon = icon;
        n->raw_icon = raw_icon;
        n->timeout = timeout < 0 ? -1 : timeout * 1000;
        n->progress = progress;
        n->urgency = urgency;
        n->category = category;
        n->dbus_client = g_strdup(sender);
        n->transient = transient;

        if (actions->count < 1) {
                actions_free(actions);
                actions = NULL;
        }
        n->actions = actions;

        n->colors[ColFG] = fgcolor;
        n->colors[ColBG] = bgcolor;

        notification_init(n);
        return n;
}

static void on_notify(GDBusConnection *connection,
                      const gchar *sender,
                      GVariant *parameters,
                      GDBusMethodInvocation *invocation)
{
        notification *n = dbus_message_to_notification(sender, parameters);
        int id = queues_notification_insert(n);

        GVariant *reply = g_variant_new("(u)", id);
        g_dbus_method_invocation_return_value(invocation, reply);
        g_dbus_connection_flush(connection, NULL, NULL, NULL);

        // The message got discarded
        if (id == 0) {
                signal_notification_closed(n, 2);
                notification_free(n);
        }

        wake_up();
}

static void on_close_notification(GDBusConnection *connection,
                                  const gchar *sender,
                                  GVariant *parameters,
                                  GDBusMethodInvocation *invocation)
{
        guint32 id;
        g_variant_get(parameters, "(u)", &id);
        queues_notification_close_id(id, REASON_SIG);
        wake_up();
        g_dbus_method_invocation_return_value(invocation, NULL);
        g_dbus_connection_flush(connection, NULL, NULL, NULL);
}

static void on_get_server_information(GDBusConnection *connection,
                                      const gchar *sender,
                                      const GVariant *parameters,
                                      GDBusMethodInvocation *invocation)
{
        GVariant *value;

        value = g_variant_new("(ssss)", "dunst", "knopwob", VERSION, "1.2");
        g_dbus_method_invocation_return_value(invocation, value);

        g_dbus_connection_flush(connection, NULL, NULL, NULL);
}

void signal_notification_closed(notification *n, enum reason reason)
{
        if (reason < REASON_MIN || REASON_MAX < reason) {
                LOG_W("Closing notification with reason '%d' not supported. "
                      "Closing it with reason '%d'.", reason, REASON_UNDEF);
                reason = REASON_UNDEF;
        }

        if (!dbus_conn) {
                LOG_E("Unable to close notification: No DBus connection.");
        }

        GVariant *body = g_variant_new("(uu)", n->id, reason);
        GError *err = NULL;

        g_dbus_connection_emit_signal(dbus_conn,
                                      n->dbus_client,
                                      FDN_PATH,
                                      FDN_IFAC,
                                      "NotificationClosed",
                                      body,
                                      &err);

        if (err) {
                LOG_W("Unable to close notification: %s", err->message);
                g_error_free(err);
        }

}

void signal_action_invoked(notification *n, const char *identifier)
{
        GVariant *body = g_variant_new("(us)", n->id, identifier);
        GError *err = NULL;

        g_dbus_connection_emit_signal(dbus_conn,
                                      n->dbus_client,
                                      FDN_PATH,
                                      FDN_IFAC,
                                      "ActionInvoked",
                                      body,
                                      &err);

        if (err) {
                LOG_W("Unable to invoke action: %s", err->message);
                g_error_free(err);
        }
}

static const GDBusInterfaceVTable interface_vtable = {
        handle_method_call
};

static void on_bus_acquired(GDBusConnection *connection,
                            const gchar *name,
                            gpointer user_data)
{
        guint registration_id;

        GError *err = NULL;

        registration_id = g_dbus_connection_register_object(connection,
                                                            FDN_PATH,
                                                            introspection_data->interfaces[0],
                                                            &interface_vtable,
                                                            NULL,
                                                            NULL,
                                                            &err);

        if (registration_id == 0) {
                DIE("Unable to register dbus connection: %s", err->message);
        }
}

static void on_name_acquired(GDBusConnection *connection,
                             const gchar *name,
                             gpointer user_data)
{
        dbus_conn = connection;
}

/*
 * Get the PID of the current process, which acquired FDN DBus Name.
 *
 * If name or vendor specified, the name and vendor
 * will get additionally get via the FDN GetServerInformation method
 *
 * Returns: valid PID, else -1
 */
static int dbus_get_fdn_daemon_info(GDBusConnection  *connection,
                                       char **name,
                                       char **vendor)
{
        char *owner = NULL;
        GError *error = NULL;
        int pid = -1;

        GDBusProxy *proxy_fdn;
        GDBusProxy *proxy_dbus;

        if (!connection)
                return pid;

        proxy_fdn = g_dbus_proxy_new_sync(
                                     connection,
                                     /* do not trigger a start of the notification daemon */
                                     G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                     NULL, /* info */
                                     FDN_NAME,
                                     FDN_PATH,
                                     FDN_IFAC,
                                     NULL, /* cancelable */
                                     &error);

        if (error) {
                g_error_free(error);
                return pid;
        }

        GVariant *daemoninfo;
        if (name || vendor) {
                daemoninfo = g_dbus_proxy_call_sync(
                                     proxy_fdn,
                                     FDN_IFAC ".GetServerInformation",
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     /* It's not worth to wait for the info
                                      * longer than half a second when dying */
                                     500,
                                     NULL, /* cancelable */
                                     &error);
        }

        if (error) {
                /* Ignore the error, we may still be able to retrieve the PID */
                g_error_free(error);
                error = NULL;
        } else {
                g_variant_get(daemoninfo, "(ssss)", name, vendor, NULL, NULL);
        }

        owner = g_dbus_proxy_get_name_owner(proxy_fdn);

        proxy_dbus = g_dbus_proxy_new_sync(
                                     connection,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     NULL, /* info */
                                     "org.freedesktop.DBus",
                                     "/org/freedesktop/DBus",
                                     "org.freedesktop.DBus",
                                     NULL, /* cancelable */
                                     &error);

        if (error) {
                g_error_free(error);
                return pid;
        }

        GVariant *pidinfo = g_dbus_proxy_call_sync(
                                     proxy_dbus,
                                     "org.freedesktop.DBus.GetConnectionUnixProcessID",
                                     g_variant_new("(s)", owner),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     /* It's not worth to wait for the PID
                                      * longer than half a second when dying */
                                     500,
                                     NULL,
                                     &error);

        if (error) {
                g_error_free(error);
                return pid;
        }

        g_variant_get(pidinfo, "(u)", &pid);

        g_object_unref(proxy_fdn);
        g_object_unref(proxy_dbus);
        g_free(owner);
        if (daemoninfo)
                g_variant_unref(daemoninfo);
        if (pidinfo)
                g_variant_unref(pidinfo);

        return pid;
}


static void on_name_lost(GDBusConnection *connection,
                         const gchar *name,
                         gpointer user_data)
{
        if (connection) {
                char *name = NULL;
                int pid = dbus_get_fdn_daemon_info(connection, &name, NULL);
                if (pid > 0) {
                        DIE("Cannot acquire '"FDN_NAME"': "
                            "Name is acquired by '%s' with PID '%d'.", name, pid);
                } else {
                        DIE("Cannot acquire '"FDN_NAME"'.");
                }
        } else {
                DIE("Cannot connect to DBus.");
        }
        exit(1);
}

static RawImage *get_raw_image_from_data_hint(GVariant *icon_data)
{
        RawImage *image = g_malloc(sizeof(RawImage));
        GVariant *data_variant;
        gsize expected_len;

        g_variant_get(icon_data,
                      "(iiibii@ay)",
                      &image->width,
                      &image->height,
                      &image->rowstride,
                      &image->has_alpha,
                      &image->bits_per_sample,
                      &image->n_channels,
                      &data_variant);

        expected_len = (image->height - 1) * image->rowstride + image->width
                * ((image->n_channels * image->bits_per_sample + 7) / 8);

        if (expected_len != g_variant_get_size (data_variant)) {
                LOG_W("Expected image data to be of length %" G_GSIZE_FORMAT
                      " but got a length of %" G_GSIZE_FORMAT,
                      expected_len,
                      g_variant_get_size(data_variant));
                g_free(image);
                g_variant_unref(data_variant);
                return NULL;
        }

        image->data = (guchar *) g_memdup(g_variant_get_data(data_variant),
                                          g_variant_get_size(data_variant));
        g_variant_unref(data_variant);

        return image;
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
                                  FDN_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                  on_bus_acquired,
                                  on_name_acquired,
                                  on_name_lost,
                                  NULL,
                                  NULL);

        return owner_id;
}

void dbus_tear_down(int owner_id)
{
        if (introspection_data)
                g_dbus_node_info_unref(introspection_data);

        g_bus_unown_name(owner_id);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

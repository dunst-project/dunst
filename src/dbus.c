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

static const char *stack_tag_hints[] = {
        "synchronous",
        "private-synchronous",
        "x-canonical-private-synchronous",
        "x-dunst-stack-tag"
};

struct dbus_method {
  const char *method_name;
  void (*method)  (GDBusConnection *connection,
                   const gchar *sender,
                   GVariant *parameters,
                   GDBusMethodInvocation *invocation);
};

#define DBUS_METHOD(name) static void dbus_cb_##name( \
                        GDBusConnection *connection, \
                        const gchar *sender, \
                        GVariant *parameters, \
                        GDBusMethodInvocation *invocation)

int cmp_methods(const void *vkey, const void *velem)
{
        const char *key = (const char*)vkey;
        const struct dbus_method *m = (const struct dbus_method*)velem;

        return strcmp(key, m->method_name);
}

DBUS_METHOD(Notify);
DBUS_METHOD(CloseNotification);
DBUS_METHOD(GetCapabilities);
DBUS_METHOD(GetServerInformation);

static struct dbus_method methods_fdn[] = {
        {"CloseNotification",     dbus_cb_CloseNotification},
        {"GetCapabilities",       dbus_cb_GetCapabilities},
        {"GetServerInformation",  dbus_cb_GetServerInformation},
        {"Notify",                dbus_cb_Notify},
};

void handle_method_call(GDBusConnection *connection,
                        const gchar *sender,
                        const gchar *object_path,
                        const gchar *interface_name,
                        const gchar *method_name,
                        GVariant *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer user_data)
{
        struct dbus_method *m = bsearch(
                        method_name,
                        &methods_fdn,
                        G_N_ELEMENTS(methods_fdn),
                        sizeof(struct dbus_method),
                        cmp_methods);

        if (m) {
                m->method(connection, sender, parameters, invocation);
        } else {
                LOG_M("Unknown method name: '%s' (sender: '%s').",
                      method_name,
                      sender);
        }
}

static void dbus_cb_GetCapabilities(
                GDBusConnection *connection,
                const gchar *sender,
                GVariant *parameters,
                GDBusMethodInvocation *invocation)
{
        GVariantBuilder *builder;
        GVariant *value;

        builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
        g_variant_builder_add(builder, "s", "actions");
        g_variant_builder_add(builder, "s", "body");
        g_variant_builder_add(builder, "s", "body-hyperlinks");

        for (int i = 0; i < sizeof(stack_tag_hints)/sizeof(*stack_tag_hints); ++i)
                g_variant_builder_add(builder, "s", stack_tag_hints[i]);

        if (settings.markup != MARKUP_NO)
                g_variant_builder_add(builder, "s", "body-markup");

        value = g_variant_new("(as)", builder);
        g_clear_pointer(&builder, g_variant_builder_unref);
        g_dbus_method_invocation_return_value(invocation, value);

        g_dbus_connection_flush(connection, NULL, NULL, NULL);
}

static struct notification *dbus_message_to_notification(const gchar *sender, GVariant *parameters)
{
        /* Assert that the parameters' type is actually correct. Albeit usually DBus
         * already rejects ill typed parameters, it may not be always the case. */
        GVariantType *required_type = g_variant_type_new("(susssasa{sv}i)");
        if (!g_variant_is_of_type(parameters, required_type)) {
                g_variant_type_free(required_type);
                return NULL;
        }

        struct notification *n = notification_create();
        n->dbus_client = g_strdup(sender);
        n->dbus_valid = true;

        GVariant *hints;
        gchar **actions;
        int timeout;

        GVariantIter i;
        g_variant_iter_init(&i, parameters);

        g_variant_iter_next(&i, "s", &n->appname);
        g_variant_iter_next(&i, "u", &n->id);
        g_variant_iter_next(&i, "s", &n->iconname);
        g_variant_iter_next(&i, "s", &n->summary);
        g_variant_iter_next(&i, "s", &n->body);
        g_variant_iter_next(&i, "^a&s", &actions);
        g_variant_iter_next(&i, "@a{?*}", &hints);
        g_variant_iter_next(&i, "i", &timeout);

        gsize num = 0;
        while (actions[num]) {
                if (actions[num+1]) {
                        g_hash_table_insert(n->actions,
                                        g_strdup(actions[num]),
                                        g_strdup(actions[num+1]));
                        num+=2;
                } else {
                        LOG_W("Odd length in actions array. Ignoring element: %s", actions[num]);
                        break;
                }
        }

        GVariant *dict_value;
        if ((dict_value = g_variant_lookup_value(hints, "urgency", G_VARIANT_TYPE_BYTE))) {
                n->urgency = g_variant_get_byte(dict_value);
                g_variant_unref(dict_value);
        }

        if ((dict_value = g_variant_lookup_value(hints, "fgcolor", G_VARIANT_TYPE_STRING))) {
                n->colors.fg = g_variant_dup_string(dict_value, NULL);
                g_variant_unref(dict_value);
        }

        if ((dict_value = g_variant_lookup_value(hints, "bgcolor", G_VARIANT_TYPE_STRING))) {
                n->colors.bg = g_variant_dup_string(dict_value, NULL);
                g_variant_unref(dict_value);
        }

        if ((dict_value = g_variant_lookup_value(hints, "frcolor", G_VARIANT_TYPE_STRING))) {
                n->colors.frame = g_variant_dup_string(dict_value, NULL);
                g_variant_unref(dict_value);
        }

        if ((dict_value = g_variant_lookup_value(hints, "category", G_VARIANT_TYPE_STRING))) {
                n->category = g_variant_dup_string(dict_value, NULL);
                g_variant_unref(dict_value);
        }

        if ((dict_value = g_variant_lookup_value(hints, "desktop-entry", G_VARIANT_TYPE_STRING))) {
                n->desktop_entry = g_variant_dup_string(dict_value, NULL);
                g_variant_unref(dict_value);
        }

        if ((dict_value = g_variant_lookup_value(hints, "image-path", G_VARIANT_TYPE_STRING))) {
                g_free(n->iconname);
                n->iconname = g_variant_dup_string(dict_value, NULL);
                g_variant_unref(dict_value);
        }

        dict_value = g_variant_lookup_value(hints, "image-data", G_VARIANT_TYPE("(iiibiiay)"));
        if (!dict_value)
                dict_value = g_variant_lookup_value(hints, "image_data", G_VARIANT_TYPE("(iiibiiay)"));
        if (!dict_value)
                dict_value = g_variant_lookup_value(hints, "icon_data", G_VARIANT_TYPE("(iiibiiay)"));
        if (dict_value) {
                notification_icon_replace_data(n, dict_value);
                g_variant_unref(dict_value);
        }

        /* Check for transient hints
         *
         * According to the spec, the transient hint should be boolean.
         * But notify-send does not support hints of type 'boolean'.
         * So let's check for int and boolean until notify-send is fixed.
         */
        if ((dict_value = g_variant_lookup_value(hints, "transient", G_VARIANT_TYPE_BOOLEAN))) {
                n->transient = g_variant_get_boolean(dict_value);
                g_variant_unref(dict_value);
        } else if ((dict_value = g_variant_lookup_value(hints, "transient", G_VARIANT_TYPE_UINT32))) {
                n->transient = g_variant_get_uint32(dict_value) > 0;
                g_variant_unref(dict_value);
        } else if ((dict_value = g_variant_lookup_value(hints, "transient", G_VARIANT_TYPE_INT32))) {
                n->transient = g_variant_get_int32(dict_value) > 0;
                g_variant_unref(dict_value);
        }

        if ((dict_value = g_variant_lookup_value(hints, "value", G_VARIANT_TYPE_INT32))) {
                n->progress = g_variant_get_int32(dict_value);
                g_variant_unref(dict_value);
        } else if ((dict_value = g_variant_lookup_value(hints, "value", G_VARIANT_TYPE_UINT32))) {
                n->progress = g_variant_get_uint32(dict_value);
                g_variant_unref(dict_value);
        }

        /* Check for hints that define the stack_tag
         *
         * Only accept to first one we find.
         */
        for (int i = 0; i < sizeof(stack_tag_hints)/sizeof(*stack_tag_hints); ++i) {
                if ((dict_value = g_variant_lookup_value(hints, stack_tag_hints[i], G_VARIANT_TYPE_STRING))) {
                        n->stack_tag = g_variant_dup_string(dict_value, NULL);
                        g_variant_unref(dict_value);
                        break;
                }
        }

        if (timeout >= 0)
                n->timeout = ((gint64)timeout) * 1000;

        g_variant_unref(hints);
        g_variant_type_free(required_type);
        g_free(actions); // the strv is only a shallow copy

        notification_init(n);
        return n;
}

static void dbus_cb_Notify(
                GDBusConnection *connection,
                const gchar *sender,
                GVariant *parameters,
                GDBusMethodInvocation *invocation)
{
        struct notification *n = dbus_message_to_notification(sender, parameters);
        if (!n) {
                LOG_W("A notification failed to decode.");
                g_dbus_method_invocation_return_dbus_error(
                                invocation,
                                FDN_IFAC".Error",
                                "Cannot decode notification!");
                return;
        }

        int id = queues_notification_insert(n);

        GVariant *reply = g_variant_new("(u)", id);
        g_dbus_method_invocation_return_value(invocation, reply);
        g_dbus_connection_flush(connection, NULL, NULL, NULL);

        // The message got discarded
        if (id == 0) {
                signal_notification_closed(n, REASON_USER);
                notification_unref(n);
        }

        wake_up();
}

static void dbus_cb_CloseNotification(
                GDBusConnection *connection,
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

static void dbus_cb_GetServerInformation(
                GDBusConnection *connection,
                const gchar *sender,
                GVariant *parameters,
                GDBusMethodInvocation *invocation)
{
        GVariant *value;

        value = g_variant_new("(ssss)", "dunst", "knopwob", VERSION, "1.2");
        g_dbus_method_invocation_return_value(invocation, value);

        g_dbus_connection_flush(connection, NULL, NULL, NULL);
}

void signal_notification_closed(struct notification *n, enum reason reason)
{
        if (!n->dbus_valid) {
                LOG_W("Closing notification '%s' not supported. "
                      "Notification already closed.", n->summary);
                return;
        }

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

        notification_invalidate_actions(n);

        n->dbus_valid = false;

        if (err) {
                LOG_W("Unable to close notification: %s", err->message);
                g_error_free(err);
        }

}

void signal_action_invoked(const struct notification *n, const char *identifier)
{
        if (!n->dbus_valid) {
                LOG_W("Invoking action '%s' not supported. "
                      "Notification already closed.", identifier);
                return;
        }

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

static void dbus_cb_bus_acquired(GDBusConnection *connection,
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

static void dbus_cb_name_acquired(GDBusConnection *connection,
                                  const gchar *name,
                                  gpointer user_data)
{
        dbus_conn = connection;
}

/**
 * Get the PID of the current process, which acquired FDN DBus Name.
 *
 * If name or vendor specified, the name and vendor
 * will get additionally get via the FDN GetServerInformation method
 *
 * @param connection The DBus connection
 * @param pid The place to report the PID to
 * @param name The place to report the name to, if not required set to NULL
 * @param vendor The place to report the vendor to, if not required set to NULL
 *
 * @retval true: on success
 * @retval false: Any error happened
 */
static bool dbus_get_fdn_daemon_info(GDBusConnection  *connection,
                                     guint   *pid,
                                     char   **name,
                                     char   **vendor)
{
        ASSERT_OR_RET(pid, false);
        ASSERT_OR_RET(connection, false);

        char *owner = NULL;
        GError *error = NULL;

        GDBusProxy *proxy_fdn;
        GDBusProxy *proxy_dbus;

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
                return false;
        }

        GVariant *daemoninfo = NULL;
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
                g_clear_pointer(&error, g_error_free);
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
                return false;
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
                return false;
        }

        g_object_unref(proxy_fdn);
        g_object_unref(proxy_dbus);
        g_free(owner);
        if (daemoninfo)
                g_variant_unref(daemoninfo);

        if (pidinfo) {
                g_variant_get(pidinfo, "(u)", pid);
                g_variant_unref(pidinfo);
                return true;
        } else {
                return false;
        }
}


static void dbus_cb_name_lost(GDBusConnection *connection,
                              const gchar *name,
                              gpointer user_data)
{
        if (connection) {
                char *name;
                unsigned int pid;
                if (dbus_get_fdn_daemon_info(connection, &pid, &name, NULL)) {
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

int dbus_init(void)
{
        guint owner_id;

        introspection_data = g_dbus_node_info_new_for_xml(introspection_xml,
                                                          NULL);

        owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                  FDN_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                  dbus_cb_bus_acquired,
                                  dbus_cb_name_acquired,
                                  dbus_cb_name_lost,
                                  NULL,
                                  NULL);

        return owner_id;
}

void dbus_teardown(int owner_id)
{
        g_clear_pointer(&introspection_data, g_dbus_node_info_unref);

        g_bus_unown_name(owner_id);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

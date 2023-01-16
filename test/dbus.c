#define wake_up wake_up_void
#include "../src/dbus.c"
#include "../src/rules.h"
#include "greatest.h"

#include <assert.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>

#include "helpers.h"
#include "queues.h"

extern const char *base;

void wake_up_void(void) {  }

struct signal_actioninvoked {
        guint id;
        gchar *key;
        guint subscription_id;
        GDBusConnection *conn;
};

struct signal_propertieschanged {
        gchar *interface;
        GVariant *array_dict_sv_data;
        GVariant *array_s_data;
        guint subscription_id;
        GDBusConnection *conn;
};

struct signal_closed {
        guint32 id;
        guint32 reason;
        guint subscription_id;
        GDBusConnection *conn;
};

void dbus_signal_cb_actioninvoked(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data)
{
        g_return_if_fail(user_data);

        guint32 id;
        gchar *key;

        struct signal_actioninvoked *sig = (struct signal_actioninvoked*) user_data;

        g_variant_get(parameters, "(us)", &id, &key);

        if (id == sig->id) {
                sig->id = id;
                sig->key = key;
        }
}

void dbus_signal_subscribe_actioninvoked(struct signal_actioninvoked *actioninvoked)
{
        assert(actioninvoked);

        actioninvoked->conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        actioninvoked->subscription_id =
                g_dbus_connection_signal_subscribe(
                        actioninvoked->conn,
                        FDN_NAME,
                        FDN_IFAC,
                        "ActionInvoked",
                        FDN_PATH,
                        NULL,
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        dbus_signal_cb_actioninvoked,
                        actioninvoked,
                        NULL);
}

void dbus_signal_unsubscribe_actioninvoked(struct signal_actioninvoked *actioninvoked)
{
        assert(actioninvoked);

        g_dbus_connection_signal_unsubscribe(actioninvoked->conn, actioninvoked->subscription_id);
        g_object_unref(actioninvoked->conn);

        actioninvoked->conn = NULL;
        actioninvoked->subscription_id = -1;
}

void dbus_signal_cb_propertieschanged(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data)
{
        g_return_if_fail(user_data);

        gchar *interface;
        GVariant *array_dict_sv_data;
        GVariant *array_s_data;

        struct signal_propertieschanged *sig = (struct signal_propertieschanged*) user_data;

        g_variant_get(parameters, "(s@a{sv}@as)", &interface, &array_dict_sv_data, &array_s_data);

        if (g_strcmp0(interface, DUNST_IFAC) == 0) {
                sig->interface = interface;
                sig->array_dict_sv_data = array_dict_sv_data;
                sig->array_s_data = array_s_data;
        }

}

void dbus_signal_subscribe_propertieschanged(struct signal_propertieschanged *propertieschanged)
{
        assert(propertieschanged);

        propertieschanged->conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        propertieschanged->subscription_id =
                g_dbus_connection_signal_subscribe(
                        // GDBusConnection *connection,
                        propertieschanged->conn,
                        // const gchar *sender,
                        FDN_NAME,
                        // const gchar *interface_name,
                        PROPERTIES_IFAC,
                        // const gchar *member,
                        "PropertiesChanged",
                        // const gchar *object_path,
                        FDN_PATH,
                        // const gchar *arg0,
                        NULL,
                        // GDBusSignalFlags flags,
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        // GDBusSignalCallback callback,
                        dbus_signal_cb_propertieschanged,
                        // gpointer user_data,
                        propertieschanged,
                        // GDestroyNotify user_data_free_func
                        NULL);

}

void dbus_signal_unsubscribe_propertieschanged(struct signal_propertieschanged *propertieschanged)
{
        assert(propertieschanged);

        g_dbus_connection_signal_unsubscribe(propertieschanged->conn, propertieschanged->subscription_id);
        g_object_unref(propertieschanged->conn);

        propertieschanged->conn = NULL;
        propertieschanged->subscription_id = -1;
}

void dbus_signal_cb_closed(GDBusConnection *connection,
                 const gchar *sender_name,
                 const gchar *object_path,
                 const gchar *interface_name,
                 const gchar *signal_name,
                 GVariant *parameters,
                 gpointer user_data)
{
        g_return_if_fail(user_data);

        guint32 id;
        guint32 reason;

        struct signal_closed *sig = (struct signal_closed*) user_data;
        g_variant_get(parameters, "(uu)", &id, &reason);

        if (id == sig->id) {
                sig->id = id;
                sig->reason = reason;
        }
}

void dbus_signal_subscribe_closed(struct signal_closed *closed)
{
        assert(closed);

        closed->conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        closed->subscription_id =
                g_dbus_connection_signal_subscribe(
                        closed->conn,
                        FDN_NAME,
                        FDN_IFAC,
                        "NotificationClosed",
                        FDN_PATH,
                        NULL,
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        dbus_signal_cb_closed,
                        closed,
                        NULL);
}

void dbus_signal_unsubscribe_closed(struct signal_closed *closed)
{
        assert(closed);

        g_dbus_connection_signal_unsubscribe(closed->conn, closed->subscription_id);
        g_object_unref(closed->conn);

        closed->conn = NULL;
        closed->subscription_id = -1;
}

GVariant *dbus_invoke(const char *method, GVariant *params)
{
        GDBusConnection *connection_client;
        GVariant *retdata;
        GError *error = NULL;

        connection_client = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        retdata = g_dbus_connection_call_sync(
                                connection_client,
                                FDN_NAME,
                                FDN_PATH,
                                FDN_IFAC,
                                method,
                                params,
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &error);
        if (error) {
                printf("Error while calling GTestDBus instance: %s\n", error->message);
                g_error_free(error);
        }

        g_object_unref(connection_client);

        return retdata;
}

struct dbus_notification {
        const char* app_name;
        guint replaces_id;
        const char* app_icon;
        const char* summary;
        const char* body;
        GHashTable *actions;
        GHashTable *hints;
        int expire_timeout;
};

void g_free_variant_value(gpointer tofree)
{
        g_variant_unref((GVariant*) tofree);
}

struct dbus_notification *dbus_notification_new(void)
{
        struct dbus_notification *n = g_malloc0(sizeof(struct dbus_notification));
        n->expire_timeout = -1;
        n->replaces_id = 0;
        n->actions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        n->hints = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free_variant_value);
        return n;
}

void dbus_notification_free(struct dbus_notification *n)
{
        g_hash_table_unref(n->hints);
        g_hash_table_unref(n->actions);
        g_free(n);
}

bool dbus_notification_fire(struct dbus_notification *n, uint *id)
{
        assert(n);
        assert(id);
        GVariantBuilder b;
        GVariantType *t;

        gpointer p_key;
        gpointer p_value;
        GHashTableIter iter;

        t = g_variant_type_new("(susssasa{sv}i)");
        g_variant_builder_init(&b, t);
        g_variant_type_free(t);

        g_variant_builder_add(&b, "s", n->app_name);
        g_variant_builder_add(&b, "u", n->replaces_id);
        g_variant_builder_add(&b, "s", n->app_icon);
        g_variant_builder_add(&b, "s", n->summary);
        g_variant_builder_add(&b, "s", n->body);

        // Add the actions
        t = g_variant_type_new("as");
        g_variant_builder_open(&b, t);
        g_hash_table_iter_init(&iter, n->actions);
        while (g_hash_table_iter_next(&iter, &p_key, &p_value)) {
                g_variant_builder_add(&b, "s", (char*)p_key);
                g_variant_builder_add(&b, "s", (char*)p_value);
        }
        // Add an invalid appendix to cover odd numbered action arrays
        // Shouldn't interfere with normal testing
        g_variant_builder_add(&b, "s", "invalid appendix");
        g_variant_builder_close(&b);
        g_variant_type_free(t);

        // Add the hints
        t = g_variant_type_new("a{sv}");
        g_variant_builder_open(&b, t);
        g_hash_table_iter_init(&iter, n->hints);
        while (g_hash_table_iter_next(&iter, &p_key, &p_value)) {
                g_variant_builder_add(&b, "{sv}", (char*)p_key, (GVariant*)p_value);
        }
        g_variant_builder_close(&b);
        g_variant_type_free(t);

        g_variant_builder_add(&b, "i", n->expire_timeout);

        GVariant *reply = dbus_invoke("Notify", g_variant_builder_end(&b));
        if (reply) {
                g_variant_get(reply, "(u)", id);
                g_variant_unref(reply);
                return true;
        } else {
                return false;
        }
}

void dbus_notification_set_raw_image(struct dbus_notification *n_dbus, const char *path)
{
        GVariant *hint = notification_setup_raw_image(path);
        if (!hint)
                return;

        g_hash_table_insert(n_dbus->hints,
                            g_strdup("image-data"),
                            g_variant_ref_sink(hint));
}

/////// TESTS
gint owner_id;

TEST test_dbus_init(void)
{
        owner_id = dbus_init();
        uint waiting = 0;
        while (!dbus_conn && waiting < 2000) {
                usleep(500);
                waiting++;
        }
        ASSERTm("After 1s, there is still no dbus connection available.",
                dbus_conn);
        PASS();
}

TEST test_dbus_teardown(void)
{
        dbus_teardown(owner_id);
        PASS();
}

TEST test_invalid_notification(void)
{
        GVariant *faulty = g_variant_new_boolean(true);

        ASSERT(NULL == dbus_message_to_notification(":123", faulty));
        ASSERT(NULL == dbus_invoke("Notify", faulty));

        g_variant_unref(faulty);
        PASS();
}

TEST test_dbus_cb_dunst_Properties_Get(void)
{

        GDBusConnection *connection_client;
        GError *error = NULL;

        connection_client = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

        GVariant *pause_variant = dbus_cb_dunst_Properties_Get(connection_client,
                                      FDN_NAME,
                                      FDN_PATH,
                                      DUNST_IFAC,
                                      "paused",
                                      &error,
                                      NULL);

        if (error) {
                printf("Error while calling dbus_cb_dunst_Properties_Get: %s\n", error->message);
                g_error_free(error);
        }

        ASSERT_FALSE(g_variant_get_boolean(pause_variant));
        g_variant_unref(pause_variant);

        dunst_status(S_RUNNING, false);

        pause_variant = dbus_cb_dunst_Properties_Get(connection_client,
                                      FDN_NAME,
                                      FDN_PATH,
                                      DUNST_IFAC,
                                      "paused",
                                      &error,
                                      NULL);

        if (error) {
                printf("Error while calling dbus_cb_dunst_Properties_Get: %s\n", error->message);
                g_error_free(error);
        }

        ASSERT(g_variant_get_boolean(pause_variant));
        g_variant_unref(pause_variant);

        g_object_unref(connection_client);
        PASS();
}

TEST test_dbus_cb_dunst_Properties_Set(void)
{

        GDBusConnection *connection_client;
        GError *error = NULL;
        struct signal_propertieschanged sig = {NULL, NULL, NULL, -1};

        dbus_signal_subscribe_propertieschanged(&sig);

        connection_client = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

        GVariant *pause_variant = g_variant_new_boolean(TRUE);


       ASSERT(dbus_cb_dunst_Properties_Set(connection_client,
                                      FDN_NAME,
                                      FDN_PATH,
                                      DUNST_IFAC,
                                      "paused",
                                      pause_variant,
                                      &error,
                                      NULL));

        if (error) {
                printf("Error while calling dbus_cb_dunst_Properties_Set: %s\n", error->message);
                g_error_free(error);
        }

        uint waiting = 0;
        while (!sig.interface && waiting < 2000) {
                usleep(500);
                waiting++;
        }

        ASSERT_STR_EQ(sig.interface, DUNST_IFAC);

        gboolean paused;
        g_variant_lookup(sig.array_dict_sv_data, "paused", "b", &paused);

        ASSERT(paused);

        g_variant_unref(pause_variant);
        g_free(sig.interface);
        g_variant_unref(sig.array_dict_sv_data);
        g_variant_unref(sig.array_s_data);
        dbus_signal_unsubscribe_propertieschanged(&sig);
        g_object_unref(connection_client);
        PASS();
}

TEST test_empty_notification(void)
{
        struct dbus_notification *n = dbus_notification_new();
        gsize len = queues_length_waiting();

        guint id;
        ASSERT(dbus_notification_fire(n, &id));
        ASSERT(id != 0);

        ASSERT_EQ(queues_length_waiting(), len+1);
        dbus_notification_free(n);
        PASS();
}

TEST test_basic_notification(void)
{
        struct dbus_notification *n = dbus_notification_new();
        gsize len = queues_length_waiting();
        n->app_name = "dunstteststack";
        n->app_icon = "NONE";
        n->summary = "Headline";
        n->body = "Text";
        g_hash_table_insert(n->actions, g_strdup("actionid"), g_strdup("Print this text"));
        g_hash_table_insert(n->hints,
                            g_strdup("x-dunst-stack-tag"),
                            g_variant_ref_sink(g_variant_new_string("volume")));

        n->replaces_id = 10;

        guint id;
        ASSERT(dbus_notification_fire(n, &id));
        ASSERT(id != 0);

        ASSERT_EQ(queues_length_waiting(), len+1);
        dbus_notification_free(n);
        PASS();
}

TEST test_dbus_notify_colors(void)
{
        const char *color_frame = "I allow all string values for frame!";
        const char *color_bg = "I allow all string values for background!";
        const char *color_fg = "I allow all string values for foreground!";
        struct notification *n;
        struct dbus_notification *n_dbus;

        gsize len = queues_length_waiting();

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_dbus_notify_colors";
        n_dbus->body = "Summary of it";
        g_hash_table_insert(n_dbus->hints,
                            g_strdup("frcolor"),
                            g_variant_ref_sink(g_variant_new_string(color_frame)));
        g_hash_table_insert(n_dbus->hints,
                            g_strdup("bgcolor"),
                            g_variant_ref_sink(g_variant_new_string(color_bg)));
        g_hash_table_insert(n_dbus->hints,
                            g_strdup("fgcolor"),
                            g_variant_ref_sink(g_variant_new_string(color_fg)));

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        ASSERT_EQ(queues_length_waiting(), len+1);

        n = queues_debug_find_notification_by_id(id);

        ASSERT_STR_EQ(n->colors.frame, color_frame);
        ASSERT_STR_EQ(n->colors.fg, color_fg);
        ASSERT_STR_EQ(n->colors.bg, color_bg);

        dbus_notification_free(n_dbus);

        PASS();
}

TEST test_hint_transient(void)
{
        static char msg[50];
        struct notification *n;
        struct dbus_notification *n_dbus;

        gsize len = queues_length_waiting();

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_hint_transient";
        n_dbus->body = "Summary of it";

        bool values[] = { true, true, true, false, false, false, false };
        GVariant *variants[] = {
                g_variant_new_boolean(true),
                g_variant_new_int32(1),
                g_variant_new_uint32(1),
                g_variant_new_boolean(false),
                g_variant_new_int32(0),
                g_variant_new_uint32(0),
                g_variant_new_int32(-1),
        };
        for (size_t i = 0; i < G_N_ELEMENTS(variants); i++) {
                g_hash_table_insert(n_dbus->hints,
                                    g_strdup("transient"),
                                    g_variant_ref_sink(variants[i]));

                guint id;
                ASSERT(dbus_notification_fire(n_dbus, &id));
                ASSERT(id != 0);

                snprintf(msg, sizeof(msg), "In round %ld", i);
                ASSERT_EQm(msg, queues_length_waiting(), len+1);

                n = queues_debug_find_notification_by_id(id);

                ASSERT_EQm(msg, values[i], n->transient);
        }

        dbus_notification_free(n_dbus);

        PASS();
}

TEST test_hint_progress(void)
{
        static char msg[50];
        struct notification *n;
        struct dbus_notification *n_dbus;

        gsize len = queues_length_waiting();

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_hint_progress";
        n_dbus->body = "Summary of it";

        int values[] = { 99, 12, 123, 123, -1, -1 };
        GVariant *variants[] = {
                g_variant_new_int32(99),
                g_variant_new_uint32(12),
                g_variant_new_int32(123), // allow higher than 100
                g_variant_new_uint32(123),
                g_variant_new_int32(-192),
                g_variant_new_uint32(-192),
        };
        for (size_t i = 0; i < G_N_ELEMENTS(variants); i++) {
                g_hash_table_insert(n_dbus->hints,
                                    g_strdup("value"),
                                    g_variant_ref_sink(variants[i]));

                guint id;
                ASSERT(dbus_notification_fire(n_dbus, &id));
                ASSERT(id != 0);

                snprintf(msg, sizeof(msg), "In round %ld", i);
                ASSERT_EQm(msg, queues_length_waiting(), len+1);

                n = queues_debug_find_notification_by_id(id);

                snprintf(msg, sizeof(msg), "In round %ld progress should be %i, but is %i", i, n->progress, values[i]);
                ASSERT_EQm(msg, values[i], n->progress);
        }

        dbus_notification_free(n_dbus);

        PASS();
}

TEST test_hint_icons(void)
{
        struct notification *n;
        struct dbus_notification *n_dbus;
        const char *iconname = "NEWICON";

        gsize len = queues_length_waiting();

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_hint_icons";
        n_dbus->body = "Summary of it";

        g_hash_table_insert(n_dbus->hints,
                            g_strdup("image-path"),
                            g_variant_ref_sink(g_variant_new_string(iconname)));

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        ASSERT_EQ(queues_length_waiting(), len+1);

        n = queues_debug_find_notification_by_id(id);

        ASSERT_STR_EQ(iconname, n->iconname);

        dbus_notification_free(n_dbus);

        PASS();
}

TEST test_hint_category(void)
{
        struct notification *n;
        struct dbus_notification *n_dbus;
        const char *category = "VOLUME";

        gsize len = queues_length_waiting();

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_hint_category";
        n_dbus->body = "Summary of it";

        g_hash_table_insert(n_dbus->hints,
                            g_strdup("category"),
                            g_variant_ref_sink(g_variant_new_string(category)));

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        ASSERT_EQ(queues_length_waiting(), len+1);

        n = queues_debug_find_notification_by_id(id);

        ASSERT_STR_EQ(category, n->category);

        dbus_notification_free(n_dbus);

        PASS();
}

TEST test_hint_desktop_entry(void)
{
        struct notification *n;
        struct dbus_notification *n_dbus;
        const char *desktop_entry = "org.dunst-project.dunst";

        gsize len = queues_length_waiting();

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_hint_desktopentry";
        n_dbus->body = "Summary of my desktop_entry";

        g_hash_table_insert(n_dbus->hints,
                            g_strdup("desktop-entry"),
                            g_variant_ref_sink(g_variant_new_string(desktop_entry)));

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        ASSERT_EQ(queues_length_waiting(), len+1);

        n = queues_debug_find_notification_by_id(id);

        ASSERT_STR_EQ(desktop_entry, n->desktop_entry);

        dbus_notification_free(n_dbus);

        PASS();
}

TEST test_hint_urgency(void)
{
        static char msg[50];
        struct notification *n;
        struct dbus_notification *n_dbus;

        gsize len = queues_length_waiting();

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_hint_urgency";
        n_dbus->body = "Summary of it";

        enum urgency values[] = { URG_MAX, URG_LOW, URG_NORM, URG_CRIT };
        GVariant *variants[] = {
                g_variant_new_byte(10),
                g_variant_new_byte(0),
                g_variant_new_byte(1),
                g_variant_new_byte(2),
        };
        for (size_t i = 0; i < G_N_ELEMENTS(variants); i++) {
                g_hash_table_insert(n_dbus->hints,
                                    g_strdup("urgency"),
                                    g_variant_ref_sink(variants[i]));

                guint id;
                ASSERT(dbus_notification_fire(n_dbus, &id));
                ASSERT(id != 0);

                ASSERT_EQ(queues_length_waiting(), len+1);

                n = queues_debug_find_notification_by_id(id);

                snprintf(msg, sizeof(msg), "In round %ld", i);
                ASSERT_EQm(msg, values[i], n->urgency);

                queues_notification_close_id(id, REASON_UNDEF);
        }

        dbus_notification_free(n_dbus);

        PASS();
}

TEST test_hint_raw_image(void)
{
        guint id;
        struct notification *n;
        struct dbus_notification *n_dbus;

        char *path = g_strconcat(base, "/data/icons/valid.png", NULL);
        gsize len = queues_length_waiting();

        n_dbus = dbus_notification_new();
        dbus_notification_set_raw_image(n_dbus, path);
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_hint_raw_image";
        n_dbus->body = "Summary of it";

        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        ASSERT_EQ(queues_length_waiting(), len+1);
        n = queues_debug_find_notification_by_id(id);

        ASSERT(n->icon);
        ASSERT(!STR_EQ(n->icon_id, n_dbus->app_icon));

        dbus_notification_free(n_dbus);
        g_free(path);

        PASS();
}

/* We didn't process the timeout parameter via DBus correctly
 * and it got limited to an int instead of a long int
 * See: Issue #646 (The timeout value in dunst wraps around) */
TEST test_timeout_overflow(void)
{
        struct notification *n;
        struct dbus_notification *n_dbus;

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE";
        n_dbus->summary = "test_hint_urgency";
        n_dbus->body = "Summary of it";
        n_dbus->expire_timeout = 2147484;
        gint64 expected_timeout = G_GINT64_CONSTANT(2147484000);

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        n = queues_debug_find_notification_by_id(id);
        ASSERT_EQ_FMT(expected_timeout, n->timeout, "%" G_GINT64_FORMAT);

        dbus_notification_free(n_dbus);

        PASS();
}

TEST test_server_caps(enum markup_mode markup)
{
        GVariant *reply;
        GVariant *caps = NULL;
        const char **capsarray;

        struct rule *global_rule = get_rule("global");
        if (!global_rule) {
                global_rule = rule_new("global");
        }
        global_rule->markup = markup;

        reply = dbus_invoke("GetCapabilities", NULL);

        caps = g_variant_get_child_value(reply, 0);
        capsarray = g_variant_get_strv(caps, NULL);

        ASSERT(capsarray);
        ASSERT(g_strv_contains(capsarray, "actions"));
        ASSERT(g_strv_contains(capsarray, "body"));
        ASSERT(g_strv_contains(capsarray, "body-hyperlinks"));
        ASSERT(g_strv_contains(capsarray, "icon-static"));
        ASSERT(g_strv_contains(capsarray, "x-dunst-stack-tag"));

        if (markup != MARKUP_NO)
                ASSERT(g_strv_contains(capsarray, "body-markup"));
        else
                ASSERT_FALSE(g_strv_contains(capsarray, "body-markup"));

        g_free(capsarray);
        g_variant_unref(caps);
        g_variant_unref(reply);
        PASS();
}

TEST test_signal_actioninvoked(void)
{
        const struct notification *n;
        struct dbus_notification *n_dbus;
        struct signal_actioninvoked sig = {0, NULL, -1};

        dbus_signal_subscribe_actioninvoked(&sig);

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE2";
        n_dbus->summary = "Headline for New";
        n_dbus->body = "Text";
        g_hash_table_insert(n_dbus->actions, g_strdup("actionkey"), g_strdup("Print this text"));

        dbus_notification_fire(n_dbus, &sig.id);
        n = queues_debug_find_notification_by_id(sig.id);

        signal_action_invoked(n, "actionkey");

        uint waiting = 0;
        while (!sig.key && waiting < 2000) {
                usleep(500);
                waiting++;
        }

        ASSERT_STR_EQ("actionkey", sig.key);

        g_free(sig.key);
        dbus_notification_free(n_dbus);
        dbus_signal_unsubscribe_actioninvoked(&sig);
        PASS();
}

TEST test_signal_length_propertieschanged(void)
{
        struct dbus_notification *n_dbus;
        struct signal_propertieschanged sig = {NULL, NULL, NULL, -1};

        dbus_signal_subscribe_propertieschanged(&sig);

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->app_icon = "NONE2";
        n_dbus->summary = "Headline for New";
        n_dbus->body = "Text";
        g_hash_table_insert(n_dbus->actions, g_strdup("actionkey"), g_strdup("Print this text"));

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        queues_update(STATUS_NORMAL, time_monotonic_now());

        uint waiting = 0;
        while (!sig.interface && waiting < 2000) {
                usleep(500);
                waiting++;
        }

        ASSERT_STR_EQ(sig.interface, DUNST_IFAC);

        guint32 displayed_length;
        g_variant_lookup(sig.array_dict_sv_data, "displayedLength", "u", &displayed_length);

        ASSERT_EQ(displayed_length, queues_length_displayed());

        g_free(sig.interface);
        g_variant_unref(sig.array_dict_sv_data);
        g_variant_unref(sig.array_s_data);
        dbus_notification_free(n_dbus);
        dbus_signal_unsubscribe_propertieschanged(&sig);
        PASS();
}

TEST test_close_and_signal(void)
{
        GVariant *data, *ret;
        struct dbus_notification *n;
        struct signal_closed sig = {0, REASON_MIN-1, -1};

        dbus_signal_subscribe_closed(&sig);

        n = dbus_notification_new();
        n->app_name = "dunstteststack";
        n->app_icon = "NONE2";
        n->summary = "Headline for New";
        n->body = "Text";

        dbus_notification_fire(n, &sig.id);

        data = g_variant_new("(u)", sig.id);
        ret = dbus_invoke("CloseNotification", data);

        ASSERT(ret);

        uint waiting = 0;
        while (sig.reason == REASON_MIN-1 && waiting < 2000) {
                usleep(500);
                waiting++;
        }

        ASSERT(sig.reason != REASON_MIN-1);

        dbus_notification_free(n);
        dbus_signal_unsubscribe_closed(&sig);
        g_variant_unref(ret);
        PASS();
}

TEST test_get_fdn_daemon_info(void)
{
        unsigned int pid_is;
        pid_t pid_should;
        char *name, *vendor;
        GDBusConnection *conn;

        pid_should = getpid();
        conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

        ASSERT(dbus_get_fdn_daemon_info(conn, &pid_is, &name, &vendor));

        ASSERT_EQ_FMT(pid_should, pid_is, "%d");
        ASSERT_STR_EQ("dunst", name);
        ASSERT_STR_EQ("knopwob", vendor);

        g_free(name);
        g_free(vendor);

        g_object_unref(conn);
        PASS();
}

TEST assert_methodlists_sorted(void)
{
        for (size_t i = 0; i+1 < G_N_ELEMENTS(methods_fdn); i++) {
                ASSERT(0 > strcmp(
                                methods_fdn[i].method_name,
                                methods_fdn[i+1].method_name));
        }

        for (size_t i = 0; i+1 < G_N_ELEMENTS(methods_dunst); i++) {
                ASSERT(0 > strcmp(
                                methods_dunst[i].method_name,
                                methods_dunst[i+1].method_name));
        }

        PASS();
}

TEST test_override_dbus_timeout(void)
{
        struct notification *n;
        struct dbus_notification *n_dbus;
        struct rule *rule;

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->expire_timeout = 2147484;

        rule = rule_new("test_override_dbus_timeout");
        rule->appname = "dunstteststack";
        rule->override_dbus_timeout = 100000;

        gint64 expected_timeout = rule->override_dbus_timeout;

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        n = queues_debug_find_notification_by_id(id);
        ASSERT_EQ_FMT(expected_timeout, n->timeout, "%" G_GINT64_FORMAT);

        dbus_notification_free(n_dbus);
        rule->enabled = false;

        PASS();
}

TEST test_match_dbus_timeout(void)
{
        struct notification *n;
        struct dbus_notification *n_dbus;
        struct rule *rule;

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";
        n_dbus->expire_timeout = 2147484;

        rule = rule_new("test_match_dbus_timeout");
        rule->match_dbus_timeout = 2147484000;
        rule->override_dbus_timeout = 100000;

        gint64 expected_timeout = rule->override_dbus_timeout;

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        n = queues_debug_find_notification_by_id(id);
        ASSERT_EQ_FMT(expected_timeout, n->timeout, "%" G_GINT64_FORMAT);

        dbus_notification_free(n_dbus);
        rule->enabled = false;

        PASS();
}

TEST test_timeout(void)
{
        struct notification *n;
        struct dbus_notification *n_dbus;
        struct rule *rule;

        n_dbus = dbus_notification_new();
        n_dbus->app_name = "dunstteststack";

        rule = rule_new("test_timeout");
        rule->appname = "dunstteststack";
        rule->timeout = 100001;

        gint64 expected_timeout = rule->timeout;

        guint id;
        ASSERT(dbus_notification_fire(n_dbus, &id));
        ASSERT(id != 0);

        n = queues_debug_find_notification_by_id(id);
        ASSERT_EQ_FMT(expected_timeout, n->timeout, "%" G_GINT64_FORMAT);

        dbus_notification_free(n_dbus);
        rule->enabled = false;

        PASS();
}

// TESTS END

GMainLoop *loop;
GThread *thread_tests;

gpointer run_threaded_tests(gpointer data)
{
        RUN_TEST(test_dbus_init);

        RUN_TEST(test_get_fdn_daemon_info);
        RUN_TEST(test_dbus_cb_dunst_Properties_Get);
        RUN_TEST(test_dbus_cb_dunst_Properties_Set);

        RUN_TEST(test_empty_notification);
        RUN_TEST(test_basic_notification);
        RUN_TEST(test_invalid_notification);
        RUN_TEST(test_hint_transient);
        RUN_TEST(test_hint_progress);
        RUN_TEST(test_hint_icons);
        RUN_TEST(test_hint_category);
        RUN_TEST(test_hint_desktop_entry);
        RUN_TEST(test_hint_urgency);
        RUN_TEST(test_hint_raw_image);
        RUN_TEST(test_dbus_notify_colors);
        RUN_TESTp(test_server_caps, MARKUP_FULL);
        RUN_TESTp(test_server_caps, MARKUP_STRIP);
        RUN_TESTp(test_server_caps, MARKUP_NO);
        RUN_TEST(test_close_and_signal);
        RUN_TEST(test_signal_actioninvoked);
        RUN_TEST(test_signal_length_propertieschanged);
        RUN_TEST(test_timeout_overflow);
        RUN_TEST(test_override_dbus_timeout);
        RUN_TEST(test_match_dbus_timeout);
        RUN_TEST(test_timeout);

        RUN_TEST(assert_methodlists_sorted);

        RUN_TEST(test_dbus_teardown);
        g_main_loop_quit(loop);
        return NULL;
}

SUITE(suite_dbus)
{
        GTestDBus *dbus_bus;
        g_test_dbus_unset();
        queues_init();

        loop = g_main_loop_new(NULL, false);

        dbus_bus = g_test_dbus_new(G_TEST_DBUS_NONE);

        // workaround bug in glib where stdout output is duplicated
        // See https://gitlab.gnome.org/GNOME/glib/-/issues/2322
        fflush(stdout);
        g_test_dbus_up(dbus_bus);

        thread_tests = g_thread_new("testexecutor", run_threaded_tests, loop);
        g_main_loop_run(loop);

        queues_teardown();
        g_test_dbus_down(dbus_bus);
        g_object_unref(dbus_bus);
        g_thread_unref(thread_tests);
        g_main_loop_unref(loop);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

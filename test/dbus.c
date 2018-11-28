#define wake_up wake_up_void
#include "../src/dbus.c"
#include "greatest.h"

#include <assert.h>
#include <gio/gio.h>

void wake_up_void(void) {  }

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

/////// TESTS
gint owner_id;

TEST test_dbus_init(void)
{
        owner_id = dbus_init();
        uint waiting = 0;
        while (!dbus_conn && waiting < 20) {
                usleep(500);
                waiting++;
        }
        ASSERTm("After 10ms, there is still no dbus connection available.",
                dbus_conn);
        PASS();
}

TEST test_dbus_teardown(void)
{
        dbus_teardown(owner_id);
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

TEST test_server_caps(enum markup_mode markup)
{
        GVariant *reply;
        GVariant *caps = NULL;
        const char **capsarray;

        settings.markup = markup;

        reply = dbus_invoke("GetCapabilities", NULL);

        caps = g_variant_get_child_value(reply, 0);
        capsarray = g_variant_get_strv(caps, NULL);

        ASSERT(capsarray);
        ASSERT(g_strv_contains(capsarray, "actions"));
        ASSERT(g_strv_contains(capsarray, "body"));
        ASSERT(g_strv_contains(capsarray, "body-hyperlinks"));
        ASSERT(g_strv_contains(capsarray, "x-dunst-stack-tag"));

        if (settings.markup != MARKUP_NO)
                ASSERT(g_strv_contains(capsarray, "body-markup"));
        else
                ASSERT_FALSE(g_strv_contains(capsarray, "body-markup"));

        g_free(capsarray);
        g_variant_unref(caps);
        g_variant_unref(reply);
        PASS();
}


// TESTS END

GMainLoop *loop;
GThread *thread_tests;

gpointer run_threaded_tests(gpointer data)
{
        RUN_TEST(test_dbus_init);

        RUN_TEST(test_basic_notification);
        RUN_TESTp(test_server_caps, MARKUP_FULL);
        RUN_TESTp(test_server_caps, MARKUP_STRIP);
        RUN_TESTp(test_server_caps, MARKUP_NO);

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

#include "../src/notification.c"
#include "greatest.h"
#include "helpers.h"

#include "../src/option_parser.h"
#include "../src/settings.h"

extern const char *base;

TEST test_notification_is_duplicate_field(char **field,
                                          struct notification *a,
                                          struct notification *b)
{
        ASSERT(notification_is_duplicate(a, b));
        char *tmp = *field;
        (*field) = "Something different";
        ASSERT_FALSE(notification_is_duplicate(a, b));
        (*field) = tmp;

        PASS();
}

TEST test_notification_is_duplicate(void)
{
        struct notification *a = notification_create();
        a->appname = g_strdup("Test");
        a->summary = g_strdup("Summary");
        a->body = g_strdup("Body");
        a->iconname = g_strdup("Icon");
        a->icon_id = g_strdup("Icon");
        a->urgency = URG_NORM;

        struct notification *b = notification_create();
        b->appname = g_strdup("Test");
        b->summary = g_strdup("Summary");
        b->body = g_strdup("Body");
        b->iconname = g_strdup("Icon");
        b->icon_id = g_strdup("Icon");
        b->urgency = URG_NORM;

        ASSERT(notification_is_duplicate(a, b));

        CHECK_CALL(test_notification_is_duplicate_field(&(b->appname), a, b));
        CHECK_CALL(test_notification_is_duplicate_field(&(b->summary), a, b));
        CHECK_CALL(test_notification_is_duplicate_field(&(b->body), a, b));

        ASSERTm("One of the notifications got corrupted during test",
                notification_is_duplicate(a, b));

        enum icon_position icon_setting_tmp = settings.icon_position;

        settings.icon_position = ICON_OFF;
        ASSERT(notification_is_duplicate(a, b));
        //Setting pointer to a random value since we are checking for null
        char *icon_id = b->icon_id;
        b->icon_id = "false";
        ASSERTm("Icons have to get ignored for duplicate check when icons are off",
                notification_is_duplicate(a, b));
        b->icon_id = icon_id;

        settings.icon_position = ICON_LEFT;
        CHECK_CALL(test_notification_is_duplicate_field(&(b->icon_id), a, b));

        settings.icon_position = ICON_RIGHT;
        CHECK_CALL(test_notification_is_duplicate_field(&(b->icon_id), a, b));

        settings.icon_position = icon_setting_tmp;

        ASSERT(notification_is_duplicate(a, b));

        b->urgency = URG_LOW;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->urgency = URG_NORM;
        ASSERT(notification_is_duplicate(a, b));
        b->urgency = URG_CRIT;
        ASSERT_FALSE(notification_is_duplicate(a, b));

        notification_unref(a);
        notification_unref(b);
        PASS();
}

TEST test_notification_replace_single_field(void)
{
        char *str = g_malloc(128 * sizeof(char));
        char *substr = NULL;

        strcpy(str, "Markup %a preserved");
        substr = strchr(str, '%');
        notification_replace_single_field(&str, &substr, "and &amp; <i>is</i>", MARKUP_FULL);
        ASSERT_STR_EQ("Markup and &amp; <i>is</i> preserved", str);
        ASSERT_EQ(26, substr - str);

        strcpy(str, "Markup %a escaped");
        substr = strchr(str, '%');
        notification_replace_single_field(&str, &substr, "and & <i>is</i>", MARKUP_NO);
        ASSERT_STR_EQ("Markup and &amp; &lt;i&gt;is&lt;/i&gt; escaped", str);
        ASSERT_EQ(38, substr - str);

        strcpy(str, "Markup %a");
        substr = strchr(str, '%');
        notification_replace_single_field(&str, &substr, "<i>is removed</i> and & escaped", MARKUP_STRIP);
        ASSERT_STR_EQ("Markup is removed and &amp; escaped", str);
        ASSERT_EQ(35, substr - str);

        g_free(str);
        PASS();
}

TEST test_notification_referencing(void)
{
        struct notification *n = notification_create();
        ASSERT(notification_refcount_get(n) == 1);

        notification_ref(n);
        ASSERT(notification_refcount_get(n) == 2);

        notification_unref(n);
        ASSERT(notification_refcount_get(n) == 1);

        // Now we have to rely on valgrind to test, that
        // it gets actually freed
        notification_unref(n);

        PASS();
}


static struct notification *notification_load_icon_with_scaling(int min_icon_size, int max_icon_size)
{
        struct notification *n = notification_create();

        char *path = g_strconcat(base, "/data/icons/valid.svg", NULL); // 16x16

        GVariant *rawIcon = notification_setup_raw_image(path);

        settings.min_icon_size = min_icon_size;
        settings.max_icon_size = max_icon_size;
        notification_icon_replace_data(n, rawIcon);
        settings.min_icon_size = 0;
        settings.max_icon_size = 0;

        g_variant_unref(rawIcon);
        g_free(path);

        return n;
}

TEST test_notification_icon_scaling_toosmall(void)
{
        struct notification *n = notification_load_icon_with_scaling(20, 100);

        ASSERT_EQ(gdk_pixbuf_get_width(n->icon), 20);
        ASSERT_EQ(gdk_pixbuf_get_height(n->icon), 20);

        notification_unref(n);

        PASS();
}


TEST test_notification_icon_scaling_toolarge(void)
{
        struct notification *n = notification_load_icon_with_scaling(5, 10);

        ASSERT_EQ(gdk_pixbuf_get_width(n->icon), 10);
        ASSERT_EQ(gdk_pixbuf_get_height(n->icon), 10);

        notification_unref(n);

        PASS();
}

TEST test_notification_icon_scaling_notconfigured(void)
{
        struct notification *n = notification_load_icon_with_scaling(0, 0);

        ASSERT_EQ(gdk_pixbuf_get_width(n->icon), 16);
        ASSERT_EQ(gdk_pixbuf_get_height(n->icon), 16);

        notification_unref(n);

        PASS();
}

TEST test_notification_icon_scaling_notneeded(void)
{
        struct notification *n = notification_load_icon_with_scaling(10, 20);

        ASSERT_EQ(gdk_pixbuf_get_width(n->icon), 16);
        ASSERT_EQ(gdk_pixbuf_get_height(n->icon), 16);

        notification_unref(n);

        PASS();
}

TEST test_notification_format_message(struct notification *n, const char *format, const char *exp)
{
        n->format = format;
        notification_format_message(n);

        ASSERT_STR_EQ(exp, n->msg);

        PASS();
}

TEST test_notification_maxlength(void)
{
        unsigned int len = 5005;
        struct notification *n = notification_create();
        n->format = "%a";

        n->appname = g_malloc(len + 1);
        n->appname[len] = '\0';

        static const char sigma[] =
                            " 0123456789"
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "abcdefghijklmnopqrstuvwxyz";
        for (int i = 0; i < len; ++i)
                n->appname[i] = sigma[rand() % (sizeof(sigma) - 1)];

        notification_format_message(n);
        ASSERT(STRN_EQ(n->appname, n->msg, 5000));

        notification_unref(n);
        PASS();
}


SUITE(suite_notification)
{
        cmdline_load(0, NULL);
        char *config_path = g_strconcat(base, "/data/dunstrc.default", NULL);
        load_settings(config_path);

        RUN_TEST(test_notification_is_duplicate);
        RUN_TEST(test_notification_replace_single_field);
        RUN_TEST(test_notification_referencing);
        RUN_TEST(test_notification_icon_scaling_toosmall);
        RUN_TEST(test_notification_icon_scaling_toolarge);
        RUN_TEST(test_notification_icon_scaling_notconfigured);
        RUN_TEST(test_notification_icon_scaling_notneeded);

        // TEST notification_format_message
        struct notification *a = notification_create();
        a->appname = g_strdup("MyApp");
        a->summary = g_strdup("I've got a summary!");
        a->body =    g_strdup("Look at my shiny <notification>");
        a->iconname =    g_strdup("/this/is/my/icoknpath.png");
        a->progress = 95;

        const char *strings[] = {
                "%a", "MyApp",
                "%s", "I&apos;ve got a summary!",
                "%b", "Look at my shiny <notification>",
                "%I", "icoknpath.png",
                "%i", "/this/is/my/icoknpath.png",
                "%p", "[ 95%]",
                "%n", "95",
                "%%", "%",
                "%",  "%",
                "%UNKNOWN", "%UNKNOWN",
                NULL
        };

        const char **in = strings;
        const char **out = strings+1;
        while (*in && *out) {
                RUN_TESTp(test_notification_format_message, a, *in, *out);
                in +=2;
                out+=2;
        }
        g_clear_pointer(&a, notification_unref);

        RUN_TEST(test_notification_maxlength);

        g_clear_pointer(&settings.icon_path, g_free);
        g_free(config_path);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

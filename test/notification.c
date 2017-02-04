#include "greatest.h"
#include "src/notification.h"
#include "src/option_parser.h"
#include "src/settings.h"

#include <glib.h>

TEST test_notification_is_duplicate_field(char **field, notification *a,
                                          notification *b)
{
        ASSERT(notification_is_duplicate(a, b));
        char *tmp = *field;
        (*field) = "Something different";
        ASSERT_FALSE(notification_is_duplicate(a, b));
        (*field) = tmp;

        PASS();
}

TEST test_notification_is_duplicate(void *notifications)
{
        notification **n = (notification**)notifications;
        notification *a = n[0];
        notification *b = n[1];

        ASSERT(notification_is_duplicate(a, b));

        CHECK_CALL(test_notification_is_duplicate_field(&(b->appname), a, b));
        CHECK_CALL(test_notification_is_duplicate_field(&(b->summary), a, b));
        CHECK_CALL(test_notification_is_duplicate_field(&(b->body), a, b));

        ASSERT(notification_is_duplicate(a, b));

        char *tmp = b->icon;
        enum icon_position_t icon_setting_tmp = settings.icon_position;

        b->icon = "Test1";

        settings.icon_position = icons_off;
        ASSERT(notification_is_duplicate(a, b));
        //Setting pointer to a random value since we are checking for null
        b->raw_icon = (RawImage*)0xff;
        ASSERT(notification_is_duplicate(a, b));
        b->raw_icon = NULL;

        settings.icon_position = icons_left;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->raw_icon = (RawImage*)0xff;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->raw_icon = NULL;

        settings.icon_position = icons_right;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->raw_icon = (RawImage*)0xff;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->raw_icon = NULL;

        b->icon = tmp;
        settings.icon_position = icon_setting_tmp;

        ASSERT(notification_is_duplicate(a, b));

        b->urgency = LOW;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->urgency = NORM;
        ASSERT(notification_is_duplicate(a, b));
        b->urgency = CRIT;
        ASSERT_FALSE(notification_is_duplicate(a, b));

        PASS();
}

TEST test_notification_replace_format(void)
{
        char *str = g_malloc(128 * sizeof(char));

        strcpy(str, "Testing format replacement");
        ASSERT_STR_EQ("Testing text replacement", (str = notification_replace_format("format", "text", str, MARKUP_FULL)));

        strcpy(str, "Markup %a preserved");
        ASSERT_STR_EQ("Markup and &amp; <i>is</i> preserved", (str = notification_replace_format("%a", "and &amp; <i>is</i>", str, MARKUP_FULL)));

        strcpy(str, "Markup %a escaped");
        ASSERT_STR_EQ("Markup and &amp; &lt;i&gt;is&lt;/i&gt; escaped", (str = notification_replace_format("%a", "and & <i>is</i>", str, MARKUP_NO)));

        strcpy(str, "Markup %a");
        ASSERT_STR_EQ("Markup is removed and &amp; escaped", (str = notification_replace_format("%a", "<i>is removed</i> and & escaped", str, MARKUP_STRIP)));

        g_free(str);
        PASS();
}

SUITE(suite_notification)
{
        cmdline_load(0, NULL);
        load_settings("data/dunstrc");

        notification *a = notification_create();
        a->appname = "Test";
        a->summary = "Summary";
        a->body = "Body";
        a->icon = "Icon";
        a->urgency = NORM;

        notification *b = notification_create();
        memcpy(b, a, sizeof(*b));

        //2 equal notifications to be passed for duplicate checking,
        notification *n[2] = {a, b};

        RUN_TEST1(test_notification_is_duplicate, (void*) n);
        g_free(a);
        g_free(b);

        RUN_TEST(test_notification_replace_format);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

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
        b->raw_icon = (struct raw_image*)0xff;
        ASSERT(notification_is_duplicate(a, b));
        b->raw_icon = NULL;

        settings.icon_position = icons_left;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->raw_icon = (struct raw_image*)0xff;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->raw_icon = NULL;

        settings.icon_position = icons_right;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->raw_icon = (struct raw_image*)0xff;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->raw_icon = NULL;

        b->icon = tmp;
        settings.icon_position = icon_setting_tmp;

        ASSERT(notification_is_duplicate(a, b));

        b->urgency = URG_LOW;
        ASSERT_FALSE(notification_is_duplicate(a, b));
        b->urgency = URG_NORM;
        ASSERT(notification_is_duplicate(a, b));
        b->urgency = URG_CRIT;
        ASSERT_FALSE(notification_is_duplicate(a, b));

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

SUITE(suite_notification)
{
        cmdline_load(0, NULL);
        load_settings("data/dunstrc.default");

        notification *a = notification_create();
        a->appname = "Test";
        a->summary = "Summary";
        a->body = "Body";
        a->icon = "Icon";
        a->urgency = URG_NORM;

        notification *b = notification_create();
        memcpy(b, a, sizeof(*b));

        //2 equal notifications to be passed for duplicate checking,
        notification *n[2] = {a, b};

        RUN_TEST1(test_notification_is_duplicate, (void*) n);
        g_free(a);
        g_free(b);

        RUN_TEST(test_notification_replace_single_field);

        g_clear_pointer(&settings.icon_path, g_free);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

#include "greatest.h"

#include <stdbool.h>
#include <glib.h>

#include "src/markup.h"

TEST test_markup_strip(void)
{
        char *ptr;

        ASSERT_STR_EQ("&quot;", (ptr=markup_strip(g_strdup("&amp;quot;"))));
        g_free(ptr);
        ASSERT_STR_EQ("&apos;", (ptr=markup_strip(g_strdup("&amp;apos;"))));
        g_free(ptr);
        ASSERT_STR_EQ("&lt;", (ptr=markup_strip(g_strdup("&amp;lt;"))));
        g_free(ptr);
        ASSERT_STR_EQ("&gt;", (ptr=markup_strip(g_strdup("&amp;gt;"))));
        g_free(ptr);
        ASSERT_STR_EQ("&amp;", (ptr=markup_strip(g_strdup("&amp;amp;"))));
        g_free(ptr);
        ASSERT_STR_EQ(">A  ", (ptr=markup_strip(g_strdup(">A <img> <string"))));
        g_free(ptr);

        PASS();
}

TEST test_markup_transform(void)
{
        char *ptr;

        settings.ignore_newline = false;
        ASSERT_STR_EQ("&lt;i&gt;foo&lt;/i&gt;&lt;br&gt;bar\nbaz", (ptr=markup_transform(g_strdup("<i>foo</i><br>bar\nbaz"), MARKUP_NO)));
        g_free(ptr);
        ASSERT_STR_EQ("foo\nbar\nbaz", (ptr=markup_transform(g_strdup("<i>foo</i><br>bar\nbaz"), MARKUP_STRIP)));
        g_free(ptr);
        ASSERT_STR_EQ("<i>foo</i>\nbar\nbaz", (ptr=markup_transform(g_strdup("<i>foo</i><br>bar\nbaz"), MARKUP_FULL)));
        g_free(ptr);

        settings.ignore_newline = true;
        ASSERT_STR_EQ("&lt;i&gt;foo&lt;/i&gt;&lt;br&gt;bar baz", (ptr=markup_transform(g_strdup("<i>foo</i><br>bar\nbaz"), MARKUP_NO)));
        g_free(ptr);
        ASSERT_STR_EQ("foo bar baz", (ptr=markup_transform(g_strdup("<i>foo</i><br>bar\nbaz"), MARKUP_STRIP)));
        g_free(ptr);
        ASSERT_STR_EQ("<i>foo</i> bar baz", (ptr=markup_transform(g_strdup("<i>foo</i><br>bar\nbaz"), MARKUP_FULL)));
        g_free(ptr);

        PASS();
}

SUITE(suite_markup)
{
        RUN_TEST(test_markup_strip);
        RUN_TEST(test_markup_transform);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

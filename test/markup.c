#include "../src/markup.c"
#include "greatest.h"

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
        ASSERT_STR_EQ(">A  <string", (ptr=markup_strip(g_strdup(">A <img> <string"))));
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

        // Test replacement of img and a tags, not renderable by pango
        ASSERT_STR_EQ("foo bar bar baz",    (ptr=markup_transform(g_strdup("<img alt=\"foo bar\"><br>bar\nbaz"), MARKUP_FULL)));
        g_free(ptr);
        ASSERT_STR_EQ("test ",              (ptr=markup_transform(g_strdup("test <img alt=\"foo bar\""), MARKUP_FULL)));
        g_free(ptr);
        ASSERT_STR_EQ("test [image] image", (ptr=markup_transform(g_strdup("test <img src=\"nothing.jpg\"> image"), MARKUP_FULL)));
        g_free(ptr);
        ASSERT_STR_EQ("bar baz",            (ptr=markup_transform(g_strdup("<a href=\"asdf\">bar</a> baz"), MARKUP_FULL)));
        g_free(ptr);

        ASSERT_STR_EQ("&#936;", (ptr=markup_transform(g_strdup("&#936;"), MARKUP_FULL)));
        free(ptr);
        ASSERT_STR_EQ("&#x3a8; &#x3A8;", (ptr=markup_transform(g_strdup("&#x3a8; &#x3A8;"), MARKUP_FULL)));
        free(ptr);
        ASSERT_STR_EQ("&gt; &lt;", (ptr=markup_transform(g_strdup("&gt; &lt;"), MARKUP_FULL)));
        free(ptr);
        ASSERT_STR_EQ("&amp;invalid; &amp;#abc; &amp;#xG;", (ptr=markup_transform(g_strdup("&invalid; &#abc; &#xG;"), MARKUP_FULL)));
        free(ptr);
        ASSERT_STR_EQ("&amp;; &amp;#; &amp;#x;", (ptr=markup_transform(g_strdup("&; &#; &#x;"), MARKUP_FULL)));
        free(ptr);

        PASS();
}

TEST helper_markup_strip_a (const char *in, const char *exp, const char *urls)
{
        // out_urls is a return parameter and the content should be ignored
        char *out_urls = (char *)0x04; //Chosen by a fair dice roll
        char *out = g_strdup(in);
        char *msg = g_strconcat("url: ", in, NULL);

        markup_strip_a(&out, &out_urls);

        ASSERT_STR_EQm(msg, exp, out);

        if (urls) {
                ASSERT_STR_EQm(msg, urls, out_urls);
        } else {
                ASSERT_EQm(msg, urls, out_urls);
        }

        g_free(out_urls);
        g_free(out);
        g_free(msg);

        PASS();
}

TEST test_markup_strip_a(void)
{
        RUN_TESTp(helper_markup_strip_a, "<a href=\"https://url.com\">valid</a> link",   "valid link", "[valid] https://url.com");
        RUN_TESTp(helper_markup_strip_a, "<a href=\"\">valid</a> link",                  "valid link", "[valid] ");
        RUN_TESTp(helper_markup_strip_a, "<a>valid</a> link",                            "valid link", NULL);
        RUN_TESTp(helper_markup_strip_a, "<a href=\"https://url.com\">valid link",       "valid link", "[valid link] https://url.com");

        RUN_TESTp(helper_markup_strip_a, "<a href=\"https://url.com\" invalid</a> link", " link",      NULL);
        RUN_TESTp(helper_markup_strip_a, "<a invalid</a> link",                          " link",      NULL);

        PASS();
}

TEST helper_markup_strip_img (const char *in, const char *exp, const char *urls)
{
        // out_urls is a return parameter and the content should be ignored
        char *out_urls = (char *)0x04; //Chosen by a fair dice roll
        char *out = g_strdup(in);
        char *msg = g_strconcat("url: ", in, NULL);

        markup_strip_img(&out, &out_urls);

        ASSERT_STR_EQm(msg, exp, out);

        if (urls) {
                ASSERT_STR_EQm(msg, urls, out_urls);
        } else {
                ASSERT_EQm(msg, urls, out_urls);
        }

        g_free(out_urls);
        g_free(out);
        g_free(msg);

        PASS();
}

TEST test_markup_strip_img(void)
{
        RUN_TESTp(helper_markup_strip_img, "v <img> img",                                         "v [image] img", NULL);
        RUN_TESTp(helper_markup_strip_img, "v <img alt=\"valid\" alt=\"invalid\"> img",           "v valid img",   NULL);
        RUN_TESTp(helper_markup_strip_img, "v <img src=\"url.com\"> img",                         "v [image] img", "[image] url.com");

        RUN_TESTp(helper_markup_strip_img, "v <img alt=\"valid\" src=\"url.com\"> img",           "v valid img",   "[valid] url.com");
        RUN_TESTp(helper_markup_strip_img, "v <img src=\"url.com\" alt=\"valid\"> img",           "v valid img",   "[valid] url.com");
        RUN_TESTp(helper_markup_strip_img, "v <img src=\"url.com\" alt=\"valid\" alt=\"i\"> img", "v valid img",   "[valid] url.com");

        RUN_TESTp(helper_markup_strip_img, "i <img alt=\"invalid  src=\"https://url.com\"> img",  "i [image] img", "[image] https://url.com");
        RUN_TESTp(helper_markup_strip_img, "i <img alt=\"broken\" src=\"https://url.com  > img",  "i broken img",  NULL);
        RUN_TESTp(helper_markup_strip_img, "i <img alt=\"invalid  src=\"https://url.com  > img",  "i [image] img", NULL);

        RUN_TESTp(helper_markup_strip_img, "i <img src=\"url.com   alt=\"broken\"> img",          "i broken img",  NULL);
        RUN_TESTp(helper_markup_strip_img, "i <img src=\"url.com\" alt=\"invalid > img",          "i [image] img", "[image] url.com");
        RUN_TESTp(helper_markup_strip_img, "i <img src=\"url.com   alt=\"invalid > img",          "i [image] img", NULL);

        RUN_TESTp(helper_markup_strip_img, "i <img src=\"url.com\" alt=\"invalid\" img",          "i ",            NULL);

        PASS();
}

SUITE(suite_markup)
{
        RUN_TEST(test_markup_strip);
        RUN_TEST(test_markup_strip_a);
        RUN_TEST(test_markup_strip_img);
        RUN_TEST(test_markup_transform);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

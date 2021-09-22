#include "../src/utils.c"
#include "greatest.h"

TEST test_string_replace_char(void)
{
        char *text = g_malloc(128 * sizeof(char));

        strcpy(text, "a aa aaa");
        ASSERT_STR_EQ("b bb bbb", string_replace_char('a', 'b', text));

        strcpy(text, "Nothing to replace");
        ASSERT_STR_EQ("Nothing to replace", string_replace_char('s', 'a', text));

        strcpy(text, "");
        ASSERT_STR_EQ("", string_replace_char('a', 'b', text));

        g_free(text);
        PASS();
}

/*
 * We trust that string_replace_all and string_replace properly reallocate
 * memory if the result is longer than the given string, no real way to test for
 * that far as I know.
 */

TEST test_string_replace_all(void)
{
        char *text = g_malloc(128 * sizeof(char));

        strcpy(text, "aaaaa");
        ASSERT_STR_EQ("bbbbb", (text = string_replace_all("a", "b", text)));

        strcpy(text, "");
        ASSERT_STR_EQ("", (text = string_replace_all("a", "b", text)));

        strcpy(text, "Nothing to replace");
        ASSERT_STR_EQ((text = string_replace_all("z", "a", text)), "Nothing to replace");

        strcpy(text, "Reverse this");
        ASSERT_STR_EQ("Reverse sith", (text = string_replace_all("this", "sith", text)));

        strcpy(text, "abcdabc");
        ASSERT_STR_EQ("xyzabcdxyzabc", (text = string_replace_all("a", "xyza", text)));

        g_free(text);
        PASS();
}

TEST test_string_append(void)
{
        char *exp;

        ASSERT_STR_EQ("text_sep_bit", (exp = string_append(g_strdup("text"), "bit", "_sep_")));
        g_free(exp);
        ASSERT_STR_EQ("textbit", (exp = string_append(g_strdup("text"), "bit", NULL)));
        g_free(exp);
        ASSERT_STR_EQ("textbit", (exp = string_append(g_strdup("text"), "bit", "")));
        g_free(exp);

        ASSERT_STR_EQ("text", (exp = string_append(g_strdup("text"), "", NULL)));
        g_free(exp);
        ASSERT_STR_EQ("text", (exp = string_append(g_strdup("text"), "", "_sep_")));
        g_free(exp);

        ASSERT_STR_EQ("b", (exp = string_append(g_strdup(""), "b", NULL)));
        g_free(exp);
        ASSERT_STR_EQ("b", (exp = string_append(NULL, "b", "_sep_")));
        g_free(exp);

        ASSERT_STR_EQ("a", (exp = string_append(g_strdup("a"), "", NULL)));
        g_free(exp);
        ASSERT_STR_EQ("a", (exp = string_append(g_strdup("a"), NULL, "_sep_")));
        g_free(exp);

        ASSERT_STR_EQ("", (exp = string_append(g_strdup(""), "", "_sep_")));
        g_free(exp);
        ASSERT_EQ(NULL, (exp = string_append(NULL, NULL, "_sep_")));
        g_free(exp);

        PASS();
}

TEST test_string_strip_quotes(void)
{
        char *exp = string_strip_quotes(NULL);
        ASSERT_FALSE(exp);

        ASSERT_STR_EQ("NewString", (exp = string_strip_quotes("NewString")));
        g_free(exp);

        ASSERT_STR_EQ("becomes unquoted", (exp = string_strip_quotes("\"becomes unquoted\"")));
        g_free(exp);

        ASSERT_STR_EQ("\"stays quoted", (exp = string_strip_quotes("\"stays quoted")));
        g_free(exp);

        ASSERT_STR_EQ("stays quoted\"", (exp = string_strip_quotes("stays quoted\"")));
        g_free(exp);

        ASSERT_STR_EQ("stays \"quoted\"", (exp = string_strip_quotes("stays \"quoted\"")));
        g_free(exp);

        ASSERT_STR_EQ(" \"stays quoted\"", (exp = string_strip_quotes(" \"stays quoted\"")));
        g_free(exp);

        PASS();
}

TEST test_string_strip_delimited(void)
{
        char *text = g_malloc(128 * sizeof(char));

        strcpy(text, "A <simple> string_strip_delimited test");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("A  string_strip_delimited test", text);

        strcpy(text, "Remove <blink>html <b><i>tags</i></b></blink>");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("Remove html tags", text);

        strcpy(text, "<Return empty string if there is nothing left>");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("", text);

        strcpy(text, "Nothing is done if there are no delimiters in the string");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("Nothing is done if there are no delimiters in the string", text);

        strcpy(text, "We <3 dunst");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("We <3 dunst", text);

        strcpy(text, "<b>We</b> <3 dunst");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("We <3 dunst", text);

        strcpy(text, "dunst > the rest");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("dunst > the rest", text);

        g_free(text);
        PASS();
}

TEST test_string_to_path(void)
{
        char *ptr, *exp;
        char *home = getenv("HOME");

        exp = "/usr/local/bin/script";
        ASSERT_STR_EQ(exp, (ptr = string_to_path(g_strdup(exp))));
        free(ptr);

        exp = "~path/with/wrong/tilde";
        ASSERT_STR_EQ(exp, (ptr = string_to_path(g_strdup(exp))));
        free(ptr);

        ASSERT_STR_EQ((exp = g_strconcat(home, "/.path/with/tilde", NULL)),
                      (ptr = string_to_path(g_strdup("~/.path/with/tilde"))));
        free(exp);
        free(ptr);

        ASSERT_STR_EQ((exp = g_strconcat(home, "/.path/with/tilde and some space", NULL)),
                      (ptr = string_to_path(g_strdup("~/.path/with/tilde and some space"))));
        free(exp);
        free(ptr);

        PASS();
}

TEST test_string_to_time(void)
{
        char *input[] = { "5000 ms", "5000ms",  "100", "10s",   "2m",    "11h",      "9d", "   5 ms   ", NULL };
        gint64  exp[] = {      5000,     5000, 100000, 10000, 120000, 39600000, 777600000,     5,         0};

        int i = 0;
        while (input[i]){
                ASSERT_EQ_FMT(string_to_time(input[i]), exp[i]*1000, "%ld");
                i++;
        }

        PASS();
}

SUITE(suite_utils)
{
        RUN_TEST(test_string_replace_char);
        RUN_TEST(test_string_replace_all);
        RUN_TEST(test_string_append);
        RUN_TEST(test_string_strip_quotes);
        RUN_TEST(test_string_strip_delimited);
        RUN_TEST(test_string_to_path);
        RUN_TEST(test_string_to_time);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

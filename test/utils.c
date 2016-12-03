#include "greatest.h"
#include "src/utils.h"

TEST test_string_replace_char(void)
{
        char *text = malloc(128 * sizeof(char));
        strcpy(text, "a aa aaa");
        ASSERT_STR_EQ("b bb bbb", string_replace_char('a', 'b', text));

        strcpy(text, "Nothing to replace");
        ASSERT_STR_EQ("Nothing to replace", string_replace_char('s', 'a', text));

        strcpy(text, "");
        ASSERT_STR_EQ("", string_replace_char('a', 'b', text));
        free(text);

        PASS();
}

/*
 * We trust that string_replace_all and string_replace properly reallocate
 * memory if the result is longer than the given string, no real way to test for
 * that far as I know.
 */

TEST test_string_replace_all(void)
{

        char *text = malloc(128 * sizeof(char));
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

        free(text);
        PASS();
}

TEST test_string_replace(void)
{
        char *text = malloc(128 * sizeof(char));
        strcpy(text, "aaaaa");
        ASSERT_STR_EQ("baaaa", (text = string_replace("a", "b", text)) );

        strcpy(text, "");
        ASSERT_STR_EQ((text = string_replace("a", "b", text)), "");

        strcpy(text, "Nothing to replace");
        ASSERT_STR_EQ((text = string_replace("z", "a", text)), "Nothing to replace");

        strcpy(text, "Reverse this");
        ASSERT_STR_EQ("Reverse sith", (text = string_replace("this", "sith", text)));

        strcpy(text, "abcdabc");
        ASSERT_STR_EQ("xyzabcdabc", (text = string_replace("a", "xyza", text)));

        free(text);
        PASS();
}

TEST test_string_append(void)
{
        SKIP(); //TODO: Implement this
        PASS();
}

TEST test_string_to_argv(void)
{
        char **argv = string_to_argv("argv");
        ASSERT_STR_EQ("argv", argv[0]);
        ASSERT_EQ(    NULL,   argv[1]);
        free(argv);
        argv = NULL;

        argv = string_to_argv("echo test");
        ASSERT_STR_EQ("echo", argv[0]);
        ASSERT_STR_EQ("test", argv[1]);
        ASSERT_EQ(    NULL,   argv[2]);
        free(argv);
        argv = NULL;

        argv = string_to_argv("");
        ASSERT_EQ(    NULL,   argv[0]);
        free(argv);

        PASS();
}

TEST test_string_strip_delimited(void)
{
        char *text = malloc(128 * sizeof(char));

        strcpy(text, "A <simple> string_strip_delimited test");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("A  string_strip_delimited test", text);

        strcpy(text, "Remove <blink>html <b><i>tags</i></b></blink>");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("Remove html tags", text);

        strcpy(text, "Calls|with|identical|delimiters|are|handled|properly");
        string_strip_delimited(text, '|', '|');
        ASSERT_STR_EQ("Calls", text);

        strcpy(text, "<Return empty string if there is nothing left>");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("", text);

        strcpy(text, "Nothing is done if there are no delimiters in the string");
        string_strip_delimited(text, '<', '>');
        ASSERT_STR_EQ("Nothing is done if there are no delimiters in the string", text);

        free(text);
        PASS();
}

SUITE(suite_utils)
{
        RUN_TEST(test_string_replace_char);
        RUN_TEST(test_string_replace_all);
        RUN_TEST(test_string_replace);
        RUN_TEST(test_string_append);
        RUN_TEST(test_string_to_argv);
        RUN_TEST(test_string_strip_delimited);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

#include "greatest.h"
#include "src/utils.h"

TEST test_string_replace_char(void)
{
        char *text = calloc(128, sizeof(char));
        strcpy(text, "a aa aaa");
        ASSERT_STR_EQ(string_replace_char('a', 'b', text), "b bb bbb");
        strcpy(text, "");
        ASSERT_STR_EQ(string_replace_char('a', 'b', text), "");
        free(text);
        PASS();
}

SUITE(utils)
{
        RUN_TEST(test_string_replace_char);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

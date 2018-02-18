#include "greatest.h"

#include <stdbool.h>
#include <glib.h>

#include "src/option_parser.h"

TEST test_next_section(void)
{
        const char *section = NULL;
        ASSERT_STR_EQ("bool", (section = next_section(section)));
        ASSERT_STR_EQ("string", (section = next_section(section)));
        ASSERT_STR_EQ("path", (section = next_section(section)));
        ASSERT_STR_EQ("int", (section = next_section(section)));
        ASSERT_STR_EQ("double", (section = next_section(section)));
        PASS();
}

TEST test_ini_get_string(void)
{
        const char *string_section = "string";
        const char *ptr;
        ASSERT_STR_EQ("A simple string", (ptr = ini_get_string(string_section, "simple", "")));

        ASSERT_STR_EQ("A quoted string", (ptr = ini_get_string(string_section, "quoted", "")));
        ASSERT_STR_EQ("A string \"with quotes\"", (ptr = ini_get_string(string_section, "quoted_with_quotes", "")));

        ASSERT_STR_EQ("default value", (ptr = ini_get_string(string_section, "nonexistent", "default value")));

        PASS();
}

TEST test_ini_get_double(void)
{
        char *double_section = "double";
        ASSERT_EQ(1, ini_get_double(double_section, "simple", 0));
        ASSERT_EQ(1.5, ini_get_double(double_section, "decimal", 0));
        ASSERT_EQ(-1.2, ini_get_double(double_section, "negative", 0));
        ASSERT_EQ(0.005, ini_get_double(double_section, "zeroes", 0));
        ASSERT_EQ(3.141592653589793, ini_get_double(double_section, "long", 0));

        ASSERT_EQ(10.5, ini_get_double(double_section, "nonexistent", 10.5));
        PASS();
}

TEST test_cmdline_get_string(void)
{
        const char *ptr;
        ASSERT_STR_EQ("A simple string from the cmdline", (ptr =cmdline_get_string("-string", "", "")));
        ASSERT_STR_EQ("Single_word_string", (ptr = cmdline_get_string("-str/-s", "", "")));
        ASSERT_STR_EQ("Default", (ptr = cmdline_get_string("-nonexistent", "Default", "")));
        PASS();
}

TEST test_cmdline_get_double(void)
{
        ASSERT_EQ(2, cmdline_get_double("-simple_double", 0, ""));
        ASSERT_EQ(5.2, cmdline_get_double("-double", 0, ""));
        ASSERT_EQ(3.14, cmdline_get_double("-nonexistent", 3.14, ""));
        PASS();
}

TEST test_cmdline_create_usage(void)
{
        cmdline_get_string("-msgstring/-ms", "", "A string to test usage creation");
        cmdline_get_double("-msgdouble/-md", 0, "A double to test usage creation");
        const char *usage = cmdline_create_usage();
        ASSERT(strstr(usage, "-msgstring/-ms"));
        ASSERT(strstr(usage, "A string to test usage creation"));
        ASSERT(strstr(usage, "-msgdouble/-md"));
        ASSERT(strstr(usage, "A double to test usage creation"));
        PASS();
}

TEST test_option_get_string(void)
{
        const char *string_section = "string";
        const char *ptr;

        ASSERT_STR_EQ("A simple string", (ptr =option_get_string(string_section, "simple", "-nonexistent", "", "")));
        ASSERT_STR_EQ("Single_word_string", (ptr = option_get_string(string_section, "simple", "-str/-s", "", "")));
        ASSERT_STR_EQ("A simple string from the cmdline", (ptr = option_get_string(string_section, "simple", "-string", "", "")));
        ASSERT_STR_EQ("A simple string from the cmdline", (ptr = option_get_string(string_section, "simple", "-string/-s", "", "")));
        ASSERT_STR_EQ("Single_word_string", (ptr = option_get_string(string_section, "simple", "-s", "", "")));
        ASSERT_STR_EQ("Default", (ptr = option_get_string(string_section, "nonexistent", "-nonexistent", "Default", "")));
        PASS();
}

TEST test_option_get_double(void)
{
        char *double_section = "double";
        ASSERT_EQ(2, option_get_double(double_section, "simple", "-simple_double", 0, ""));
        ASSERT_EQ(5.2, option_get_double(double_section, "simple", "-double", 0, ""));
        ASSERT_EQ(0.005, option_get_double(double_section, "zeroes", "-nonexistent", 0, ""));
        ASSERT_EQ(10.5, option_get_double(double_section, "nonexistent", "-nonexistent", 10.5, ""));
        PASS();
}

SUITE(suite_option_parser)
{
        FILE *config_file = fopen("data/test-ini", "r");
        if (config_file == NULL) {
                fputs("\nTest config file 'data/test-ini' couldn't be opened, failing.\n", stderr);
                exit(1);
        }
        load_ini_file(config_file);
        RUN_TEST(test_next_section);
        RUN_TEST(test_ini_get_string);
        RUN_TEST(test_ini_get_double);
        char cmdline[] = "dunst -b "
                "-string \"A simple string from the cmdline\" -s Single_word_string "
                "-int 3 -i 2 -negative -7 -zeroes 04 -intdecim 2.5 "
                "-path ~/path/from/cmdline "
                "-simple_double 2 -double 5.2"
                ;
        int argc;
        char **argv;
        g_shell_parse_argv(&cmdline[0], &argc, &argv, NULL);
        cmdline_load(argc, argv);
        RUN_TEST(test_cmdline_get_string);
        RUN_TEST(test_cmdline_get_double);
        RUN_TEST(test_cmdline_create_usage);

        RUN_TEST(test_option_get_string);
        RUN_TEST(test_option_get_double);
        free_ini();
        g_strfreev(argv);
        fclose(config_file);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

#include "../src/option_parser.c"
#include "greatest.h"

extern const char *base;

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

TEST test_ini_get_bool(void)
{
        char *bool_section = "bool";
        ASSERT(ini_get_bool(bool_section, "booltrue", false));
        ASSERT(ini_get_bool(bool_section, "booltrue_capital", false));

        ASSERT_FALSE(ini_get_bool(bool_section, "boolfalse", true));
        ASSERT_FALSE(ini_get_bool(bool_section, "boolfalse_capital", true));

        ASSERT(ini_get_bool(bool_section, "boolyes", false));
        ASSERT(ini_get_bool(bool_section, "boolyes_capital", false));

        ASSERT_FALSE(ini_get_bool(bool_section, "boolno", true));
        ASSERT_FALSE(ini_get_bool(bool_section, "boolno_capital", true));

        ASSERT(ini_get_bool(bool_section, "boolbin1", false));
        ASSERT_FALSE(ini_get_bool(bool_section, "boolbin0", true));

        ASSERT(ini_get_bool(bool_section, "boolinvalid", true));
        ASSERT_FALSE(ini_get_bool(bool_section, "boolinvalid", false));

        ASSERT(ini_get_bool(bool_section, "nonexistent", true));
        ASSERT_FALSE(ini_get_bool(bool_section, "nonexistent", false));
        PASS();
}

TEST test_ini_get_string(void)
{
        char *string_section = "string";
        char *ptr;
        ASSERT_STR_EQ("A simple string", (ptr = ini_get_string(string_section, "simple", "")));
        free(ptr);

        ASSERT_STR_EQ("A quoted string", (ptr = ini_get_string(string_section, "quoted", "")));
        free(ptr);
        ASSERT_STR_EQ("A string \"with quotes\"", (ptr = ini_get_string(string_section, "quoted_with_quotes", "")));
        free(ptr);
        ASSERT_STR_EQ("A\" string with quotes\"", (ptr = ini_get_string(string_section, "unquoted_with_quotes", "")));
        free(ptr);

        ASSERT_STR_EQ("String with a", (ptr = ini_get_string(string_section, "quoted_comment", "")));
        free(ptr);
        ASSERT_STR_EQ("String with a", (ptr = ini_get_string(string_section, "unquoted_comment", "")));
        free(ptr);
        ASSERT_STR_EQ("#ffffff", (ptr = ini_get_string(string_section, "color_comment", "")));
        free(ptr);


        ASSERT_STR_EQ("default value", (ptr = ini_get_string(string_section, "nonexistent", "default value")));
        free(ptr);

        PASS();
}

TEST test_ini_get_path(void)
{
        char *section = "path";
        char *ptr, *exp;
        char *home = getenv("HOME");

        // return default, if nonexistent key
        ASSERT_EQ(NULL, (ptr = ini_get_path(section, "nonexistent", NULL)));
        ASSERT_STR_EQ("default", (ptr = ini_get_path(section, "nonexistent", "default")));
        g_free(ptr);

        // return path with replaced home
        ASSERT_STR_EQ((exp = g_strconcat(home, "/.path/to/tilde", NULL)),
                      (ptr = ini_get_path(section, "expand_tilde", NULL)));
        g_free(ptr);
        g_free(exp);

        PASS();
}


TEST test_ini_get_int(void)
{
        char *int_section = "int";

        ASSERT_EQ(5, ini_get_int(int_section, "simple", 0));
        ASSERT_EQ(-10, ini_get_int(int_section, "negative", 0));
        ASSERT_EQ(2, ini_get_int(int_section, "decimal", 0));
        ASSERT_EQ(7, ini_get_int(int_section, "leading_zeroes", 0));
        ASSERT_EQ(1024, ini_get_int(int_section, "multi_char", 0));

        ASSERT_EQ(10, ini_get_int(int_section, "nonexistent", 10));
        PASS();
}

TEST test_ini_get_double(void)
{
        if (2.3 != atof("2.3")) {
                SKIPm("Skipping test_ini_get_double, as it seems we're running under musl+valgrind!");
        }

        char *double_section = "double";
        ASSERT_EQ(1, ini_get_double(double_section, "simple", 0));
        ASSERT_EQ(1.5, ini_get_double(double_section, "decimal", 0));
        ASSERT_EQ(-1.2, ini_get_double(double_section, "negative", 0));
        ASSERT_EQ(0.005, ini_get_double(double_section, "zeroes", 0));
        ASSERT_EQ(3.141592653589793, ini_get_double(double_section, "long", 0));

        ASSERT_EQ(10.5, ini_get_double(double_section, "nonexistent", 10.5));
        PASS();
}

TEST test_cmdline_get_path(void)
{
        char *ptr, *exp;
        char *home = getenv("HOME");

        // return default, if nonexistent key
        ASSERT_EQ(NULL, (ptr = cmdline_get_path("-nonexistent", NULL, "desc")));
        ASSERT_STR_EQ("default", (ptr = cmdline_get_path("-nonexistent", "default", "desc")));
        g_free(ptr);

        // return path with replaced home
        ASSERT_STR_EQ((exp = g_strconcat(home, "/path/from/cmdline", NULL)),
                      (ptr = cmdline_get_path("-path", NULL, "desc")));
        g_free(ptr);
        g_free(exp);

        PASS();
}

TEST test_cmdline_get_string(void)
{
        char *ptr;
        ASSERT_STR_EQ("A simple string from the cmdline", (ptr =cmdline_get_string("-string", "", "")));
        free(ptr);
        ASSERT_STR_EQ("Single_word_string", (ptr = cmdline_get_string("-str/-s", "", "")));
        free(ptr);
        ASSERT_STR_EQ("Default", (ptr = cmdline_get_string("-nonexistent", "Default", "")));
        free(ptr);
        PASS();
}

TEST test_cmdline_get_int(void)
{
        ASSERT_EQ(3, cmdline_get_int("-int", 0, ""));
        ASSERT_EQ(2, cmdline_get_int("-int2/-i", 0, ""));
        ASSERT_EQ(-7, cmdline_get_int("-negative", 0, ""));
        ASSERT_EQ(4, cmdline_get_int("-zeroes", 0, ""));
        ASSERT_EQ(2, cmdline_get_int("-intdecim", 0, ""));
        ASSERT_EQ(10, cmdline_get_int("-nonexistent", 10, ""));
        PASS();
}

TEST test_cmdline_get_double(void)
{
        if (2.3 != atof("2.3")) {
                SKIPm("Skipping test_cmdline_get_double, as it seems we're running under musl+valgrind!");
        }

        ASSERT_EQ(2, cmdline_get_double("-simple_double", 0, ""));
        ASSERT_EQ(5.2, cmdline_get_double("-double", 0, ""));
        ASSERT_EQ(3.14, cmdline_get_double("-nonexistent", 3.14, ""));
        PASS();
}

TEST test_cmdline_get_bool(void)
{
        ASSERT(cmdline_get_bool("-bool", false, ""));
        ASSERT(cmdline_get_bool("-shortbool/-b", false, ""));
        ASSERT(cmdline_get_bool("-boolnd/-n", true, ""));
        ASSERT_FALSE(cmdline_get_bool("-boolnd/-n", false, ""));
        PASS();
}

TEST test_cmdline_create_usage(void)
{
        g_free(cmdline_get_string("-msgstring/-ms", "", "A string to test usage creation"));
        cmdline_get_int("-msgint/-mi", 0, "An int to test usage creation");
        cmdline_get_double("-msgdouble/-md", 0, "A double to test usage creation");
        cmdline_get_bool("-msgbool/-mb", false, "A bool to test usage creation");
        const char *usage = cmdline_create_usage();
        ASSERT(strstr(usage, "-msgstring/-ms"));
        ASSERT(strstr(usage, "A string to test usage creation"));
        ASSERT(strstr(usage, "-msgint/-mi"));
        ASSERT(strstr(usage, "An int to test usage creation"));
        ASSERT(strstr(usage, "-msgdouble/-md"));
        ASSERT(strstr(usage, "A double to test usage creation"));
        ASSERT(strstr(usage, "-msgbool/-mb"));
        ASSERT(strstr(usage, "A bool to test usage creation"));
        PASS();
}

TEST test_option_get_string(void)
{
        char *string_section = "string";
        char *ptr;

        ASSERT_STR_EQ("A simple string", (ptr =option_get_string(string_section, "simple", "-nonexistent", "", "")));
        free(ptr);
        ASSERT_STR_EQ("Single_word_string", (ptr = option_get_string(string_section, "simple", "-str/-s", "", "")));
        free(ptr);
        ASSERT_STR_EQ("A simple string from the cmdline", (ptr = option_get_string(string_section, "simple", "-string", "", "")));
        free(ptr);
        ASSERT_STR_EQ("A simple string from the cmdline", (ptr = option_get_string(string_section, "simple", "-string/-s", "", "")));
        free(ptr);
        ASSERT_STR_EQ("Single_word_string", (ptr = option_get_string(string_section, "simple", "-s", "", "")));
        free(ptr);
        ASSERT_STR_EQ("Default", (ptr = option_get_string(string_section, "nonexistent", "-nonexistent", "Default", "")));
        free(ptr);
        PASS();
}

TEST test_option_get_path(void)
{
        char *section = "path";
        char *ptr, *exp;
        char *home = getenv("HOME");

        // invalid ini, invalid cmdline
        ASSERT_EQ(NULL, (ptr = option_get_path(section, "nonexistent", "-nonexistent", NULL, "desc")));
        ASSERT_STR_EQ("default", (ptr = option_get_path(section, "nonexistent", "-nonexistent", "default", "desc")));
        free(ptr);

        //   valid ini, invalid cmdline
        ASSERT_STR_EQ((exp = g_strconcat(home, "/.path/to/tilde", NULL)),
                      (ptr = option_get_path(section, "expand_tilde", "-nonexistent", NULL, "desc")));
        g_free(exp);
        g_free(ptr);

        //   valid ini,   valid cmdline
        ASSERT_STR_EQ((exp = g_strconcat(home, "/path/from/cmdline", NULL)),
                      (ptr = option_get_path(section, "expand_tilde", "-path", NULL, "desc")));
        g_free(exp);
        g_free(ptr);

        // invalid ini,   valid cmdline
        ASSERT_STR_EQ((exp = g_strconcat(home, "/path/from/cmdline", NULL)),
                      (ptr = option_get_path(section, "nonexistent", "-path", NULL, "desc")));
        g_free(exp);
        g_free(ptr);

        PASS();
}

TEST test_option_get_int(void)
{
        char *int_section = "int";
        ASSERT_EQ(3,  option_get_int(int_section, "negative", "-int", 0, ""));
        ASSERT_EQ(2,  option_get_int(int_section, "simple", "-int2/-i", 0, ""));
        ASSERT_EQ(-7, option_get_int(int_section, "decimal", "-negative", 0, ""));
        ASSERT_EQ(4,  option_get_int(int_section, "simple", "-zeroes", 0, ""));
        ASSERT_EQ(2,  option_get_int(int_section, "simple", "-intdecim", 0, ""));

        ASSERT_EQ(5, option_get_int(int_section, "simple", "-nonexistent", 0, ""));
        ASSERT_EQ(-10, option_get_int(int_section, "negative", "-nonexistent", 0, ""));
        ASSERT_EQ(2, option_get_int(int_section, "decimal", "-nonexistent", 0, ""));
        ASSERT_EQ(7, option_get_int(int_section, "leading_zeroes", "-nonexistent", 0, ""));
        ASSERT_EQ(1024, option_get_int(int_section, "multi_char", "-nonexistent", 0, ""));

        ASSERT_EQ(3, option_get_int(int_section, "nonexistent", "-nonexistent", 3, ""));
        PASS();
}

TEST test_option_get_double(void)
{
        if (2.3 != atof("2.3")) {
                SKIPm("Skipping test_option_get_double, as it seems we're running under musl+valgrind!");
        }

        char *double_section = "double";
        ASSERT_EQ(2, option_get_double(double_section, "simple", "-simple_double", 0, ""));
        ASSERT_EQ(5.2, option_get_double(double_section, "simple", "-double", 0, ""));
        ASSERT_EQ(0.005, option_get_double(double_section, "zeroes", "-nonexistent", 0, ""));
        ASSERT_EQ(10.5, option_get_double(double_section, "nonexistent", "-nonexistent", 10.5, ""));
        PASS();
}

TEST test_option_get_bool(void)
{
        char *bool_section = "bool";
        ASSERT(option_get_bool(bool_section, "boolfalse", "-bool/-b", false, ""));
        ASSERT(option_get_bool(bool_section, "boolbin1", "-nonexistent", false, ""));
        ASSERT_FALSE(option_get_bool(bool_section, "boolbin0", "-nonexistent", false, ""));
        ASSERT_FALSE(option_get_bool(bool_section, "nonexistent", "-nonexistent", false, ""));
        PASS();
}

SUITE(suite_option_parser)
{
        char *config_path = g_strconcat(base, "/data/test-ini", NULL);
        FILE *config_file = fopen(config_path, "r");
        if (!config_file) {
                fputs("\nTest config file 'data/test-ini' couldn't be opened, failing.\n", stderr);
                exit(1);
        }
        load_ini_file(config_file);
        RUN_TEST(test_next_section);
        RUN_TEST(test_ini_get_bool);
        RUN_TEST(test_ini_get_string);
        RUN_TEST(test_ini_get_path);
        RUN_TEST(test_ini_get_int);
        RUN_TEST(test_ini_get_double);
        char cmdline[] = "dunst -bool -b "
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
        RUN_TEST(test_cmdline_get_path);
        RUN_TEST(test_cmdline_get_int);
        RUN_TEST(test_cmdline_get_double);
        RUN_TEST(test_cmdline_get_bool);
        RUN_TEST(test_cmdline_create_usage);

        RUN_TEST(test_option_get_string);
        RUN_TEST(test_option_get_path);
        RUN_TEST(test_option_get_int);
        RUN_TEST(test_option_get_double);
        RUN_TEST(test_option_get_bool);

        g_free(config_path);
        free_ini();
        g_strfreev(argv);
        fclose(config_file);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

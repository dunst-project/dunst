#include "../src/option_parser.c"
#include "greatest.h"

extern const char *base;

TEST test_next_section(void)
{
        const char *section = NULL;
        ASSERT_STR_EQ("bool", (section = next_section(section)));
        ASSERT_STR_EQ("string", (section = next_section(section)));
        ASSERT_STR_EQ("list", (section = next_section(section)));
        ASSERT_STR_EQ("path", (section = next_section(section)));
        ASSERT_STR_EQ("int", (section = next_section(section)));
        ASSERT_STR_EQ("double", (section = next_section(section)));
        PASS();
}

enum greatest_test_res ARRAY_EQ(char **a, char **b){
        ASSERT(a);
        ASSERT(b);
        int i = 0;
        while (a[i] && b[i]){
                ASSERT_STR_EQ(a[i], b[i]);
                i++;
        }
        ASSERT_FALSE(a[i]);
        ASSERT_FALSE(b[i]);
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

TEST test_cmdline_get_list(void)
{
        char **ptr;
        char *cmp1[] = {"A", "simple", "list", "from", "the", "cmdline", NULL};
        char *cmp2[] = {"A", "list", "with", "spaces", NULL};
        char *cmp3[] = {"A", "default", "list", NULL};

        CHECK_CALL(ARRAY_EQ(cmp1, (ptr = cmdline_get_list("-list", "", ""))));
        g_strfreev(ptr);
        CHECK_CALL(ARRAY_EQ(cmp2, (ptr = cmdline_get_list("-list2", "", ""))));
        g_strfreev(ptr);
        CHECK_CALL(ARRAY_EQ(cmp3, (ptr = cmdline_get_list("-nonexistent", "A, default, list", ""))));
        g_strfreev(ptr);
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
        char cmdline[] = "dunst -bool -b "
                "-string \"A simple string from the cmdline\" -s Single_word_string "
                "-list A,simple,list,from,the,cmdline -list2 \"A, list, with, spaces\" "
                "-int 3 -i 2 -negative -7 -zeroes 04 -intdecim 2.5 "
                "-path ~/path/from/cmdline "
                "-simple_double 2 -double 5.2"
                ;
        int argc;
        char **argv;
        g_shell_parse_argv(&cmdline[0], &argc, &argv, NULL);
        cmdline_load(argc, argv);
        RUN_TEST(test_cmdline_get_string);
        RUN_TEST(test_cmdline_get_list);
        RUN_TEST(test_cmdline_get_path);
        RUN_TEST(test_cmdline_get_int);
        RUN_TEST(test_cmdline_get_double);
        RUN_TEST(test_cmdline_get_bool);
        RUN_TEST(test_cmdline_create_usage);

        g_free(config_path);
        free_ini();
        g_strfreev(argv);
        fclose(config_file);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

#include "../src/option_parser.c"
#include "greatest.h"

#define ARRAY_SAME_LENGTH(a, b) { \
        ASSERT_EQm("Test is invalid. Input data has to be the same length",\
                        G_N_ELEMENTS(a), G_N_ELEMENTS(b));\
}

enum greatest_test_res ARRAY_EQ(char **a, char **b){
        ASSERT(a);
        ASSERT(b);
        int i = 0;
        while (a[i] && b[i]) {
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

TEST test_string_to_int(void)
{
        int val = -123;
        const char* inputs[] = {
                "0",
                "1",
                "-1",
                "12345678",
                "-12345678"
        };
        const int results[] = {
                0,
                1,
                -1,
                12345678,
                -12345678
        };

        ARRAY_SAME_LENGTH(inputs, results);

        struct setting s;
        s.type = TYPE_INT;

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERTm(buf, set_from_string(&val, s, inputs[i]));
                ASSERT_EQm(buf, val, results[i]);
        }
        PASS();
}

TEST test_string_to_int_invalid(void)
{
        int val = -123;
        const char* inputs[] = {
                "a0",
                "something",
                "x674asdf",
                "-dsf4544asdf",
                "0x145",
                "64a567",
                "(5678)",
                "5678)",
        };

        struct setting s;
        s.type = TYPE_INT;
        s.name = "test_int";

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERT_FALSEm(buf, set_from_string(&val, s, inputs[i]));
        }
        ASSERT_EQm("Value should not be changed for invalid ints", val, -123);
        PASS();
}

TEST test_string_to_double(void)
{
        if (2.3 != atof("2.3")) {
                SKIPm("Skipping test_string_to_double, as it seems we're running under musl+valgrind!");
        }

        double val = -100.0;
        const char* inputs[] = {
                "0",
                "1",
                "-1",
                "45.8",
                "-45.8"
        };
        const double results[] = {
                0,
                1,
                -1,
                45.8,
                -45.8
        };
        struct setting s;
        s.type = TYPE_DOUBLE;

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERTm(buf, set_from_string(&val, s, inputs[i]));
                ASSERT_EQm(buf, val, results[i]);
        }
        PASS();
}

TEST test_string_to_double_invalid(void)
{
        double val = -100.0;
        const char* inputs[] = {
                "a0",
                "something",
                "x1234asdf",
                "-dsf1234asdf",
                "1234a567",
                "12.34a567",
                "56.7.1",
        };

        struct setting s;
        s.type = TYPE_DOUBLE;
        s.name = "test_double";

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERT_FALSEm(buf, set_from_string(&val, s, inputs[i]));
        }
        ASSERT_EQm("Value should not be changed for invalid doubles", val, -100.0);
        PASS();
}

TEST test_string_to_boolean(void)
{
        bool val;

        struct setting s;
        s.type = TYPE_CUSTOM;
        s.parser = string_parse_bool;
        s.parser_data = boolean_enum_data;
        s.value = &val;

        const char* inputs[] = {
                "0",
                "1",
                "true",
                "True",
                "false",
                "off"
        };
        const int results[] = {
                0,
                1,
                1,
                1,
                0,
                0
        };

        ARRAY_SAME_LENGTH(inputs, results);

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERTm(buf, set_from_string(&val, s, inputs[i]));
                sprintf(buf, "Failed in round %i. %i should be %i", i, val, results[i]);
                ASSERT_EQm(buf, val, results[i]);
        }
        PASS();
}

TEST test_string_to_boolean_invalid(void)
{
        bool val = true;

        struct setting s = {0};
        s.type = TYPE_CUSTOM;
        s.parser = string_parse_bool;
        s.parser_data = boolean_enum_data;
        s.value = &val;
        s.name = "test_boolean";

        const char* invalid_inputs[] = {
                "-1",
                "123",
                "something",
                "else",
        };

        char buf[50];

        for (int i = 0; i < G_N_ELEMENTS(invalid_inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                bool success = set_from_string(&val, s, invalid_inputs[i]);
                ASSERT_FALSEm(buf, success);
        }
        ASSERT_EQm("Boolean should not change from invalid values", val, true);
        PASS();
}

TEST test_string_to_enum(void)
{
        int val = -123;

        struct setting s;
        s.type = TYPE_CUSTOM;
        s.value = &val;
        s.parser = string_parse_enum;
        s.parser_data = ellipsize_enum_data;

        char buf[50];

        // do not go until last element, since it's ENUM_END (all 0)
        for (int i = 0; i < G_N_ELEMENTS(ellipsize_enum_data)-1; i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERTm(buf, set_from_string(&val, s, ellipsize_enum_data[i].string));
                ASSERT_EQm(buf, val, ellipsize_enum_data[i].enum_value);
        }
        PASS();
}

TEST test_string_to_enum_invalid(void)
{
        int val = -123;

        struct setting s;
        s.name = "test_enum";
        s.type = TYPE_CUSTOM;
        s.value = &val;
        s.parser = string_parse_enum;
        s.parser_data = ellipsize_enum_data;

        const char* invalid_inputs[] = {
                "0",
                "1",
                "-1",
                "something",
                "else"
        };

        char buf[50];

        for (int i = 0; i < G_N_ELEMENTS(invalid_inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERT_FALSEm(buf, set_from_string(&val, s, invalid_inputs[i]));
        }
        ASSERT_EQm("Enum should not change from invalid values", val, -123);
        PASS();
}

int get_list_len(const enum mouse_action *in) {
        int len = 0;
        while (in[len] != MOUSE_ACTION_END)
                len++;
        return len;
}

TEST test_string_to_list(void)
{
        enum mouse_action *val = NULL;

        struct setting s;
        s.type = TYPE_LIST;
        s.value = &val;
        s.parser_data = GINT_TO_POINTER(MOUSE_LIST);

        const char* inputs[] = {
                "close_current",
                "none",
                "none, close_current",
                "close_all,close_current",
                "close_all,close_current,close_all",
        };
        const enum mouse_action results[][4] = {
                {MOUSE_CLOSE_CURRENT, MOUSE_ACTION_END},
                {MOUSE_NONE, MOUSE_ACTION_END},
                {MOUSE_NONE, MOUSE_CLOSE_CURRENT, MOUSE_ACTION_END},
                {MOUSE_CLOSE_ALL, MOUSE_CLOSE_CURRENT, MOUSE_ACTION_END},
                {MOUSE_CLOSE_ALL, MOUSE_CLOSE_CURRENT, MOUSE_CLOSE_ALL, MOUSE_ACTION_END},
        };

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERTm(buf, set_from_string(s.value, s, inputs[i]));
                ASSERT_EQm(buf, get_list_len(val), get_list_len(results[i]));
                for (int j = 0; val[j] != MOUSE_ACTION_END; j++) {
                        sprintf(buf, "Failed in round %i, element %i. Is %i, should be %i", i, j, val[j], results[i][j]);
                        ASSERT_EQm(buf, val[j], results[i][j]);
                }
        }
        free(val);
        PASS();
}

TEST test_string_to_list_invalid(void)
{
        enum mouse_action val_initial[] = {123, MOUSE_ACTION_END};
        enum mouse_action *val;

        // set the list to some initial value
        int len = get_list_len(val_initial);
        val = g_malloc_n(len+1, sizeof(enum mouse_action));
        for (int i = 0; i < len + 1; i++) {
                val[i] = val_initial[i];
        }

        struct setting s;
        s.name = "test_list";
        s.type = TYPE_LIST;
        s.value = &val;
        s.parser_data = GINT_TO_POINTER(MOUSE_LIST);

        const char* invalid_inputs[] = {
                "0",
                "1",
                "-1",
                "something",
                "else"
                "close_all,close_current,close_all,invalid",
                "close_all,invalid,close_current,close_all",
        };

        char buf[256];

        for (int i = 0; i < G_N_ELEMENTS(invalid_inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERT_FALSEm(buf, set_from_string(&val, s, invalid_inputs[i]));
        }
        sprintf(buf,"List should not change from invalid values. Expected length %i, got %i", len, get_list_len(val));
        ASSERT_EQm(buf, get_list_len(val), len);
        ASSERT_EQm("List should not change from invalid values", (int) val[0], 123);
        g_free(val);
        PASS();
}

TEST test_string_to_time(void)
{
        gint64 val;
        struct setting s;
        s.type = TYPE_TIME;
        s.value = &val;
        s.name = "test_time";

        const char* inputs[] = {
                "-1",
                "0",
                "1",
                "3s",
                "100ms",
                "2m",
        };
        const int results[] = {
                S2US(-1),
                S2US(0),
                S2US(1),
                S2US(3),
                100 * 1000,
                S2US(120),
        };

        ARRAY_SAME_LENGTH(inputs, results);

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERTm(buf, set_from_string(&val, s, inputs[i]));
                sprintf(buf, "Failed in round %i. %li should be %i", i, val, results[i]);
                ASSERT_EQm(buf, val, results[i]);
        }
        PASS();
}

TEST test_string_to_time_invalid(void)
{
        gint64 val = 1234;
        struct setting s;
        s.type = TYPE_TIME;
        s.value = &val;

        const char* invalid_inputs[] = {
                // -1 is allowed, but only without suffix
                "-1s",
                "-1ms",
                "-2",
                "-4s",
                "-2ms",
                "3basdf",
                "100asdf",
                "anything",
                "s",
        };

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(invalid_inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERT_FALSEm(buf, set_from_string(&val, s, invalid_inputs[i]));
        }
        ASSERT_EQm("Time should not change from invalid values", val, 1234);
        PASS();
}

TEST test_string_to_path(void)
{
        char *val = NULL;
        char **val2 = NULL;
        struct setting s;
        s.type = TYPE_PATH;
        s.value = &val;
        s.name = "test_path";
        s.parser_data = &val2;

        const char* inputs[] = {
                "/bin/something",
                "something",
                "/path/path/path/",
                "/path/p argument",
                "p with multiple arguments",
                "~/p/p",
        };

        char *expanded_home = g_strconcat(user_get_home(), "/", "p/p", NULL);
        const char* results[] = {
                "/bin/something",
                "something",
                "/path/path/path/",
                "/path/p argument",
                "p with multiple arguments",
                expanded_home,
        };

        const char* results2[][5] = {
                {"/bin/something", NULL},
                {"something", NULL},
                {"/path/path/path/", NULL},
                {"/path/p", "argument", NULL},
                {"p", "with", "multiple", "arguments", NULL},
                {expanded_home},
        };

        ARRAY_SAME_LENGTH(inputs, results);
        ARRAY_SAME_LENGTH(inputs, results2);

        char buf[256];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i", i);
                ASSERTm(buf, set_from_string(&val, s, inputs[i]));
                sprintf(buf, "Failed in round %i. %s should be %s", i, val, results[i]);
                ASSERTm(buf, STR_EQ(val, results[i]));
                for (int j = 0; results2[i][j] != NULL; j++) {
                        ASSERT_STR_EQ(results2[i][j], val2[j]);
                }
        }

        g_free(val);
        g_free(expanded_home);
        g_strfreev(val2);
        PASS();
}

TEST test_string_to_sepcolor(void)
{
        struct separator_color_data val = {0};
        struct setting s;
        s.type = TYPE_CUSTOM;
        s.value = &val;
        s.name = "test_sepcolor";
        s.parser = string_parse_sepcolor;
        s.parser_data = sep_color_enum_data;

        const char* inputs[] = {
                "auto",
                "foreground",
                "frame",
                "#123456",
                "#ab123c",
                "#AB123C",
        };

        const struct separator_color_data results[] = {
                {SEP_AUTO, NULL},
                {SEP_FOREGROUND, NULL},
                {SEP_FRAME, NULL},
                {SEP_CUSTOM, "#123456"},
                {SEP_CUSTOM, "#ab123c"},
                {SEP_CUSTOM, "#AB123C"},
        };

        ARRAY_SAME_LENGTH(inputs, results);

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i. Expected %i, got %i", i, results[i].type, val.type);
                ASSERTm(buf, set_from_string(&val, s, inputs[i]));
                ASSERT_EQm(buf, results[i].type, val.type);
                sprintf(buf, "Failed in round %i. Expected %s, got %s", i, results[i].sep_color, val.sep_color);
                ASSERTm(buf, STR_EQ(results[i].sep_color, val.sep_color));
        }

        g_free(val.sep_color);
        PASS();
}

TEST test_string_to_sepcolor_invalid(void)
{
        struct separator_color_data val = {123, "test123"};
        struct setting s;
        s.type = TYPE_CUSTOM;
        s.value = &val;
        s.name = "test_sepcolor";
        s.parser = string_parse_sepcolor;
        s.parser_data = sep_color_enum_data;

        const char* inputs[] = {
                "",
                "f00reground",
                "fraame",
                "123456",
                "#ab",

                // TODO detect these mistakes as well
                /* "#12456", */
                /* "#gg123c", */
                /* "#AB123C123212", */
        };

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i.", i);
                ASSERT_FALSEm(buf, set_from_string(&val, s, inputs[i]));
        }

        ASSERT_EQm("Sep color shouldn't changed from invalid inputs", 123, (int) val.type);
        ASSERT_STR_EQm("Sep color shouldn't changed from invalid inputs", "test123", val.sep_color);
        PASS();
}

TEST test_string_to_length(void)
{
        struct length val = {0};
        struct setting s;
        s.type = TYPE_LENGTH;
        s.value = &val.min;
        s.name = "test_length";
        s.parser = NULL;
        s.parser_data = NULL;

        const char* inputs[] = {
                "123",
                "(123, 1234)",
                "( , )",
                "(234, )",
                "(, 123)",
        };

        const struct length results[] = {
                { 123, 123 },
                /* { 123, 123 }, */
                { 123, 1234 },
                /* { -123, 1234 }, */
                /* { -1234, 123 }, */
                /* { -1234, -123 }, */
                /* { -123, -1 }, */
                { 0, INT_MAX },
                { 234, INT_MAX },
                { 0, 123 },
        };

        ARRAY_SAME_LENGTH(inputs, results);

        char buf[500];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i.", i);
                ASSERTm(buf, set_from_string(&val, s, inputs[i]));
                sprintf(buf, "Failed in round %i. Expected min to be %i, got %i", i, results[i].min, val.min);
                ASSERT_EQm(buf, results[i].min, val.min);
                sprintf(buf, "Failed in round %i. Expected max to be %i, got %i", i, results[i].max, val.max);
                ASSERT_EQm(buf, results[i].max, val.max);
        }

        PASS();
}

TEST test_string_to_length_invalid(void)
{
        struct length val = {-123, -1234};
        struct setting s;
        s.type = TYPE_LENGTH;
        s.value = &val.min;
        s.name = "test_length";
        s.parser = NULL;
        s.parser_data = NULL;

        const char* inputs[] = {
                "",
                "f00reground",
                "fraame",
                "asb",
                "#ab",
                "(456",
                "456)",
                "(456, 567",
                "456, 567",
                "456, 567)",
                "-456",
                "-1",
                "(-123, 1234)", // Negative values
                "(123, -1234)",
                "(-123, -1234)",
                "(-123, )",
                "(123)",
                "(123, 122)", // invalid order
        };

        char buf[50];
        for (int i = 0; i < G_N_ELEMENTS(inputs); i++) {
                sprintf(buf, "Failed in round %i.", i);
                ASSERT_FALSEm(buf, set_from_string(&val, s, inputs[i]));
        }

        ASSERT_EQm("Length shouldn't change from invalid inputs", -123, val.min);
        ASSERT_EQm("Length shouldn't change from invalid inputs", -1234, val.max);
        PASS();
}

#define TEST_ENUM(t) { \
ASSERT_EQ(sizeof(t), sizeof(int)); \
}

// The settings code relies on the enums being the same size as an int. If
// they're bigger it's not a big problem, but if they're smaller, other parts
// of the memory may be overwritten.
TEST test_enum_size(void)
{
        TEST_ENUM(enum markup_mode);
        TEST_ENUM(enum alignment);
        TEST_ENUM(enum icon_position);
        TEST_ENUM(enum vertical_alignment);
        TEST_ENUM(enum follow_mode);
        TEST_ENUM(enum mouse_action );
        TEST_ENUM(enum zwlr_layer_shell_v1_layer);
        PASS();
}

SUITE(suite_option_parser)
{
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


        // These test try out the parsing of set_from_string for different
        // config types.
        // Non-stripped strings should not be passed to set_from_string. These
        // are normally stripped out by the ini parser.
        RUN_TEST(test_string_to_int);
        RUN_TEST(test_string_to_int_invalid);
        RUN_TEST(test_string_to_double);
        RUN_TEST(test_string_to_double_invalid);
        RUN_TEST(test_string_to_enum);
        RUN_TEST(test_string_to_enum_invalid);
        RUN_TEST(test_string_to_boolean);
        RUN_TEST(test_string_to_boolean_invalid);
        RUN_TEST(test_string_to_list);
        RUN_TEST(test_string_to_list_invalid);
        RUN_TEST(test_string_to_time);
        RUN_TEST(test_string_to_time_invalid);
        RUN_TEST(test_string_to_path);
        // Paths are now almost always considered valid
        RUN_TEST(test_string_to_sepcolor);
        RUN_TEST(test_string_to_sepcolor_invalid);
        RUN_TEST(test_enum_size);
        RUN_TEST(test_string_to_length);
        RUN_TEST(test_string_to_length_invalid);

        g_strfreev(argv);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

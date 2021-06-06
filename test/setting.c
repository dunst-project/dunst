#include "../src/settings.h"
#include "../src/option_parser.h"
#include "../src/settings_data.h"

#include "greatest.h"

extern const char *base;

// In this suite a few dunstrc's are tested to see if the settings code works
// This file is called setting.c, since the name settings.c caused issues.

char *config_path;

TEST test_dunstrc_markup(void) {
        config_path = g_strconcat(base, "/data/dunstrc.markup", NULL);
        load_settings(config_path);

        ASSERT_EQ(settings.markup, MARKUP_FULL);
        ASSERT_STR_EQ(settings.font, "Monospace 8");


        const char *e_format = "%s\\n%b"; // escape the \n since it would otherwise result in the newline character
        const struct rule * r = get_rule("global");
        const char *got_format = r->format;
        ASSERT_STR_EQ(e_format, got_format);
        ASSERT(settings.indicate_hidden);

        ASSERT_EQ(settings.geometry.w, 300);
        ASSERT_EQ(settings.geometry.h, 5);
        ASSERT_EQ(settings.geometry.x, -30);
        ASSERT_EQ(settings.geometry.y, 20);
        ASSERT(settings.geometry.negative_x);
        ASSERT_FALSE(settings.geometry.negative_y);

        g_free(config_path);
        PASS();
}

TEST test_dunstrc_nomarkup(void) {
        config_path = g_strconcat(base, "/data/dunstrc.nomarkup", NULL);
        load_settings(config_path);

        ASSERT_EQ(settings.markup, MARKUP_NO);
        ASSERT_STR_EQ(settings.font, "Monospace 8");


        const char *e_format = "%s\\n%b"; // escape the \n since it would otherwise result in the newline character
        const struct rule * r = get_rule("global");
        const char *got_format = r->format;
        ASSERT_STR_EQ(e_format, got_format);
        ASSERT(settings.indicate_hidden);

        ASSERT_EQ(settings.geometry.w, 290);
        ASSERT_EQ(settings.geometry.h, 10);
        ASSERT_EQ(settings.geometry.x, -30);
        ASSERT_EQ(settings.geometry.y, 20);
        ASSERT(settings.geometry.negative_x);
        ASSERT_FALSE(settings.geometry.negative_y);

        g_free(config_path);
        PASS();
}

// Check if the values at offset are the same using EQ_FUNC
#define CHECK_EQUAL_OFFSET(t, offset, EQ_FUNC) { \
        typeof(t) a = *(typeof(t)*) ((char*) &s_default + offset); \
        typeof(t) b = *(typeof(t)*) ((char*) &s_dunstrc + offset); \
        EQ_FUNC(message, a, b); \
} 

// Test if the defaults in code and in dunstrc match
TEST test_dunstrc_defaults(void) {
        struct settings s_default;
        struct settings s_dunstrc;

        config_path = g_strconcat(base, "/data/dunstrc.default", NULL);
        set_defaults();
        s_default = settings;

        load_settings(config_path);
        s_dunstrc = settings;

        ASSERT_EQ(s_default.corner_radius, s_dunstrc.corner_radius);
        char message[500];

        for (int i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                if (!allowed_settings[i].value) {
                        continue; // it's a rule, that's harder to test
                }
                size_t offset = (char*)allowed_settings[i].value - (char*)&settings;
                enum setting_type type = allowed_settings[i].type;
                snprintf(message, 500, "The default of setting %s does not match. Different defaults are set in code and dunstrc"
                                , allowed_settings[i].name);
                switch (type) {
                        case TYPE_CUSTOM:
                        case TYPE_TIME:
                        case TYPE_INT:;
                                      CHECK_EQUAL_OFFSET(int, offset, ASSERT_EQm);
                        case TYPE_BOOLEAN:;
                                      CHECK_EQUAL_OFFSET(bool, offset, ASSERT_EQm);
                        case TYPE_STRING: ;
                        case TYPE_PATH:
                        case TYPE_GEOMETRY:
                        case TYPE_LIST:
                                      continue; // TODO implement these checks as well
                        default:
                                      printf("Type unknown %s:%d", __FILE__, __LINE__);
                }
                /* printf("%zu\n", offset); */
        }

        g_free(config_path);
        PASS();
}

SUITE(suite_setting) {
        RUN_TEST(test_dunstrc_markup);
        RUN_TEST(test_dunstrc_nomarkup);
        RUN_TEST(test_dunstrc_defaults);
}

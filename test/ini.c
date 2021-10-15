#include "greatest.h"
#include "../src/ini.c"
#include <glib.h>

extern const char *base;

TEST test_next_section(void)
{
        char *config_path = g_strconcat(base, "/data/test-ini", NULL);
        FILE *config_file = fopen(config_path, "r");
        if (!config_file) {
                ASSERTm(false, "Test config file 'data/test-ini' couldn't be opened, failing.\n");
        }
        struct ini * ini = load_ini_file(config_file);
        ASSERT(ini);
        fclose(config_file);
        free(config_path);

        const char *section = NULL;
        ASSERT_STR_EQ("bool", (section = next_section(ini, section)));
        ASSERT_STR_EQ("string", (section = next_section(ini, section)));
        ASSERT_STR_EQ("list", (section = next_section(ini, section)));
        ASSERT_STR_EQ("path", (section = next_section(ini, section)));
        ASSERT_STR_EQ("int", (section = next_section(ini, section)));
        ASSERT_STR_EQ("double", (section = next_section(ini, section)));
        finish_ini(ini);
        free(ini);
        PASS();
}


SUITE(suite_ini)
{
        RUN_TEST(test_next_section);
}

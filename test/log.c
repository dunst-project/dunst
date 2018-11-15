#include "../src/log.c"
#include "greatest.h"

TEST test_log_level(GLogLevelFlags level, const char *shortstr, const char *longstr)
{
        ASSERT_STR_EQ(log_level_to_string(level), longstr);

        log_set_level_from_string(shortstr);

        if (level != G_LOG_LEVEL_ERROR)
                ASSERT_ENUM_EQ(level, log_level, log_level_to_string);

        log_set_level_from_string(longstr);

        if (level != G_LOG_LEVEL_ERROR)
                ASSERT_ENUM_EQ(level, log_level, log_level_to_string);

        PASS();
}

SUITE(suite_log)
{
        GLogLevelFlags oldlevel = log_level;

        RUN_TESTp(test_log_level, G_LOG_LEVEL_ERROR,    NULL,   "ERROR");
        RUN_TESTp(test_log_level, G_LOG_LEVEL_CRITICAL, "crit", "CRITICAL");
        RUN_TESTp(test_log_level, G_LOG_LEVEL_WARNING,  "warn", "WARNING");
        RUN_TESTp(test_log_level, G_LOG_LEVEL_MESSAGE,  "mesg", "MESSAGE");
        RUN_TESTp(test_log_level, G_LOG_LEVEL_INFO,     "info", "INFO");
        RUN_TESTp(test_log_level, G_LOG_LEVEL_DEBUG,    "deb",  "DEBUG");

        log_level = oldlevel;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

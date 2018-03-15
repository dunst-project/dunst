#include "greatest.h"

// This actually tests the buildsystem to make sure,
// the build system hands over a correct version number
// This is not testable via macros
TEST assert_version_number(void)
{
        ASSERTm("Version number is empty",
                0 != strcmp(VERSION, ""));

        ASSERTm("Version number is not seeded by git",
                NULL == strstr(VERSION, "non-git"));
        PASS();
}

SUITE(suite_misc)
{
        RUN_TEST(assert_version_number);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

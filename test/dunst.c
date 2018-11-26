#include "../src/dunst.c"
#include "greatest.h"

TEST test_dunst_status(void)
{
        status = (struct dunst_status) {false, false, false};

        dunst_status(S_FULLSCREEN, true);
        ASSERT(status.fullscreen);
        dunst_status(S_IDLE, true);
        ASSERT(status.idle);
        dunst_status(S_RUNNING, true);
        ASSERT(status.running);

        PASS();
}

SUITE(suite_dunst)
{
        RUN_TEST(test_dunst_status);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

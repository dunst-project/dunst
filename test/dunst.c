#define dbus_signal_status_changed(status) signal_sent_stub(status)
#include "../src/dunst.c"
#include "greatest.h"

static bool signal_sent = false;

void signal_sent_stub(struct dunst_status status)
{
        signal_sent = true;
        return;
}

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

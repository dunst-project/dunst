#include "../src/queues.c"
#include "greatest.h"
#include "queues.h"

TEST test_queue_init(void)
{
        queues_init();

        QUEUE_LEN_ALL(0, 0, 0);

        queues_teardown();
        PASS();
}

SUITE(suite_queues)
{
        RUN_TEST(test_queue_init);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

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

TEST test_queue_teardown(void)
{
        queues_init();
        QUEUE_LEN_ALL(0, 0, 0);

        struct notification *n = test_notification("n", -1);
        queues_notification_insert(n);

        queues_teardown();

        ASSERT(waiting == NULL);
        ASSERT(displayed == NULL);
        ASSERT(history == NULL);

        PASS();
}

SUITE(suite_queues)
{
        RUN_TEST(test_queue_init);
        RUN_TEST(test_queue_teardown);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

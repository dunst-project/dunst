#include "../src/queues.c"
#include "greatest.h"
#include "queues.h"

TEST test_queue_insert_id_valid_newid(void)
{
        struct notification *n;
        queues_init();

        n = test_notification("n", -1);
        n->id = 0;

        queues_notification_insert(n);

        QUEUE_LEN_ALL(1, 0, 0);
        ASSERT(n->id > 0);

        queues_teardown();
        PASS();
}

TEST test_queue_insert_id_invalid(void)
{
        struct notification *n;
        queues_init();

        n = test_notification("n", -1);
        n->id = 1000;

        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        ASSERTm("The given ID shouldn't be 0 anymore.",
                n->id > 0);
        ASSERT_EQm("This is a relict from times before stack_tag: "
                   "Even if next_notification_id is lower than the requested id, "
                   "it should use the requested id.",
                   1000, n->id);

        queues_teardown();
        PASS();
}

TEST test_queue_insert_id_replacement(void)
{
        struct notification *a, *b, *c;
        queues_init();

        a = test_notification("a", -1);
        notification_ref(a);

        queues_notification_insert(a);
        QUEUE_LEN_ALL(1, 0, 0);

        b = test_notification("b", -1);
        notification_ref(b);
        b->id = a->id;

        queues_notification_insert(b);
        QUEUE_LEN_ALL(1, 0, 0);

        ASSERT_EQ(a->id, b->id);
        NOT_LAST(a);

        queues_update(false);
        c = test_notification("c", -1);
        c->id = b->id;

        queues_notification_insert(c);

        QUEUE_LEN_ALL(0, 1, 0);
        ASSERT_EQ(b->id, c->id);
        NOT_LAST(b);

        queues_teardown();
        PASS();
}

TEST test_queue_notification_close(void)
{
        struct notification *n;

        // Test closing from waiting queue
        n = test_notification("n", -1);

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_notification_close(n, REASON_UNDEF);
        queues_update(NULL);
        QUEUE_LEN_ALL(0, 0, 1);
        queues_teardown();

        // Test closing from displayed queue
        n = test_notification("n", -1);

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(NULL);
        QUEUE_LEN_ALL(0, 1, 0);
        queues_notification_close(n, REASON_UNDEF);
        queues_update(NULL);
        QUEUE_LEN_ALL(0, 0, 1);
        queues_teardown();

        PASS();
}

TEST test_queue_notification_close_histignore(void)
{
        struct notification *n;

        // Test closing from waiting queue
        n = test_notification("n", -1);
        n->history_ignore = true;

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_notification_close(n, REASON_UNDEF);
        queues_update(NULL);
        QUEUE_LEN_ALL(0, 0, 0);
        queues_teardown();

        // Test closing from displayed queue
        n = test_notification("n", -1);
        n->history_ignore = true;

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(NULL);
        QUEUE_LEN_ALL(0, 1, 0);
        queues_notification_close(n, REASON_UNDEF);
        queues_update(NULL);
        QUEUE_LEN_ALL(0, 0, 0);
        queues_teardown();

        PASS();
}

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
        RUN_TEST(test_queue_insert_id_invalid);
        RUN_TEST(test_queue_insert_id_replacement);
        RUN_TEST(test_queue_insert_id_valid_newid);
        RUN_TEST(test_queue_notification_close);
        RUN_TEST(test_queue_notification_close_histignore);
        RUN_TEST(test_queue_teardown);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

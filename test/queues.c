#include "../src/queues.c"

#define GREATEST_FLOAT gint64
#define GREATEST_FLOAT_FMT "%ld"

#include "greatest.h"
#include "queues.h"

TEST test_queue_length(void)
{
        queues_init();

        struct notification *n;

        n = test_notification("n1", 0);
        queues_notification_insert(n);
        queues_notification_close(n, REASON_UNDEF);

        n = test_notification("n2", 0);
        queues_notification_insert(n);
        queues_update(STATUS_NORMAL);

        n = test_notification("n3", 0);
        queues_notification_insert(n);

        QUEUE_LEN_ALL(1,1,1);

        ASSERT_EQm("Queue waiting has to contain an element",
                   1, queues_length_waiting());
        ASSERT_EQm("Queue displayed has to contain an element",
                   1, queues_length_displayed());
        ASSERT_EQm("Queue history has to contain an element",
                   1, queues_length_history());

        queues_teardown();
        PASS();
}

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

        queues_update(STATUS_NORMAL);
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
        queues_update(STATUS_NORMAL);
        QUEUE_LEN_ALL(0, 0, 1);
        queues_teardown();

        // Test closing from displayed queue
        n = test_notification("n", -1);

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(STATUS_NORMAL);
        QUEUE_LEN_ALL(0, 1, 0);
        queues_notification_close(n, REASON_UNDEF);
        queues_update(STATUS_NORMAL);
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
        queues_update(STATUS_NORMAL);
        QUEUE_LEN_ALL(0, 0, 0);
        queues_teardown();

        // Test closing from displayed queue
        n = test_notification("n", -1);
        n->history_ignore = true;

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(STATUS_NORMAL);
        QUEUE_LEN_ALL(0, 1, 0);
        queues_notification_close(n, REASON_UNDEF);
        queues_update(STATUS_NORMAL);
        QUEUE_LEN_ALL(0, 0, 0);
        queues_teardown();

        PASS();
}

TEST test_queue_history_overfull(void)
{
        settings.history_length = 10;
        queues_init();

        struct notification *n;

        for (int i = 0; i < 10; i++) {
                char name[] = { 'n', '0'+i, '\0' }; // n<i>
                n = test_notification(name, -1);
                queues_notification_insert(n);
                queues_update(STATUS_NORMAL);
                queues_notification_close(n, REASON_UNDEF);
        }

        QUEUE_LEN_ALL(0, 0, 10);

        n = test_notification("n", -1);
        queues_notification_insert(n);
        queues_notification_close(n, REASON_UNDEF);

        QUEUE_CONTAINS(HIST, n);
        QUEUE_LEN_ALL(0, 0, 10);

        queues_teardown();
        PASS();
}

TEST test_queue_history_pushall(void)
{
        settings.history_length = 5;
        settings.indicate_hidden = false;
        settings.geometry.h = 0;

        queues_init();

        struct notification *n;

        for (int i = 0; i < 10; i++) {
                char name[] = { 'n', '0'+i, '\0' }; // n<i>
                n = test_notification(name, -1);
                queues_notification_insert(n);
        }
        queues_update(STATUS_NORMAL);

        for (int i = 0; i < 10; i++) {
                char name[] = { '2', 'n', '0'+i, '\0' }; // 2n<i>
                n = test_notification(name, -1);
                queues_notification_insert(n);
        }

        QUEUE_LEN_ALL(10, 10, 0);

        queues_history_push_all();

        QUEUE_CONTAINS(HIST, n);
        QUEUE_LEN_ALL(0, 0, 5);

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

TEST test_datachange_beginning_empty(void)
{
        queues_init();

        ASSERTm("There are no notifications at all, the timeout has to be less than 0.",
                queues_get_next_datachange(time_monotonic_now()) < 0);

        queues_teardown();
        PASS();
}

TEST test_datachange_endless(void)
{
        queues_init();

        settings.show_age_threshold = -1;

        struct notification *n = test_notification("n", 0);

        queues_notification_insert(n);
        queues_update(STATUS_NORMAL);

        ASSERTm("Age threshold is deactivated and the notification is infinite, there is no wakeup necessary.",
                queues_get_next_datachange(time_monotonic_now()) < 0);

        queues_teardown();
        PASS();
}

TEST test_datachange_endless_agethreshold(void)
{
        settings.show_age_threshold = S2US(5);

        queues_init();

        struct notification *n = test_notification("n", 0);

        queues_notification_insert(n);
        queues_update(STATUS_NORMAL);

        ASSERT_IN_RANGEm("Age threshold is activated and the next wakeup should be less than a second away",
                S2US(1)/2, queues_get_next_datachange(time_monotonic_now() + S2US(4)), S2US(1)/2);

        ASSERT_IN_RANGEm("Age threshold is activated and the next wakeup should be less than the age threshold",
                settings.show_age_threshold/2, queues_get_next_datachange(time_monotonic_now()), settings.show_age_threshold/2);

        settings.show_age_threshold = S2US(0);
        ASSERT_IN_RANGEm("Age threshold is activated and the next wakeup should be less than a second away",
                S2US(1)/2, queues_get_next_datachange(time_monotonic_now()), S2US(1)/2);

        queues_teardown();
        PASS();
}

TEST test_datachange_queues(void)
{
        queues_init();

        struct notification *n = test_notification("n", 10);

        queues_notification_insert(n);
        ASSERTm("The inserted notification is inside the waiting queue, so it should get ignored.",
               queues_get_next_datachange(time_monotonic_now()) < S2US(0));

        queues_update(STATUS_NORMAL);
        ASSERT_IN_RANGEm("The notification has to get closed in less than its timeout",
               S2US(10)/2, queues_get_next_datachange(time_monotonic_now()), S2US(10)/2);

        queues_notification_close(n, REASON_UNDEF);
        ASSERTm("The inserted notification is inside the history queue, so it should get ignored",
               queues_get_next_datachange(time_monotonic_now()) < S2US(0));

        queues_teardown();
        PASS();
}

TEST test_datachange_ttl(void)
{
        struct notification *n;
        queues_init();

        n = test_notification("n1", 15);

        queues_notification_insert(n);
        queues_update(STATUS_NORMAL);
        ASSERT_IN_RANGEm("The notification has to get closed in less than its timeout.",
               n->timeout/2, queues_get_next_datachange(time_monotonic_now()), n->timeout/2);

        n = test_notification("n2", 10);

        queues_notification_insert(n);
        queues_update(STATUS_NORMAL);
        ASSERT_IN_RANGEm("The timeout of the second notification has to get used as sleep time now.",
               n->timeout/2, queues_get_next_datachange(time_monotonic_now()), n->timeout/2);

        ASSERT_EQm("The notification already timed out. You have to answer with 0.",
               S2US(0), queues_get_next_datachange(time_monotonic_now() + S2US(10)));

        queues_teardown();
        PASS();
}


SUITE(suite_queues)
{
        RUN_TEST(test_datachange_beginning_empty);
        RUN_TEST(test_datachange_endless);
        RUN_TEST(test_datachange_endless_agethreshold);
        RUN_TEST(test_datachange_queues);
        RUN_TEST(test_datachange_ttl);
        RUN_TEST(test_queue_history_overfull);
        RUN_TEST(test_queue_history_pushall);
        RUN_TEST(test_queue_init);
        RUN_TEST(test_queue_insert_id_invalid);
        RUN_TEST(test_queue_insert_id_replacement);
        RUN_TEST(test_queue_insert_id_valid_newid);
        RUN_TEST(test_queue_length);
        RUN_TEST(test_queue_notification_close);
        RUN_TEST(test_queue_notification_close_histignore);
        RUN_TEST(test_queue_teardown);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

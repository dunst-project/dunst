#include "../src/queues.c"

#define GREATEST_FLOAT gint64
#define GREATEST_FLOAT_FMT "%ld"

#include "greatest.h"
#include "queues.h"
#include "helpers.h"

struct notification *queues_debug_find_notification_by_id(int id)
{
        assert(id > 0);

        GQueue *allqueues[] = { displayed, waiting, history };
        for (int i = 0; i < sizeof(allqueues)/sizeof(GQueue*); i++) {
                for (GList *iter = g_queue_peek_head_link(allqueues[i]); iter;
                     iter = iter->next) {
                        struct notification *cur = iter->data;
                        if (cur->id == id)
                                return cur;
                }
        }

        return NULL;
}

TEST test_queue_length(void)
{
        queues_init();

        struct notification *n;

        n = test_notification("n1", 0);
        queues_notification_insert(n);
        queues_notification_close(n, REASON_UNDEF);

        n = test_notification("n2", 0);
        queues_notification_insert(n);
        queues_update(STATUS_NORMAL, time_monotonic_now());

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

        queues_update(STATUS_NORMAL, time_monotonic_now());
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
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0, 0, 1);
        queues_teardown();

        // Test closing from displayed queue
        n = test_notification("n", -1);

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0, 1, 0);
        queues_notification_close(n, REASON_UNDEF);
        queues_update(STATUS_NORMAL, time_monotonic_now());
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
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0, 0, 0);
        queues_teardown();

        // Test closing from displayed queue
        n = test_notification("n", -1);
        n->history_ignore = true;

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0, 1, 0);
        queues_notification_close(n, REASON_UNDEF);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0, 0, 0);
        queues_teardown();

        PASS();
}

TEST test_queue_notification_skip_display(void)
{
        struct notification *n;

        // Test skipping display
        n = test_notification("n", -1);
        n->skip_display = true;

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0, 0, 1);
        queues_teardown();

        PASS();
}

TEST test_queue_notification_skip_display_redisplayed(void)
{
        struct notification *n;

        // Test skipping display
        n = test_notification("n", -1);
        n->skip_display = true;

        queues_init();
        queues_notification_insert(n);
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0, 0, 1);

        queues_history_pop();
        QUEUE_LEN_ALL(1, 0, 0);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_CONTAINSm("A skip display notification should stay in displayed "
                        "queue when it got pulled out of history queue",
                        DISP, n);

        queues_teardown();

        PASS();
}

TEST test_queue_notification_skip_display_redisplayed_by_random_id(void) {
        // breaks if the notification_buffer_size is above 5,
        // possible bug??????
        size_t notification_buffer_size = 5;

        struct notification *n[notification_buffer_size];
        bool is_n_popped[notification_buffer_size];

        // insert notifications
        char string[128];
        for(size_t i=0; i<notification_buffer_size; i++) {
                snprintf(string, 128, "n%08lu", i+1);

                n[i] = test_notification(string, -1);
                n[i]->skip_display = true;
                n[i]->id = i+1;

                // generate a noisy pop table
                is_n_popped[i] = rand() > RAND_MAX/4;
        }

        queues_init();
        for(size_t i=0; i<notification_buffer_size; i++) {
                queues_notification_insert(n[i]);
        }

        QUEUE_LEN_ALL(notification_buffer_size, 0, 0);

        // update notification event
        queues_update(STATUS_NORMAL, time_monotonic_now());

        QUEUE_LEN_ALL(0, 0, notification_buffer_size);

        // pop according to the pop table
        size_t popped_notification_count = 0;
        for(size_t i=0; i<notification_buffer_size; i++) {
                if(is_n_popped[i]) {
                        queues_history_pop_by_id(i+1);
                        popped_notification_count++;
                        queues_update(STATUS_NORMAL, time_monotonic_now());
                }
        }

        // test whether the notifications were unpopped properly
        QUEUE_LEN_ALL(0, popped_notification_count,
                        notification_buffer_size-popped_notification_count);

        queues_update(STATUS_NORMAL, time_monotonic_now());
        // check if any of the notifications got moved
        for(size_t i=0; i<notification_buffer_size; i++) {
                if(is_n_popped[i]) {
                        QUEUE_CONTAINSm("A skip display notification should stay in displayed "
                                        "queue when it got pulled out of history queue",
                                        DISP, n[i]);
                } else {
                        QUEUE_CONTAINSm("A skip display notification that didn't get popped"
                                        "out of history should've stayed there",
                                        HIST, n[i]);
                }
        }

        queues_teardown();

        PASS();
}

TEST test_queue_history_clear(void)
{
        struct notification *n;

        n = test_notification("n", -1);

        queues_init();
        queues_notification_insert(n);
        queues_history_push_all();

        QUEUE_LEN_ALL(0, 0, 1);
        queues_history_clear();
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
                queues_update(STATUS_NORMAL, time_monotonic_now());
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
        settings.notification_limit = 0;

        queues_init();

        struct notification *n;

        for (int i = 0; i < 10; i++) {
                char name[] = { 'n', '0'+i, '\0' }; // n<i>
                n = test_notification(name, -1);
                queues_notification_insert(n);
        }
        queues_update(STATUS_NORMAL, time_monotonic_now());

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

TEST test_queue_history_remove_by_id(void)
{
        settings.history_length = 5;
        settings.indicate_hidden = false;
        settings.notification_limit = 0;

        queues_init();

        struct notification *n;
        struct notification *n1 = NULL;


        for (int i = 0; i < 5; i++) {
                char name[] = { 'n', '0'+i, '\0' }; // n<i>
                n = test_notification(name, -1);
                queues_notification_insert(n);

                // Store notification at arbitrary position to remove
                if(i==1) {
                        n1 = n;
                }
        }
        queues_history_push_all();

        queues_history_remove_by_id(n->id);
        queues_history_remove_by_id(n1->id);

        QUEUE_LEN_ALL(0, 0, 3);   
        
        QUEUE_NOT_CONTAINS(HIST, n);
        QUEUE_NOT_CONTAINS(HIST, n1);

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
        queues_update(STATUS_NORMAL, time_monotonic_now());

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
        queues_update(STATUS_NORMAL, time_monotonic_now());

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

        queues_update(STATUS_NORMAL, time_monotonic_now());
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
        queues_update(STATUS_NORMAL, time_monotonic_now());
        ASSERT_IN_RANGEm("The notification has to get closed in less than its timeout.",
               n->timeout/2, queues_get_next_datachange(time_monotonic_now()), n->timeout/2);

        n = test_notification("n2", 10);

        queues_notification_insert(n);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        ASSERT_IN_RANGEm("The timeout of the second notification has to get used as sleep time now.",
               n->timeout/2, queues_get_next_datachange(time_monotonic_now()), n->timeout/2);

        ASSERT_EQm("The notification already timed out. You have to answer with 0.",
               S2US(0), queues_get_next_datachange(time_monotonic_now() + S2US(10)));

        queues_teardown();
        PASS();
}

TEST test_queue_stacking(void)
{
        settings.stack_duplicates = true;
        struct notification *n1, *n2, *n3;

        queues_init();

        n1 = test_notification("n1", -1);
        n2 = test_notification("n1", -1);
        n3 = test_notification("n1", -1);

        queues_notification_insert(n1);
        QUEUE_LEN_ALL(1, 0, 0);

        notification_ref(n1);
        queues_notification_insert(n2);
        NOT_LAST(n1);

        notification_ref(n2);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        queues_notification_insert(n3);
        QUEUE_LEN_ALL(0, 1, 0);
        NOT_LAST(n2);

        queues_teardown();
        PASS();
}

TEST test_queue_stacktag(void)
{
        const char *stacktag = "THIS IS A SUPER WIERD STACK TAG";
        struct notification *n1, *n2, *n3;

        queues_init();

        n1 = test_notification("n1", 1);
        n2 = test_notification("n1", 1);
        n3 = test_notification("n1", 1);
        n1->stack_tag = g_strdup(stacktag);
        n2->stack_tag = g_strdup(stacktag);
        n3->stack_tag = g_strdup(stacktag);

        queues_notification_insert(n1);
        QUEUE_LEN_ALL(1, 0, 0);

        notification_ref(n1);
        queues_notification_insert(n2);
        NOT_LAST(n1);

        notification_ref(n2);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        queues_notification_insert(n3);
        QUEUE_LEN_ALL(0, 1, 0);
        NOT_LAST(n2);

        queues_teardown();
        PASS();
}

TEST test_queue_different_stacktag(void)
{
        const char *stacktag = "asdf";
        const char *stacktag2 = "something else";
        struct notification *n1, *n2, *n3;
        settings.stack_duplicates = false;

        queues_init();

        n1 = test_notification("n1", 1);
        n2 = test_notification("n1", 1);
        n3 = test_notification("n1", 1);
        n1->stack_tag = g_strdup(stacktag);
        n2->stack_tag = g_strdup(stacktag);
        n3->stack_tag = g_strdup(stacktag2);

        queues_notification_insert(n1);
        QUEUE_LEN_ALL(1, 0, 0);

        notification_ref(n1);
        queues_notification_insert(n2);
        NOT_LAST(n1);

        queues_notification_insert(n3);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        printf("queue %i\n",g_queue_get_length(QUEUE(HIST)));
        QUEUE_LEN_ALL(0, 2, 0);

        queues_teardown();
        PASS();
}

TEST test_queue_stacktag_different_appid(void)
{
        const char *stacktag = "THIS IS A SUPER WIERD STACK TAG";
        struct notification *n1, *n2, *n3;

        queues_init();

        n1 = test_notification("n1", 1);
        n2 = test_notification("n2", 1);
        n3 = test_notification("n2", 1);
        n1->stack_tag = g_strdup(stacktag);
        n2->stack_tag = g_strdup(stacktag);
        n3->stack_tag = g_strdup(stacktag);

        queues_notification_insert(n1);
        QUEUE_LEN_ALL(1, 0, 0);

        queues_notification_insert(n2);

        notification_ref(n2);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        queues_notification_insert(n3);
        QUEUE_LEN_ALL(0, 2, 0);
        NOT_LAST(n2);

        queues_teardown();
        PASS();
}

TEST test_queue_timeout(void)
{
        settings.notification_limit = 5;
        struct notification *n1, *n2, *n3;

        queues_init();

        n1 = test_notification("n1", 0);
        n2 = test_notification("n2", 10);
        n3 = test_notification("n3", 10);
        n3->transient = true;

        queues_notification_insert(n1);
        queues_notification_insert(n2);
        queues_notification_insert(n3);

        queues_update(STATUS_NORMAL, time_monotonic_now());

        // hacky way to shift time
        n1->start -= S2US(11);
        n2->start -= S2US(11);
        n3->start -= S2US(11);
        queues_update(STATUS_IDLE, time_monotonic_now());

        QUEUE_LEN_ALL(0,2,1);
        QUEUE_CONTAINS(HIST, n3);

        // hacky way to shift time
        n1->start -= S2US(11);
        n2->start -= S2US(11);
        queues_update(STATUS_NORMAL, time_monotonic_now());

        QUEUE_LEN_ALL(0,1,2);
        QUEUE_CONTAINS(DISP, n1);
        QUEUE_CONTAINS(HIST, n2);
        QUEUE_CONTAINS(HIST, n3);

        queues_teardown();
        PASS();
}

TEST test_queues_update_fullscreen(void)
{
        settings.notification_limit = 5;
        struct notification *n_show, *n_dela, *n_push;

        queues_init();

        n_show = test_notification("show", 10);
        n_dela = test_notification("dela", 10);
        n_push = test_notification("push", 10);
        n_show->fullscreen = FS_SHOW;
        n_dela->fullscreen = FS_DELAY;
        n_push->fullscreen = FS_PUSHBACK;

        queues_notification_insert(n_show);
        queues_notification_insert(n_dela);
        queues_notification_insert(n_push);

        queues_update(STATUS_FS, time_monotonic_now());
        QUEUE_CONTAINS(DISP, n_show);
        QUEUE_CONTAINS(WAIT, n_dela);
        QUEUE_CONTAINS(WAIT, n_push);

        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_CONTAINS(DISP, n_show);
        QUEUE_CONTAINS(DISP, n_dela);
        QUEUE_CONTAINS(DISP, n_push);

        queues_update(STATUS_FS, time_monotonic_now());
        QUEUE_CONTAINS(DISP, n_show);
        QUEUE_CONTAINS(DISP, n_dela);
        QUEUE_CONTAINS(WAIT, n_push);

        queues_teardown();
        PASS();
}

TEST test_queues_update_paused(void)
{
        settings.notification_limit = 5;
        struct notification *n1, *n2, *n3;
        queues_init();

        n1 = test_notification("n1", 0);
        n2 = test_notification("n2", 0);
        n3 = test_notification("n3", 0);

        queues_notification_insert(n1);
        queues_notification_insert(n2);
        queues_notification_insert(n3);

        QUEUE_LEN_ALL(3,0,0);

        queues_update(STATUS_PAUSE, time_monotonic_now());
        QUEUE_LEN_ALL(3,0,0);

        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0,3,0);

        queues_update(STATUS_PAUSE, time_monotonic_now());
        QUEUE_LEN_ALL(3,0,0);

        queues_teardown();
        PASS();
}

TEST test_queues_update_seeping(void)
{
        settings.notification_limit = 5;
        settings.sort = true;
        settings.indicate_hidden = false;
        struct notification *nl1, *nl2, *nl3, *nl4, *nl5;
        struct notification *nc1, *nc2, *nc3, *nc4, *nc5;
        queues_init();

        nl1 = test_notification("nl1", 0);
        nl2 = test_notification("nl2", 0);
        nl3 = test_notification("nl3", 0);
        nl4 = test_notification("nl4", 0);
        nl5 = test_notification("nl5", 0);

        nc1 = test_notification("nc1", 0);
        nc2 = test_notification("nc2", 0);
        nc3 = test_notification("nc3", 0);
        nc4 = test_notification("nc4", 0);
        nc5 = test_notification("nc5", 0);
        nc1->urgency = URG_CRIT;
        nc2->urgency = URG_CRIT;
        nc3->urgency = URG_CRIT;
        nc4->urgency = URG_CRIT;
        nc5->urgency = URG_CRIT;

        queues_notification_insert(nl1);
        queues_notification_insert(nl2);
        queues_notification_insert(nl3);
        queues_notification_insert(nl4);
        queues_notification_insert(nl5);

        QUEUE_LEN_ALL(5,0,0);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0,5,0);

        queues_notification_insert(nc1);
        queues_notification_insert(nc2);
        queues_notification_insert(nc3);
        queues_notification_insert(nc4);
        queues_notification_insert(nc5);

        QUEUE_LEN_ALL(5,5,0);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(5,5,0);

        QUEUE_CONTAINS(DISP, nc1);
        QUEUE_CONTAINS(DISP, nc2);
        QUEUE_CONTAINS(DISP, nc3);
        QUEUE_CONTAINS(DISP, nc4);
        QUEUE_CONTAINS(DISP, nc5);

        QUEUE_CONTAINS(WAIT, nl1);
        QUEUE_CONTAINS(WAIT, nl2);
        QUEUE_CONTAINS(WAIT, nl3);
        QUEUE_CONTAINS(WAIT, nl4);
        QUEUE_CONTAINS(WAIT, nl5);

        queues_teardown();
        PASS();
}

TEST test_queues_update_xmore(void)
{
        settings.indicate_hidden = true;
        settings.notification_limit = 4;
        struct notification *n1, *n2, *n3, *n4, *n5;
        queues_init();

        n1 = test_notification("n1", 0);
        n2 = test_notification("n2", 0);
        n3 = test_notification("n3", 0);
        n4 = test_notification("n4", 0);
        n5 = test_notification("n5", 0);

        queues_notification_insert(n1);
        queues_notification_insert(n2);
        queues_notification_insert(n3);

        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0,3,0);

        queues_notification_insert(n4);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(0,4,0);

        queues_notification_insert(n5);
        queues_update(STATUS_NORMAL, time_monotonic_now());
        QUEUE_LEN_ALL(2,3,0);

        queues_teardown();
        PASS();
}

TEST test_queues_update_seep_showlowurg(void)
{
        // Test 3 notifications during fullscreen and only the one
        // with the lowest priority is eligible to get shown
        settings.notification_limit = 4;
        struct notification *n1, *n2, *n3;
        queues_init();

        n1 = test_notification("n1", 0);
        n2 = test_notification("n2", 0);
        n3 = test_notification("n3", 0);

        n1->fullscreen = FS_DELAY;
        n2->fullscreen = FS_DELAY;
        n3->fullscreen = FS_SHOW;

        n3->urgency = URG_LOW;

        queues_notification_insert(n1);
        queues_notification_insert(n2);
        queues_update(STATUS_FS, time_monotonic_now());
        QUEUE_LEN_ALL(2,0,0);

        queues_notification_insert(n3);

        queues_update(STATUS_FS, time_monotonic_now());

        QUEUE_LEN_ALL(2,1,0);
        QUEUE_CONTAINS(WAIT, n1);
        QUEUE_CONTAINS(WAIT, n2);
        QUEUE_CONTAINS(DISP, n3);

        queues_teardown();
        PASS();
}

TEST test_queues_timeout_before_paused(void)
{
        struct notification *n;
        queues_init();

        n = test_notification("n", 10);

        queues_notification_insert(n);
        queues_update(STATUS_NORMAL, time_monotonic_now());

        n->start -= S2US(11);
        queues_update(STATUS_PAUSE, time_monotonic_now());

        QUEUE_LEN_ALL(0,0,1);

        queues_teardown();
        PASS();
}

TEST test_queue_find_by_id(void)
{
        struct notification *n;
        int id;
        queues_init();

        n = test_notification("n", 0);
        queues_notification_insert(n);
        n = test_notification("n1", 0);
        queues_notification_insert(n);
        id = n->id;
        n = test_notification("n2", 0);
        queues_notification_insert(n);

        n = queues_get_by_id(id);

        ASSERT(n->id == id);
        ASSERT(!strncmp(n->summary, "n1", 2));

        queues_teardown();
        PASS();
}

TEST test_queue_get_history(void)
{
        struct notification *n;
        queues_init();

        n = test_notification("n", -1);
        n->skip_display = true;
        queues_notification_insert(n);
        n = test_notification("n1", -1);
        n->skip_display = true;
        queues_notification_insert(n);
        n = test_notification("n3", -1);
        n->skip_display = true;
        queues_notification_insert(n);

        queues_update(STATUS_NORMAL, time_monotonic_now());

        QUEUE_LEN_ALL(0, 0, 3);

        GList *h = queues_get_history();
        ASSERT(g_list_length(h) == 3);

        queues_teardown();
        PASS();
}


void print_queues() {
        printf("\nQueues:\n");
        for (GList *iter = g_queue_peek_head_link(QUEUE_WAIT); iter;
                        iter = iter->next) {
                struct notification *notif = iter->data;
                printf("waiting %s\n", notif->summary);
        }
}

// Test if notifications are correctly sorted, even if dunst is paused in
// between. See #838 for the issue.
TEST test_queue_no_sort_and_pause(void)
{
        // Setting sort to false, this means that notifications will only be
        // sorted based on time
        settings.sort = false;
        settings.notification_limit = 0;
        struct notification *n;
        queues_init();

        n = test_notification("n0", 0);
        queues_notification_insert(n);
        queues_update(STATUS_NORMAL, time_monotonic_now());

        n = test_notification("n1", 0);
        queues_notification_insert(n);
        queues_update(STATUS_NORMAL, time_monotonic_now());

        n = test_notification("n2", 0);
        queues_notification_insert(n);
        queues_update(STATUS_PAUSE, time_monotonic_now());

        n = test_notification("n3", 0);
        queues_notification_insert(n);
        queues_update(STATUS_PAUSE, time_monotonic_now());
        /* queues_update(STATUS_NORMAL, time_monotonic_now()); */

        n = test_notification("n4", 0);
        queues_notification_insert(n);
        queues_update(STATUS_NORMAL, time_monotonic_now());

        QUEUE_LEN_ALL(0, 5, 0);

        const char* order[] = {
                "n0",
                "n1",
                "n2",
                "n3",
                "n4",
        };

        for (int i = 0; i < g_queue_get_length(QUEUE_DISP); i++) {
                struct notification *notif = g_queue_peek_nth(QUEUE_DISP, i);
                ASSERTm("Notifications are not in the right order",
                                STR_EQ(notif->summary, order[i]));
        }

        queues_teardown();
        PASS();
}

SUITE(suite_queues)
{
        bool store = settings.stack_duplicates;
        settings.stack_duplicates = false;

        RUN_TEST(test_datachange_beginning_empty);
        RUN_TEST(test_datachange_endless);
        RUN_TEST(test_datachange_endless_agethreshold);
        RUN_TEST(test_datachange_queues);
        RUN_TEST(test_datachange_ttl);
        RUN_TEST(test_queue_history_clear);
        RUN_TEST(test_queue_history_overfull);
        RUN_TEST(test_queue_history_pushall);
        RUN_TEST(test_queue_history_remove_by_id);
        RUN_TEST(test_queue_init);
        RUN_TEST(test_queue_insert_id_invalid);
        RUN_TEST(test_queue_insert_id_replacement);
        RUN_TEST(test_queue_insert_id_valid_newid);
        RUN_TEST(test_queue_length);
        RUN_TEST(test_queue_notification_close);
        RUN_TEST(test_queue_notification_close_histignore);
        RUN_TEST(test_queue_notification_skip_display);
        RUN_TEST(test_queue_notification_skip_display_redisplayed);
        RUN_TEST(test_queue_notification_skip_display_redisplayed_by_random_id);
        RUN_TEST(test_queue_stacking);
        RUN_TEST(test_queue_stacktag);
        RUN_TEST(test_queue_different_stacktag);
        RUN_TEST(test_queue_stacktag_different_appid);
        RUN_TEST(test_queue_teardown);
        RUN_TEST(test_queue_timeout);
        RUN_TEST(test_queues_update_fullscreen);
        RUN_TEST(test_queues_update_paused);
        RUN_TEST(test_queues_update_seep_showlowurg);
        RUN_TEST(test_queues_update_seeping);
        RUN_TEST(test_queues_update_xmore);
        RUN_TEST(test_queues_timeout_before_paused);
        RUN_TEST(test_queue_find_by_id);
        RUN_TEST(test_queue_no_sort_and_pause);
        RUN_TEST(test_queue_get_history);

        settings.stack_duplicates = store;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

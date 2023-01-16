#include "../src/input.c"
#include "queues.h"
#include "greatest.h"
#include "helpers.h"
#include "../src/utils.h"

TEST test_get_notification_clickable_height_first(void)
{
        bool orginal_gap_size = settings.gap_size;
        settings.gap_size = 0;

        struct notification *n = test_notification("test", 10);
        n->displayed_height = 12;

        int expected_size = n->displayed_height + settings.frame_width;
        expected_size += (settings.separator_height / 2.0);
        int result = get_notification_clickable_height(n, true, false);

        ASSERT(result == expected_size);

        settings.gap_size = orginal_gap_size;
        notification_unref(n);
        PASS();
}

TEST test_get_notification_clickable_height_middle(void)
{
        bool orginal_gap_size = settings.gap_size;
        settings.gap_size = 0;

        struct notification *n = test_notification("test", 10);
        n->displayed_height = 12;

        int expected_size = n->displayed_height + settings.separator_height;
        int result = get_notification_clickable_height(n, false, false);

        ASSERT(result == expected_size);

        settings.gap_size = orginal_gap_size;
        notification_unref(n);
        PASS();
}

TEST test_get_notification_clickable_height_last(void)
{
        bool orginal_gap_size = settings.gap_size;
        settings.gap_size = 0;

        struct notification *n = test_notification("test", 10);
        n->displayed_height = 12;

        int expected_size = n->displayed_height + settings.frame_width;
        expected_size += (settings.separator_height / 2.0);
        int result = get_notification_clickable_height(n, false, true);

        ASSERT(result == expected_size);

        settings.gap_size = orginal_gap_size;
        notification_unref(n);
        PASS();
}

TEST test_get_notification_clickable_height_gaps(void)
{
        bool orginal_gap_size = settings.gap_size;
        settings.gap_size = 7;

        struct notification *n = test_notification("test", 10);
        n->displayed_height = 12;

        int expected_size = n->displayed_height + (settings.frame_width * 2);

        int result_first = get_notification_clickable_height(n, true, false);
        ASSERT(result_first == expected_size);

        int result_middle = get_notification_clickable_height(n, false, false);
        ASSERT(result_middle == expected_size);

        int result_last = get_notification_clickable_height(n, false, true);
        ASSERT(result_last == expected_size);

        settings.gap_size = orginal_gap_size;
        notification_unref(n);
        PASS();
}

TEST test_notification_at(void)
{
        int total_notifications = 3;
        GSList *notifications = get_dummy_notifications(total_notifications);

        queues_init();

        int display_height = 12;
        struct notification *n;
        for (GSList *iter = notifications; iter; iter = iter->next) {
                n = iter->data;
                n->displayed_height = display_height;
                queues_notification_insert(n);
        }

        queues_update(STATUS_NORMAL, time_monotonic_now());

        struct notification *top_notification = g_slist_nth_data(notifications, 0);
        int top_notification_height = get_notification_clickable_height(top_notification, true, false);

        struct notification *middle_notification = g_slist_nth_data(notifications, 1);
        int middle_notification_height = get_notification_clickable_height(middle_notification, false, false);

        struct notification *bottom_notification = g_slist_nth_data(notifications, 2);
        int bottom_notification_height = get_notification_clickable_height(bottom_notification, false, true);

        struct notification *result;

        int top_y_coord;
        top_y_coord = 0;
        result = get_notification_at(top_y_coord);
        ASSERT(result != NULL);
        ASSERT(result == top_notification);

        top_y_coord = top_notification_height - 1;
        result = get_notification_at(top_y_coord);
        ASSERT(result != NULL);
        ASSERT(result == top_notification);

        int middle_y_coord;
        middle_y_coord = top_notification_height;
        result = get_notification_at(middle_y_coord);
        ASSERT(result != NULL);
        ASSERT(result == middle_notification);

        middle_y_coord = top_notification_height;
        result = get_notification_at(middle_y_coord);
        ASSERT(result != NULL);
        ASSERT(result == middle_notification);

        int bottom_y_coord;
        bottom_y_coord = top_notification_height + middle_notification_height;
        result = get_notification_at(bottom_y_coord);
        ASSERT(result != NULL);
        ASSERT(result == bottom_notification);

        bottom_y_coord = top_notification_height + middle_notification_height + bottom_notification_height - 1;
        result = get_notification_at(bottom_y_coord);
        ASSERT(result != NULL);
        ASSERT(result == bottom_notification);

        queues_teardown();
        g_slist_free(notifications);
        PASS();
}

SUITE(suite_input)
{
        SHUFFLE_TESTS(time(NULL), {
                        RUN_TEST(test_get_notification_clickable_height_first);
                        RUN_TEST(test_get_notification_clickable_height_middle);
                        RUN_TEST(test_get_notification_clickable_height_last);
                        RUN_TEST(test_get_notification_clickable_height_gaps);
                        RUN_TEST(test_notification_at);
        });
}

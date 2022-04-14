#include "../src/draw.c"
#include "greatest.h"
#include "helpers.h"
#include <cairo.h>

cairo_t *c;

double get_dummy_scale() { return 1; }

const struct screen_info* noop_screen(void) {
        static struct screen_info i;
        return &i;
}

const struct output dummy_output = {
        x_setup,
        x_free,

        x_win_create,
        x_win_destroy,

        x_win_show,
        x_win_hide,

        x_display_surface,
        x_win_get_context,

        noop_screen,

        x_is_idle,
        have_fullscreen_window,

        get_dummy_scale,
};

GSList *get_dummy_layouts(GSList *notifications)
{
        GSList *layouts = NULL;

        for (GSList *iter = notifications; iter; iter = iter->next) {
                struct colored_layout *cl = layout_from_notification(c, iter->data);
                layouts = g_slist_append(layouts, cl);

        }
        return layouts;
}

int get_small_max_height()
{
        // to keep test calculations simpler, set max height small to
        // only test cases where height is not dynamically determined
        // by notification content
        // future tests targeting dynamic sizing logic could be added
        // to address this limitation
        int small_max_height = 10;
        return small_max_height;
}

int get_expected_dimension_height(int layout_count)
{
        // assumes settings.height == notification height, see get_small_max_height
        int separator_height = (layout_count - 1) * settings.separator_height;
        int total_gap_size = (layout_count - 1) * settings.gap_size;
        int height = settings.height * layout_count;
        int frame_width_total_height;
        int expected_height;
        if(settings.gap_size) {
                frame_width_total_height = layout_count * (2 * settings.frame_width);
                expected_height = height + frame_width_total_height + total_gap_size;
        } else {
                frame_width_total_height = 2 * settings.frame_width;
                expected_height = separator_height + height + frame_width_total_height;
        }
        return expected_height;
}

int get_expected_dimension_y_offset(int layout_count)
{
        // assumes settings.height == notification height, see get_small_max_height
        int expected_y = layout_count * settings.height;
        if(settings.gap_size) {
                expected_y += (layout_count * (2 * settings.frame_width));
                expected_y += (layout_count * settings.gap_size);
        } else {
                expected_y += (2 * settings.frame_width);
                expected_y += (layout_count * settings.separator_height);
        }
        return expected_y;
}

TEST test_layout_from_notification(void)
{
        struct notification *n = test_notification_with_icon("test", 10);
        n->icon_position = ICON_LEFT;
        ASSERT(n->icon);
        n->text_to_render = g_strdup("");
        struct colored_layout *cl = layout_from_notification(c, n);
        ASSERT(cl->icon);
        free_colored_layout(cl);
        notification_unref(n);
        PASS();
}

TEST test_layout_from_notification_icon_off(void)
{
        struct notification *n = test_notification_with_icon("test", 10);
        n->icon_position = ICON_OFF;
        ASSERT(n->icon);
        n->text_to_render = g_strdup("");
        struct colored_layout *cl = layout_from_notification(c, n);
        ASSERT_FALSE(cl->icon);
        free_colored_layout(cl);
        notification_unref(n);
        PASS();
}

TEST test_layout_from_notification_no_icon(void)
{
        struct notification *n = test_notification("test", 10);
        n->icon_position = ICON_LEFT;
        ASSERT_FALSE(n->icon);
        n->text_to_render = g_strdup("");
        struct colored_layout *cl = layout_from_notification(c, n);
        ASSERT_FALSE(cl->icon);

        free_colored_layout(cl);
        notification_unref(n);
        PASS();
}

TEST test_calculate_dimensions_height_no_gaps(void)
{
        int original_height = settings.height;
        bool orginal_gap_size = settings.gap_size;
        settings.height = get_small_max_height();
        settings.gap_size = 10;

        int layout_count;
        GSList *notifications;
        GSList *layouts;
        struct dimensions dim;
        int expected_height;

        layout_count = 1;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 2;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 3;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        settings.gap_size = orginal_gap_size;
        settings.height = original_height;

        PASS();
}

TEST test_calculate_dimensions_height_gaps(void)
{
        int original_height = settings.height;
        bool orginal_gap_size = settings.gap_size;
        settings.height = get_small_max_height();
        settings.gap_size = 10;

        int layout_count;
        GSList *notifications;
        GSList *layouts;
        struct dimensions dim;
        int expected_height;

        layout_count = 1;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 2;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 3;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count);
        ASSERT(dim.h == expected_height);

        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);
        settings.gap_size = orginal_gap_size;
        settings.height = original_height;

        PASS();
}

TEST test_layout_render_no_gaps(void)
{
        int original_height = settings.height;
        bool orginal_gap_size = settings.gap_size;
        settings.height = get_small_max_height();
        settings.gap_size = 0;

        int layout_count;
        GSList *notifications;
        GSList *layouts;
        struct dimensions dim;
        cairo_surface_t *image_surface;
        int expected_y;

        layout_count = 3;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        image_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);

        bool first = true;
        for (GSList *iter = layouts; iter; iter = iter->next) {
                struct colored_layout *cl_this = iter->data;
                struct colored_layout *cl_next = iter->next ? iter->next->data : NULL;

                dim = layout_render(image_surface, cl_this, cl_next, dim, first, !cl_next);

                first = false;
        }

        expected_y = get_expected_dimension_y_offset(layout_count);
        ASSERT(dim.y == expected_y);

        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);
        cairo_surface_destroy(image_surface);
        settings.gap_size = orginal_gap_size;
        settings.height = original_height;

        PASS();
}

TEST test_layout_render_gaps(void)
{
        int original_height = settings.height;
        bool orginal_gap_size = settings.gap_size;
        settings.height = get_small_max_height();
        settings.gap_size = 10;

        int layout_count;
        GSList *notifications;
        GSList *layouts;
        struct dimensions dim;
        cairo_surface_t *image_surface;
        int expected_y;

        layout_count = 3;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        image_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);

        for (GSList *iter = layouts; iter; iter = iter->next) {
                struct colored_layout *cl_this = iter->data;
                struct colored_layout *cl_next = iter->next ? iter->next->data : NULL;

                dim = layout_render(image_surface, cl_this, cl_next, dim, true, true);
        }

        expected_y = get_expected_dimension_y_offset(layout_count);
        ASSERT(dim.y == expected_y);

        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);
        cairo_surface_destroy(image_surface);
        settings.gap_size = orginal_gap_size;
        settings.height = original_height;

        PASS();
}

SUITE(suite_draw)
{
        output = &dummy_output;
        cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        c = cairo_create(s);

        SHUFFLE_TESTS(time(NULL), {
                        RUN_TEST(test_layout_from_notification);
                        RUN_TEST(test_layout_from_notification_icon_off);
                        RUN_TEST(test_layout_from_notification_no_icon);
                        RUN_TEST(test_calculate_dimensions_height_no_gaps);
                        RUN_TEST(test_calculate_dimensions_height_gaps);
                        RUN_TEST(test_layout_render_no_gaps);
                        RUN_TEST(test_layout_render_gaps);
        });
}

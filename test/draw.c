#include "../src/draw.c"
#include "greatest.h"
#include "helpers.h"
#include <cairo.h>

#if WAYLAND_ONLY
#include "../src/wayland/wl.h"
#else
#include "../src/x11/x.h"
#endif

cairo_t *c;

double get_dummy_scale(void) { return 1; }

const struct screen_info* noop_screen(void) {
        static struct screen_info i = {0};
        return &i;
}

const struct output dummy_output = {
#if !X11_SUPPORT
        wl_init,
        wl_deinit,

        wl_win_create,
        wl_win_destroy,

        wl_win_show,
        wl_win_hide,

        wl_display_surface,
        wl_win_get_context,
        wl_win_get_surface,

        noop_screen,

        wl_is_idle,
        wl_have_fullscreen_window,
#else
        x_setup,
        x_free,

        x_win_create,
        x_win_destroy,

        x_win_show,
        x_win_hide,

        x_display_surface,
        x_win_get_context,
        x_win_get_surface,

        noop_screen,

        x_is_idle,
        have_fullscreen_window,
#endif

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

struct length get_small_max_height(void)
{
        // to keep test calculations simpler, set max height small to
        // only test cases where height is not dynamically determined
        // by notification content
        // future tests targeting dynamic sizing logic could be added
        // to address this limitation
		struct length height = { 0, 10 };
		return height;
}

int get_expected_dimension_height(int layout_count, int height)
{
        // assumes height == notification height, see get_small_max_height
        int separator_height = (layout_count - 1) * settings.separator_height;
        int total_gap_size = (layout_count - 1) * settings.gap_size;
        height *= layout_count;
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
        int expected_y = layout_count * settings.height.max;
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
        struct length original_height = settings.height;
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
        expected_height = get_expected_dimension_height(layout_count, settings.height.max);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 2;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count, settings.height.max);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 3;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count, settings.height.max);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        settings.gap_size = orginal_gap_size;
        settings.height = original_height;

        PASS();
}

TEST test_calculate_dimensions_height_gaps(void)
{
        struct length original_height = settings.height;
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
        expected_height = get_expected_dimension_height(layout_count, settings.height.max);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 2;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count, settings.height.max);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 3;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count, settings.height.max);
        ASSERT(dim.h == expected_height);

        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);
        settings.gap_size = orginal_gap_size;
        settings.height = original_height;

        PASS();
}

TEST test_layout_render_no_gaps(void)
{
        struct length original_height = settings.height;
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

        enum corner_pos corners = (settings.corners & C_TOP) | _C_FIRST;
        for (GSList *iter = layouts; iter; iter = iter->next) {

                struct colored_layout *cl_this = iter->data;
                struct colored_layout *cl_next = iter->next ? iter->next->data : NULL;

                if (settings.gap_size)
                        corners = settings.corners;
                else if (!cl_next)
                        corners |= (settings.corners & C_BOT) | _C_LAST;

                dim = layout_render(image_surface, cl_this, cl_next, dim, corners);
                corners &= ~(C_TOP | _C_FIRST);
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

TEST test_calculate_dimensions_height_min(void)
{
        struct length original_height = settings.height;
        bool orginal_gap_size = settings.gap_size;
		// NOTE: Should be big enough to fit the notification nicely
        settings.height.min = 100;
        settings.height.max = 200;
        settings.gap_size = 0;

        int layout_count;
        GSList *notifications;
        GSList *layouts;
        struct dimensions dim;
        int expected_height;

        layout_count = 1;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count, settings.height.min);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 2;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count, settings.height.min);
        ASSERT(dim.h == expected_height);
        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);

        layout_count = 3;
        notifications = get_dummy_notifications(layout_count);
        layouts = get_dummy_layouts(notifications);
        dim = calculate_dimensions(layouts);
        expected_height = get_expected_dimension_height(layout_count, settings.height.min);
        ASSERT(dim.h == expected_height);

        g_slist_free_full(layouts, free_colored_layout);
        g_slist_free_full(notifications, free_dummy_notification);
        settings.gap_size = orginal_gap_size;
        settings.height = original_height;

        PASS();
}

TEST test_layout_render_gaps(void)
{
        struct length original_height = settings.height;
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

        enum corner_pos corners = (settings.corners & C_TOP) | _C_FIRST;
        for (GSList *iter = layouts; iter; iter = iter->next) {

                struct colored_layout *cl_this = iter->data;
                struct colored_layout *cl_next = iter->next ? iter->next->data : NULL;

                if (settings.gap_size)
                        corners = settings.corners;
                else if (!cl_next)
                        corners |= (settings.corners & C_BOT) | _C_LAST;

                dim = layout_render(image_surface, cl_this, cl_next, dim, corners);
                corners &= ~(C_TOP | _C_FIRST);
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

        // XXX: This variable should not be accessed like this
        extern PangoContext *pango_ctx;
        pango_ctx = pango_cairo_create_context(c);

        SHUFFLE_TESTS(time(NULL), {
                        RUN_TEST(test_layout_from_notification);
                        RUN_TEST(test_layout_from_notification_icon_off);
                        RUN_TEST(test_layout_from_notification_no_icon);
                        RUN_TEST(test_calculate_dimensions_height_no_gaps);
                        RUN_TEST(test_calculate_dimensions_height_gaps);
                        RUN_TEST(test_calculate_dimensions_height_min);
                        RUN_TEST(test_layout_render_no_gaps);
                        RUN_TEST(test_layout_render_gaps);
        });

        g_object_unref(pango_ctx);
        cairo_destroy(c);
        cairo_surface_destroy(s);
}

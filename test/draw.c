#include "../src/draw.c"
#include "greatest.h"
#include "helpers.h"
#include <cairo.h>
#include <glib.h>

cairo_t *c;

double dummy_get_scale() { return 1; }

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

        dummy_get_scale,
};

GSList *get_dummy_layouts(int count) {
        GSList *layouts = NULL;

        for(int i=0;i<count;i++) {
                struct notification *n = test_notification("test", 10);
                n->icon_position = ICON_LEFT;
                n->text_to_render = g_strdup("dummy layout");
                struct colored_layout *cl = layout_from_notification(c, n);
                layouts = g_slist_append(layouts, cl);
        }

        return layouts;
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
        // to keep test calculations simpler, set max height small to
        // only test cases where height is not dynamically determined
        // by notification content
        // future tests targeting dynamic sizing logic could be added
        // to address this limitation
        settings.height = 10;
        settings.gaps = 0;
        int layout_count;
        GSList *layouts;
        struct dimensions dim;
        int separator_height;
        int height;
        int frame_width;
        int expected_height;

        layout_count = 1;
        layouts = get_dummy_layouts(layout_count);
        dim = calculate_dimensions(layouts);
        separator_height = (layout_count - 1) * settings.separator_height;
        height = settings.height * layout_count;
        frame_width = 2 * settings.frame_width;
        expected_height = separator_height + height + frame_width;
        ASSERT(dim.h == expected_height);

        layout_count = 2;
        layouts = get_dummy_layouts(layout_count);
        dim = calculate_dimensions(layouts);
        separator_height = (layout_count - 1) * settings.separator_height;
        height = settings.height * layout_count;
        frame_width = 2 * settings.frame_width;
        expected_height = separator_height + height + frame_width;
        ASSERT(dim.h == expected_height);

        layout_count = 3;
        layouts = get_dummy_layouts(layout_count);
        dim = calculate_dimensions(layouts);
        separator_height = (layout_count - 1) * settings.separator_height;
        height = settings.height * layout_count;
        frame_width = 2 * settings.frame_width;
        expected_height = separator_height + height + frame_width;
        ASSERT(dim.h == expected_height);

        PASS();
}

TEST test_calculate_dimensions_height_gaps(void)
{
        // to keep test calculations simpler, set max height small to
        // only test cases where height is not dynamically determined
        // by notification content
        // future tests targeting dynamic sizing logic could be added
        // to address this limitation
        settings.height = 10;
        settings.gaps = 1;
        settings.gap_size = 27;
        int layout_count;
        GSList *layouts;
        struct dimensions dim;
        int height;
        int frame_width;
        int expected_height;
        int gap_size;
        int separator_height = 0;

        layout_count = 1;
        layouts = get_dummy_layouts(layout_count);
        dim = calculate_dimensions(layouts);
        height = settings.height * layout_count;
        frame_width = layout_count * (2 * settings.frame_width);
        gap_size = (layout_count - 1) * settings.gap_size;
        expected_height = height + frame_width + gap_size + separator_height;
        ASSERT(dim.h == expected_height);

        layout_count = 2;
        layouts = get_dummy_layouts(layout_count);
        dim = calculate_dimensions(layouts);
        height = settings.height * layout_count;
        frame_width = layout_count * (2 * settings.frame_width);
        gap_size = (layout_count - 1) * settings.gap_size;
        expected_height = height + frame_width + gap_size + separator_height;
        ASSERT(dim.h == expected_height);

        layout_count = 3;
        layouts = get_dummy_layouts(layout_count);
        dim = calculate_dimensions(layouts);
        height = settings.height * layout_count;
        frame_width = layout_count * (2 * settings.frame_width);
        gap_size = (layout_count - 1) * settings.gap_size;
        expected_height = height + frame_width + gap_size + separator_height;
        ASSERT(dim.h == expected_height);

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
        });
}

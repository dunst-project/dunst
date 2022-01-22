#include "../src/draw.c"
#include "greatest.h"
#include "helpers.h"
#include <cairo.h>

cairo_t *c;

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

        x_get_scale,
};

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

SUITE(suite_draw)
{
        output = &dummy_output;
        cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        c = cairo_create(s);

        SHUFFLE_TESTS(time(NULL), {
                        RUN_TEST(test_layout_from_notification);
                        RUN_TEST(test_layout_from_notification_icon_off);
                        RUN_TEST(test_layout_from_notification_no_icon);
        });
}

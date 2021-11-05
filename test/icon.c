#include "../src/icon.c"
#include "greatest.h"

#define DATAPREFIX "/data"
#define ICONPATH "/data/icons/theme"

/* As there are no hints to test if the loaded GdkPixbuf is
 * read from a PNG or an SVG file, the sample icons in the
 * test structure have different sizes
 */
#define IS_ICON_PNG(pb)  4 == gdk_pixbuf_get_width(pb)
#define IS_ICON_SVG(pb) 16 == gdk_pixbuf_get_width(pb)

extern const char *base;

int scale = 1;
int size = 16;

TEST test_get_path_from_icon_null(void)
{
        char *result = get_path_from_icon_name(NULL, 16);
        ASSERT_EQ(result, NULL);
        PASS();
}

TEST test_get_path_from_icon_name_full(void)
{
        const char *iconpath = ICONPATH;

        gchar *path = g_build_filename(base, iconpath, "valid", "icon1.svg", NULL);

        char *result = get_path_from_icon_name(path, size);
        ASSERT(result);
        ASSERT_STR_EQ(result, path);

        g_free(path);
        g_free(result);
        PASS();
}

TEST test_icon_size_clamp_too_small(void)
{
        int w = 12, h = 24;
        bool resized = icon_size_clamp(&w, &h);
        ASSERT(resized);
        ASSERT_EQ(w, 16);
        ASSERT_EQ(h, 32);

        PASS();
}

TEST test_icon_size_clamp_not_necessary(void)
{
        int w = 20, h = 30;
        bool resized = icon_size_clamp(&w, &h);
        ASSERT(!resized);
        ASSERT_EQ(w, 20);
        ASSERT_EQ(h, 30);

        PASS();
}

TEST test_icon_size_clamp_too_big(void)
{
        int w = 75, h = 150;
        bool resized = icon_size_clamp(&w, &h);
        ASSERT(resized);
        ASSERT_EQ(w, 50);
        ASSERT_EQ(h, 100);

        PASS();
}

TEST test_icon_size_clamp_too_small_then_too_big(void)
{
        int w = 8, h = 80;
        bool resized = icon_size_clamp(&w, &h);
        ASSERT(resized);
        ASSERT_EQ(w, 10);
        ASSERT_EQ(h, 100);

        PASS();
}

SUITE(suite_icon)
{
        // set only valid icons in the path
        char *icon_path = g_build_filename(base, DATAPREFIX, NULL);
        setenv("XDG_DATA_HOME", icon_path, 1);
        printf("Icon path: %s\n", icon_path);
        RUN_TEST(test_get_path_from_icon_null);
        RUN_TEST(test_get_path_from_icon_name_full);
        RUN_TEST(test_icon_size_clamp_not_necessary);

        settings.min_icon_size = 16;
        settings.max_icon_size = 100;

        RUN_TEST(test_icon_size_clamp_too_small);
        RUN_TEST(test_icon_size_clamp_not_necessary);
        RUN_TEST(test_icon_size_clamp_too_big);
        RUN_TEST(test_icon_size_clamp_too_small_then_too_big);

        settings.min_icon_size = 16;
        settings.max_icon_size = 0;

        RUN_TEST(test_icon_size_clamp_too_small);
        RUN_TEST(test_icon_size_clamp_not_necessary);

        settings.min_icon_size = 0;
        settings.max_icon_size = 100;

        RUN_TEST(test_icon_size_clamp_not_necessary);
        RUN_TEST(test_icon_size_clamp_too_big);

        settings.min_icon_size = 0;
        settings.max_icon_size = 0;
        g_clear_pointer(&icon_path, g_free);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

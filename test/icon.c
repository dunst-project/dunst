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

        gchar *path = g_build_filename(base, iconpath, "16x16", "actions", "edit.png", NULL);

        char *result = get_path_from_icon_name(path, size);
        ASSERT(result);
        ASSERT_STR_EQ(result, path);

        g_free(path);
        g_free(result);
        PASS();
}

TEST test_icon_size_clamp_too_small(int min_icon_size, int max_icon_size)
{
        int w = 12, h = 24;
        bool resized = icon_size_clamp(&w, &h, min_icon_size, max_icon_size);
        ASSERT(resized);
        ASSERT_EQ(w, 16);
        ASSERT_EQ(h, 32);

        PASS();
}

TEST test_icon_size_clamp_not_necessary(int min_icon_size, int max_icon_size)
{
        int w = 20, h = 30;
        bool resized = icon_size_clamp(&w, &h, min_icon_size, max_icon_size);
        ASSERT(!resized);
        ASSERT_EQ(w, 20);
        ASSERT_EQ(h, 30);

        PASS();
}

TEST test_icon_size_clamp_too_big(int min_icon_size, int max_icon_size)
{
        int w = 75, h = 150;
        bool resized = icon_size_clamp(&w, &h, min_icon_size, max_icon_size);
        ASSERT(resized);
        ASSERT_EQ(w, 50);
        ASSERT_EQ(h, 100);

        PASS();
}

TEST test_icon_size_clamp_too_small_then_too_big(int min_icon_size, int max_icon_size)
{
        int w = 8, h = 80;
        bool resized = icon_size_clamp(&w, &h, min_icon_size, max_icon_size);
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
        RUN_TESTp(test_icon_size_clamp_not_necessary, 0, 100);

        RUN_TESTp(test_icon_size_clamp_too_small, 16, 100);
        RUN_TESTp(test_icon_size_clamp_not_necessary, 16, 100);
        RUN_TESTp(test_icon_size_clamp_too_big, 16, 100);
        RUN_TESTp(test_icon_size_clamp_too_small_then_too_big, 16, 100);

        RUN_TESTp(test_icon_size_clamp_too_small, 16, 0);
        RUN_TESTp(test_icon_size_clamp_not_necessary, 16, 0);

        RUN_TESTp(test_icon_size_clamp_not_necessary, 0, 100);
        RUN_TESTp(test_icon_size_clamp_too_big, 0, 100);

        g_clear_pointer(&icon_path, g_free);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

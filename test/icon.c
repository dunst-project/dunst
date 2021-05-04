#include "../src/icon.c"
#include "greatest.h"

#define ICONPREFIX "/data/icons/theme"

/* As there are no hints to test if the loaded GdkPixbuf is
 * read from a PNG or an SVG file, the sample icons in the
 * test structure have different sizes
 */
#define IS_ICON_PNG(pb)  4 == gdk_pixbuf_get_width(pb)
#define IS_ICON_SVG(pb) 16 == gdk_pixbuf_get_width(pb)

extern const char *base;

int scale = 1;

TEST test_get_path_from_icon_null(void){
    char *result = get_path_from_icon_name(NULL);
    ASSERT_EQ(result, NULL);
    PASS();
}

TEST test_get_path_from_icon_name(void)
{
        const char *iconpath = ICONPREFIX;
        settings.max_icon_size = 128;

        const char* icon_name = "edit";
        const char* expected_suffix = ".png";
        char* full_name = g_strconcat(icon_name, expected_suffix, NULL);

        gchar *path = g_build_filename(base, iconpath, "32x32",
                        "actions", full_name, NULL);
        char *result = get_path_from_icon_name(icon_name);

        ASSERT(result);
        ASSERT_EQ(strcmp(result, path), 0);

        g_free(full_name);
        g_free(path);
        g_free(result);
        PASS();
}

TEST test_get_path_from_icon_name_full(void)
{
        const char *iconpath = ICONPREFIX;

        gchar *path = g_build_filename(base, iconpath, "valid", "icon1.svg", NULL);

        char *result = get_path_from_icon_name(path);
        ASSERT(result);
        ASSERT_EQ(strcmp(result, path), 0);

        g_free(path);
        g_free(result);
        PASS();
}

TEST test_get_pixbuf_from_file_tilde(void)
{
        const char *home = g_get_home_dir();
        const char *iconpath = ICONPREFIX;

        if (0 != strncmp(home, base, strlen(home))) {
                SKIPm("Current directory is not a subdirectory from user's home."
                      " Cannot test iconpath tilde expansion.\n");
        }

        gchar *path = g_build_filename(base, iconpath, "16x16", "actions", "edit.png", NULL);
        path = string_replace_at(path, 0, strlen(home), "~");

        GdkPixbuf *pixbuf = get_pixbuf_from_file(path, scale);
        g_clear_pointer(&path, g_free);

        ASSERT(pixbuf);
        g_clear_pointer(&pixbuf, g_object_unref);
        PASS();
}

TEST test_get_pixbuf_from_file_absolute(void)
{
        const char *iconpath = ICONPREFIX;

        gchar *path = g_build_filename(base, iconpath, "16x16", "actions", "edit.png", NULL);

        GdkPixbuf *pixbuf = get_pixbuf_from_file(path, scale);
        g_clear_pointer(&path, g_free);

        ASSERT(pixbuf);
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

TEST test_get_pixbuf_from_icon_invalid(void)
{
        GdkPixbuf *pixbuf = get_pixbuf_from_icon("invalid", scale);
        ASSERT(pixbuf == NULL);
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

TEST test_get_pixbuf_from_icon(void)
{
        GdkPixbuf *pixbuf = get_pixbuf_from_icon("edit", scale);
        ASSERT(pixbuf);
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

TEST test_get_pixbuf_from_icon_filename(void)
{
        char *icon = g_strconcat(base, "/data/icons/valid.png", NULL);
        GdkPixbuf *pixbuf = get_pixbuf_from_icon(icon, scale);
        ASSERT(pixbuf);
        ASSERTm("PNG pixbuf isn't loaded", IS_ICON_PNG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        g_free(icon);
        PASS();
}

TEST test_get_pixbuf_from_icon_fileuri(void)
{
        char *icon = g_strconcat("file://", base, "/data/icons/valid.svg", NULL);
        GdkPixbuf *pixbuf = get_pixbuf_from_icon(icon, scale);
        ASSERT(pixbuf);
        ASSERTm("SVG pixbuf isn't loaded", IS_ICON_SVG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        g_free(icon);
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

TEST test_get_pixbuf_from_icon_both_is_scaled(void)
{
        GdkPixbuf *pixbuf = get_pixbuf_from_icon("preferences", scale);
        ASSERT(pixbuf);
        ASSERT_EQ(gdk_pixbuf_get_width(pixbuf), 16);
        ASSERT_EQ(gdk_pixbuf_get_height(pixbuf), 16);
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

SUITE(suite_icon)
{
        // set only valid icons in the path
        settings.icon_path = g_build_filename(base, ICONPREFIX, NULL);
        RUN_TEST(test_get_path_from_icon_name);
        RUN_TEST(test_get_path_from_icon_null);
        RUN_TEST(test_get_path_from_icon_name_full);
        RUN_TEST(test_get_pixbuf_from_file_tilde);
        RUN_TEST(test_get_pixbuf_from_file_absolute);
        RUN_TEST(test_get_pixbuf_from_icon_invalid);
        RUN_TEST(test_get_pixbuf_from_icon);
        RUN_TEST(test_get_pixbuf_from_icon_filename);
        RUN_TEST(test_get_pixbuf_from_icon_fileuri);
        RUN_TEST(test_icon_size_clamp_not_necessary);

        settings.min_icon_size = 16;
        settings.max_icon_size = 100;

        RUN_TEST(test_get_pixbuf_from_icon_both_is_scaled);
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

        g_clear_pointer(&settings.icon_path, g_free);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

#include "../src/icon.c"
#include "greatest.h"

#define ICONPREFIX "/data/icons/path"

/* As there are no hints to test if the loaded GdkPixbuf is
 * read from a PNG or an SVG file, the sample icons in the
 * test structure have different sizes
 */
#define IS_ICON_PNG(pb)  4 == gdk_pixbuf_get_width(pb)
#define IS_ICON_SVG(pb) 16 == gdk_pixbuf_get_width(pb)

extern const char *base;

TEST test_get_pixbuf_from_file_tilde(void)
{
        const char *home = g_get_home_dir();
        const char *iconpath = ICONPREFIX;

        if (0 != strncmp(home, base, strlen(home))) {
                SKIPm("Current directory is not a subdirectory from user's home."
                      " Cannot test iconpath tilde expansion.\n");
        }

        gchar *path = g_build_filename(base, iconpath, "valid", "icon1.svg", NULL);
        path = string_replace_at(path, 0, strlen(home), "~");

        GdkPixbuf *pixbuf = get_pixbuf_from_file(path);
        g_clear_pointer(&path, g_free);

        ASSERT(pixbuf);
        ASSERTm("The wrong pixbuf is loaded in the icon file.", IS_ICON_SVG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);
        PASS();
}

TEST test_get_pixbuf_from_file_absolute(void)
{
        const char *iconpath = ICONPREFIX;

        gchar *path = g_build_filename(base, iconpath, "valid", "icon1.svg", NULL);

        GdkPixbuf *pixbuf = get_pixbuf_from_file(path);
        g_clear_pointer(&path, g_free);

        ASSERT(pixbuf);
        ASSERTm("The wrong pixbuf is loaded in the icon file.", IS_ICON_SVG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

TEST test_get_pixbuf_from_icon_invalid(void)
{
        GdkPixbuf *pixbuf = get_pixbuf_from_icon("invalid");
        ASSERT(pixbuf == NULL);
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

TEST test_get_pixbuf_from_icon_both(void)
{
        GdkPixbuf *pixbuf = get_pixbuf_from_icon("icon1");
        ASSERT(pixbuf);
        ASSERTm("SVG pixbuf hasn't precedence", IS_ICON_SVG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

TEST test_get_pixbuf_from_icon_onlysvg(void)
{
        GdkPixbuf *pixbuf = get_pixbuf_from_icon("onlysvg");
        ASSERT(pixbuf);
        ASSERTm("SVG pixbuf isn't loaded", IS_ICON_SVG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

TEST test_get_pixbuf_from_icon_onlypng(void)
{
        GdkPixbuf *pixbuf = get_pixbuf_from_icon("onlypng");
        ASSERT(pixbuf);
        ASSERTm("PNG pixbuf isn't loaded", IS_ICON_PNG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        PASS();
}

TEST test_get_pixbuf_from_icon_filename(void)
{
        char *icon = g_strconcat(base, "/data/icons/valid.png", NULL);
        GdkPixbuf *pixbuf = get_pixbuf_from_icon(icon);
        ASSERT(pixbuf);
        ASSERTm("PNG pixbuf isn't loaded", IS_ICON_PNG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        g_free(icon);
        PASS();
}

TEST test_get_pixbuf_from_icon_fileuri(void)
{
        char *icon = g_strconcat("file://", base, "/data/icons/valid.svg", NULL);
        GdkPixbuf *pixbuf = get_pixbuf_from_icon(icon);
        ASSERT(pixbuf);
        ASSERTm("SVG pixbuf isn't loaded", IS_ICON_SVG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        g_free(icon);
        PASS();
}

SUITE(suite_icon)
{
        settings.icon_path = g_strconcat(
                     base, ICONPREFIX "/invalid"
                ":", base, ICONPREFIX "/valid"
                ":", base, ICONPREFIX "/both",
                NULL);

        RUN_TEST(test_get_pixbuf_from_file_tilde);
        RUN_TEST(test_get_pixbuf_from_file_absolute);
        RUN_TEST(test_get_pixbuf_from_icon_invalid);
        RUN_TEST(test_get_pixbuf_from_icon_both);
        RUN_TEST(test_get_pixbuf_from_icon_onlysvg);
        RUN_TEST(test_get_pixbuf_from_icon_onlypng);
        RUN_TEST(test_get_pixbuf_from_icon_filename);
        RUN_TEST(test_get_pixbuf_from_icon_fileuri);

        g_clear_pointer(&settings.icon_path, g_free);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

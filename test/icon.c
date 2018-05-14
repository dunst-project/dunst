#include "greatest.h"
#include "../src/icon.h"
#include "../src/utils.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>

#define ICONPREFIX "./data/icons/path"

/* As there are no hints to test if the loaded GdkPixbuf is
 * read from a PNG or an SVG file, the sample icons in the
 * test structure have different sizes
 */
#define IS_ICON_PNG(pb)  4 == gdk_pixbuf_get_width(pb)
#define IS_ICON_SVG(pb) 16 == gdk_pixbuf_get_width(pb)

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
        char *icon = string_append(g_get_current_dir(), "/data/icons/valid.png", NULL);
        GdkPixbuf *pixbuf = get_pixbuf_from_icon(icon);
        ASSERT(pixbuf);
        ASSERTm("PNG pixbuf isn't loaded", IS_ICON_PNG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        g_free(icon);
        PASS();
}

TEST test_get_pixbuf_from_icon_fileuri(void)
{
        char *curdir = g_get_current_dir();
        char *icon = g_strconcat("file://", curdir,"/data/icons/valid.svg", NULL);
        GdkPixbuf *pixbuf = get_pixbuf_from_icon(icon);
        ASSERT(pixbuf);
        ASSERTm("SVG pixbuf isn't loaded", IS_ICON_SVG(pixbuf));
        g_clear_pointer(&pixbuf, g_object_unref);

        g_free(icon);
        g_free(curdir);
        PASS();
}

SUITE(suite_icon)
{
        settings.icon_path =
                    ICONPREFIX "/invalid"
                ":" ICONPREFIX "/valid"
                ":" ICONPREFIX "/both";

        RUN_TEST(test_get_pixbuf_from_icon_invalid);
        RUN_TEST(test_get_pixbuf_from_icon_both);
        RUN_TEST(test_get_pixbuf_from_icon_onlysvg);
        RUN_TEST(test_get_pixbuf_from_icon_onlypng);
        RUN_TEST(test_get_pixbuf_from_icon_filename);
        RUN_TEST(test_get_pixbuf_from_icon_fileuri);

        settings.icon_path = NULL;
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

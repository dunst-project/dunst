#include "icon.h"

#include <assert.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdbool.h>
#include <string.h>

#include "log.h"
#include "notification.h"
#include "settings.h"
#include "utils.h"

static bool is_readable_file(const char *filename)
{
        return (access(filename, R_OK) != -1);
}

/**
 * Reassemble the data parts of a GdkPixbuf into a cairo_surface_t's data field.
 *
 * Requires to call on the surface flush before and mark_dirty after the execution.
 */
static void pixbuf_data_to_cairo_data(
                const unsigned char *pixels_p,
                      unsigned char *pixels_c,
                size_t rowstride_p,
                size_t rowstride_c,
                int width,
                int height,
                int n_channels)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
        static const size_t CAIRO_B = 0;
        static const size_t CAIRO_G = 1;
        static const size_t CAIRO_R = 2;
        static const size_t CAIRO_A = 3;
#elif G_BYTE_ORDER == G_BIG_ENDIAN
        static const size_t CAIRO_A = 0;
        static const size_t CAIRO_R = 1;
        static const size_t CAIRO_G = 2;
        static const size_t CAIRO_B = 3;
#elif G_BYTE_ORDER == G_PDP_ENDIAN
        static const size_t CAIRO_R = 0;
        static const size_t CAIRO_A = 1;
        static const size_t CAIRO_B = 2;
        static const size_t CAIRO_G = 3;
#else
// GLib doesn't support any other endiannesses
#error Unsupported Endianness
#endif

        assert(pixels_p);
        assert(pixels_c);
        assert(width > 0);
        assert(height > 0);

        if (n_channels == 3) {
                for (int h = 0; h < height; h++) {
                              unsigned char *iter_c = pixels_c + h * rowstride_c;
                        const unsigned char *iter_p = pixels_p + h * rowstride_p;
                        for (int w = 0; w < width; w++) {
                                iter_c[CAIRO_R] = iter_p[0];
                                iter_c[CAIRO_G] = iter_p[1];
                                iter_c[CAIRO_B] = iter_p[2];
                                iter_c[CAIRO_A] = 0xff;
                                iter_c += 4;
                                iter_p += n_channels;
                        }
                }
        } else {
                for (int h = 0; h < height; h++) {
                              unsigned char *iter_c = pixels_c + h * rowstride_c;
                        const unsigned char *iter_p = pixels_p + h * rowstride_p;
                        for (int w = 0; w < width; w++) {
                                double alpha_factor = iter_p[3] / (double)0xff;
                                iter_c[CAIRO_R] = (unsigned char)(iter_p[0] * alpha_factor + .5);
                                iter_c[CAIRO_G] = (unsigned char)(iter_p[1] * alpha_factor + .5);
                                iter_c[CAIRO_B] = (unsigned char)(iter_p[2] * alpha_factor + .5);
                                iter_c[CAIRO_A] =                 iter_p[3];
                                iter_c += 4;
                                iter_p += n_channels;
                        }
                }

        }
}

cairo_surface_t *gdk_pixbuf_to_cairo_surface(GdkPixbuf *pixbuf)
{
        assert(pixbuf);

        int width  = gdk_pixbuf_get_width(pixbuf);
        int height = gdk_pixbuf_get_height(pixbuf);

        cairo_format_t fmt = gdk_pixbuf_get_has_alpha(pixbuf) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
        cairo_surface_t *icon_surface = cairo_image_surface_create(fmt, width, height);

        /* Copy pixel data from pixbuf to surface */
        cairo_surface_flush(icon_surface);
        pixbuf_data_to_cairo_data(gdk_pixbuf_read_pixels(pixbuf),
                                  cairo_image_surface_get_data(icon_surface),
                                  gdk_pixbuf_get_rowstride(pixbuf),
                                  cairo_format_stride_for_width(fmt, width),
                                  gdk_pixbuf_get_width(pixbuf),
                                  gdk_pixbuf_get_height(pixbuf),
                                  gdk_pixbuf_get_n_channels(pixbuf));
        cairo_surface_mark_dirty(icon_surface);

        return icon_surface;
}

GdkPixbuf *icon_pixbuf_scale(GdkPixbuf *pixbuf)
{
        ASSERT_OR_RET(pixbuf, NULL);

        int w = gdk_pixbuf_get_width(pixbuf);
        int h = gdk_pixbuf_get_height(pixbuf);
        int larger = w > h ? w : h;
        if (settings.max_icon_size && larger > settings.max_icon_size) {
                int scaled_w = settings.max_icon_size;
                int scaled_h = settings.max_icon_size;
                if (w >= h)
                        scaled_h = (settings.max_icon_size * h) / w;
                else
                        scaled_w = (settings.max_icon_size * w) / h;

                GdkPixbuf *scaled = gdk_pixbuf_scale_simple(
                                pixbuf,
                                scaled_w,
                                scaled_h,
                                GDK_INTERP_BILINEAR);
                g_object_unref(pixbuf);
                pixbuf = scaled;
        }

        return pixbuf;
}

GdkPixbuf *get_pixbuf_from_file(const char *filename)
{
        char *path = string_to_path(g_strdup(filename));
        GError *error = NULL;

        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &error);

        if (error) {
                LOG_W("%s", error->message);
                g_error_free(error);
        }

        g_free(path);
        return pixbuf;
}

GdkPixbuf *get_pixbuf_from_icon(const char *iconname)
{
        if (STR_EMPTY(iconname))
                return NULL;

        const char *suffixes[] = { ".svg", ".png", ".xpm", NULL };
        GdkPixbuf *pixbuf = NULL;
        gchar *uri_path = NULL;

        if (g_str_has_prefix(iconname, "file://")) {
                uri_path = g_filename_from_uri(iconname, NULL, NULL);
                if (uri_path)
                        iconname = uri_path;
        }

        /* absolute path? */
        if (iconname[0] == '/' || iconname[0] == '~') {
                pixbuf = get_pixbuf_from_file(iconname);
        } else {
        /* search in icon_path */
                char *start = settings.icon_path,
                     *end, *current_folder, *maybe_icon_path;
                do {
                        end = strchr(start, ':');
                        if (!end) end = strchr(settings.icon_path, '\0'); /* end = end of string */

                        current_folder = g_strndup(start, end - start);

                        for (const char **suf = suffixes; *suf; suf++) {
                                maybe_icon_path = g_strconcat(current_folder, "/", iconname, *suf, NULL);
                                if (is_readable_file(maybe_icon_path))
                                        pixbuf = get_pixbuf_from_file(maybe_icon_path);
                                g_free(maybe_icon_path);

                                if (pixbuf)
                                        break;
                        }

                        g_free(current_folder);
                        if (pixbuf)
                                break;

                        start = end + 1;
                } while (STR_FULL(end));
                if (!pixbuf)
                        LOG_W("No icon found in path: '%s'", iconname);
        }

        g_free(uri_path);
        return pixbuf;
}

GdkPixbuf *get_pixbuf_from_raw_image(const struct raw_image *raw_image)
{
        GdkPixbuf *pixbuf = NULL;

        pixbuf = gdk_pixbuf_new_from_data(raw_image->data,
                                          GDK_COLORSPACE_RGB,
                                          raw_image->has_alpha,
                                          raw_image->bits_per_sample,
                                          raw_image->width,
                                          raw_image->height,
                                          raw_image->rowstride,
                                          NULL,
                                          NULL);

        return pixbuf;
}

cairo_surface_t *icon_get_for_notification(const struct notification *n)
{
        return gdk_pixbuf_to_cairo_surface(n->icon);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

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

/**
 * Scales the given image dimensions if necessary according to the settings.
 *
 * @param w a pointer to the image width, to be modified in-place
 * @param h a pointer to the image height, to be modified in-place
 * @return TRUE if the dimensions were updated, FALSE if they were left unchanged
 */
static bool icon_size_clamp(int *w, int *h) {
        int _w = *w, _h = *h;
        int landscape = _w > _h;
        int orig_larger = landscape ? _w : _h;
        double larger = orig_larger;
        double smaller = landscape ? _h : _w;
        if (settings.min_icon_size && smaller < settings.min_icon_size) {
                larger = larger / smaller * settings.min_icon_size;
                smaller = settings.min_icon_size;
        }
        if (settings.max_icon_size && larger > settings.max_icon_size) {
                smaller = smaller / larger * settings.max_icon_size;
                larger = settings.max_icon_size;
        }
        if ((int) larger != orig_larger) {
                *w = (int) (landscape ? larger : smaller);
                *h = (int) (landscape ? smaller : larger);
                return TRUE;
        }
        return FALSE;
}

/**
 * Scales the given GdkPixbuf if necessary according to the settings.
 *
 * @param pixbuf (nullable) The pixbuf, which may be too big.
 *                          Takes ownership of the reference.
 * @return the scaled version of the pixbuf. If scaling wasn't
 *         necessary, it returns the same pixbuf. Transfers full
 *         ownership of the reference.
 */
static GdkPixbuf *icon_pixbuf_scale(GdkPixbuf *pixbuf)
{
        ASSERT_OR_RET(pixbuf, NULL);

        int w = gdk_pixbuf_get_width(pixbuf);
        int h = gdk_pixbuf_get_height(pixbuf);

        if (icon_size_clamp(&w, &h)) {
                GdkPixbuf *scaled = gdk_pixbuf_scale_simple(
                                pixbuf,
                                w,
                                h,
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
        gint w, h;

        if (!gdk_pixbuf_get_file_info (path, &w, &h)) {
                LOG_W("Failed to load image info for %s", filename);
                g_free(path);
                return NULL;
        }
        icon_size_clamp(&w, &h);
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(path,
                                                              w,
                                                              h,
                                                              TRUE,
                                                              &error);

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

GdkPixbuf *icon_get_for_name(const char *name, char **id)
{
        ASSERT_OR_RET(name, NULL);
        ASSERT_OR_RET(id, NULL);

        GdkPixbuf *pb = get_pixbuf_from_icon(name);
        if (pb)
                *id = g_strdup(name);
        return pb;
}

GdkPixbuf *icon_get_for_data(GVariant *data, char **id)
{
        ASSERT_OR_RET(data, NULL);
        ASSERT_OR_RET(id, NULL);

        if (!STR_EQ("(iiibiiay)", g_variant_get_type_string(data))) {
                LOG_W("Invalid data for pixbuf given.");
                return NULL;
        }

        /* The raw image is a big array of char data.
         *
         * The image is serialised rowwise pixel by pixel. The rows are aligned
         * by a spacer full of garbage. The overall data length of data + garbage
         * is called the rowstride.
         *
         * Mind the missing spacer at the last row.
         *
         * len:     |<--------------rowstride---------------->|
         * len:     |<-width*pixelstride->|
         * row 1:   |   data for row 1    | spacer of garbage |
         * row 2:   |   data for row 2    | spacer of garbage |
         *          |         .           | spacer of garbage |
         *          |         .           | spacer of garbage |
         *          |         .           | spacer of garbage |
         * row n-1: |   data for row n-1  | spacer of garbage |
         * row n:   |   data for row n    |
         */

        GdkPixbuf *pixbuf = NULL;
        GVariant *data_variant = NULL;
        unsigned char *data_pb;

        gsize len_expected;
        gsize len_actual;
        gsize pixelstride;

        int width;
        int height;
        int rowstride;
        int has_alpha;
        int bits_per_sample;
        int n_channels;

        g_variant_get(data,
                      "(iiibii@ay)",
                      &width,
                      &height,
                      &rowstride,
                      &has_alpha,
                      &bits_per_sample,
                      &n_channels,
                      &data_variant);

        // note: (A+7)/8 rounds up A to the next byte boundary
        pixelstride = (n_channels * bits_per_sample + 7)/8;
        len_expected = (height - 1) * rowstride + width * pixelstride;
        len_actual = g_variant_get_size(data_variant);

        if (len_actual != len_expected) {
                LOG_W("Expected image data to be of length %" G_GSIZE_FORMAT
                      " but got a length of %" G_GSIZE_FORMAT,
                      len_expected,
                      len_actual);
                g_variant_unref(data_variant);
                return NULL;
        }

        data_pb = (guchar *) g_memdup(g_variant_get_data(data_variant), len_actual);

        pixbuf = gdk_pixbuf_new_from_data(data_pb,
                                          GDK_COLORSPACE_RGB,
                                          has_alpha,
                                          bits_per_sample,
                                          width,
                                          height,
                                          rowstride,
                                          (GdkPixbufDestroyNotify) g_free,
                                          data_pb);
        if (!pixbuf) {
                /* Dear user, I'm sorry, I'd like to give you a more specific
                 * error message. But sadly, I can't */
                LOG_W("Cannot serialise raw icon data into pixbuf.");
                return NULL;
        }

        /* To calculate a checksum of the current image, we have to remove
         * all excess spacers, so that our checksummed memory only contains
         * real data. */
        size_t data_chk_len = pixelstride * width * height;
        unsigned char *data_chk = g_malloc(data_chk_len);
        size_t rowstride_short = pixelstride * width;

        for (int i = 0; i < height; i++) {
                memcpy(data_chk + (i*rowstride_short),
                       data_pb  + (i*rowstride),
                       rowstride_short);
        }

        *id = g_compute_checksum_for_data(G_CHECKSUM_MD5, data_chk, data_chk_len);

        g_free(data_chk);
        g_variant_unref(data_variant);

        pixbuf = icon_pixbuf_scale(pixbuf);

        return pixbuf;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

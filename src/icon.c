#include "icon.h"

#include <assert.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdbool.h>
#include <string.h>
#include <glob.h>
#include <errno.h>

#include "log.h"
#include "notification.h"
#include "settings.h"
#include "utils.h"

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

int get_icon_width(cairo_surface_t *icon, double scale) {
        return round(cairo_image_surface_get_width(icon) / scale);
}

int get_icon_height(cairo_surface_t *icon, double scale) {
        return round(cairo_image_surface_get_height(icon) / scale);
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
 * @param dpi_scale An integer for the dpi scaling. That doesn't mean the icon
 *                  is always scaled by dpi_scale.
 * @return the scaled version of the pixbuf. If scaling wasn't
 *         necessary, it returns the same pixbuf. Transfers full
 *         ownership of the reference.
 */
static GdkPixbuf *icon_pixbuf_scale(GdkPixbuf *pixbuf, double dpi_scale)
{
        ASSERT_OR_RET(pixbuf, NULL);

        int w = gdk_pixbuf_get_width(pixbuf);
        int h = gdk_pixbuf_get_height(pixbuf);


        // TODO immediately rescale icon upon scale changes
        if (icon_size_clamp(&w, &h)) {
                GdkPixbuf *scaled = gdk_pixbuf_scale_simple(
                                pixbuf,
                                round(w * dpi_scale),
                                round(h * dpi_scale),
                                GDK_INTERP_BILINEAR);
                g_object_unref(pixbuf);
                pixbuf = scaled;
        }

        return pixbuf;
}

GdkPixbuf *get_pixbuf_from_file(const char *filename, double scale)
{
        char *path = string_to_path(g_strdup(filename));
        GError *error = NULL;
        gint w, h;

        if (!gdk_pixbuf_get_file_info (path, &w, &h)) {
                LOG_W("Failed to load image info for %s", filename);
                g_free(path);
                return NULL;
        }
        // TODO immediately rescale icon upon scale changes
        icon_size_clamp(&w, &h);
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(path,
                                                              round(w * scale),
                                                              round(h * scale),
                                                              TRUE,
                                                              &error);

        if (error) {
                LOG_W("%s", error->message);
                g_error_free(error);
        }

        g_free(path);
        return pixbuf;
}

// Attempt to find a full path for icon_name, which may be:
// - An absolute path, which will simply be returned (as a newly allocated string)
// - A file:// URI, which will be converted to an absolute path
// - A Freedesktop icon name, which will be resolved within the configured
//   `icon-path` using something that looks vaguely like the algorithm defined
//   in the icon theme spec (https://standards.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html)
//
// Returns the resolved path, or NULL if it was unable to find an icon. The
// return value must be freed by the caller.
char *get_path_from_icon_name(const char *icon_name)
{
        if (STR_EMPTY(icon_name))
                return NULL;

        if (g_str_has_prefix(icon_name, "file://")) {
                return g_filename_from_uri(icon_name, NULL, NULL);
        }

        if (icon_name[0] == '/' || icon_name[0] == '~') {
                return g_strdup(icon_name);
        }

        int32_t max_scale = 1;
        // TODO Determine the largest scale factor of any attached output.
        /* struct mako_output *output = NULL; */
        /* wl_list_for_each(output, &notif->state->outputs, link) { */
        /* if (output->scale > max_scale) { */
        /* max_scale = output->scale; */
        /* } */
        /* } */

        static const char fallback[] = "%s:/usr/share/icons/hicolor";
        char *search = g_strdup_printf(fallback, settings.icon_path);

        char *saveptr = NULL;
        char *theme_path = strtok_r(search, ":", &saveptr);

        // Match all icon files underneath of the theme_path followed by any icon
        // size and category subdirectories. This pattern assumes that all the
        // files in the icon path are valid icon types.
        static const char pattern_fmt[] = "%s/*/*/%s.*";

        char *icon_path = NULL;
        int32_t last_icon_size = 0;
        while (theme_path) {
                if (strlen(theme_path) == 0) {
                        continue;
                }

                glob_t icon_glob = {0};
                char *pattern = g_strdup_printf(pattern_fmt, theme_path, icon_name);

                // Disable sorting because we're going to do our own anyway.
                int found = glob(pattern, GLOB_NOSORT, NULL, &icon_glob);
                size_t found_count = 0;
                if (found == 0) {
                        // The value of gl_pathc isn't guaranteed to be usable if glob
                        // returns non-zero.
                        found_count = icon_glob.gl_pathc;
                }

                for (size_t i = 0; i < found_count; ++i) {
                        char *relative_path = icon_glob.gl_pathv[i];

                        // Find the end of the current search path and walk to the next
                        // path component. Hopefully this will be the icon resolution
                        // subdirectory.
                        relative_path += strlen(theme_path);
                        while (relative_path[0] == '/') {
                                ++relative_path;
                        }

                        errno = 0;
                        int32_t icon_size = strtol(relative_path, NULL, 10);
                        if (errno || icon_size == 0) {
                                // Try second level subdirectory if failed.
                                errno = 0;
                                while (relative_path[0] != '/') {
                                        ++relative_path;
                                }
                                ++relative_path;
                                icon_size = strtol(relative_path, NULL, 10);
                                if (errno || icon_size == 0) {
                                        continue;
                                }
                        }

                        int32_t icon_scale = 1;
                        char *scale_str = strchr(relative_path, '@');
                        if (scale_str != NULL) {
                                icon_scale = strtol(scale_str + 1, NULL, 10);
                        }

                        if (icon_size == settings.max_icon_size &&
                                        icon_scale == max_scale) {
                                // If we find an exact match, we're done.
                                free(icon_path);
                                icon_path = strdup(icon_glob.gl_pathv[i]);
                                break;
                        } else if (icon_size < settings.max_icon_size * max_scale &&
                                        icon_size > last_icon_size) {
                                // Otherwise, if this icon is small enough to fit but bigger
                                // than the last best match, choose it on a provisional basis.
                                // We multiply by max_scale to increase the odds of finding an
                                // icon which looks sharp on the highest-scale output.
                                free(icon_path);
                                icon_path = strdup(icon_glob.gl_pathv[i]);
                                last_icon_size = icon_size;
                        }
                }

                free(pattern);
                globfree(&icon_glob);

                if (icon_path) {
                        // The spec says that if we find any match whatsoever in a theme,
                        // we should stop there to avoid mixing icons from different
                        // themes even if one is a better size.
                        break;
                }
                theme_path = strtok_r(NULL, ":", &saveptr);
        }

        if (icon_path == NULL) {
                // Finally, fall back to looking in /usr/share/pixmaps. These are
                // unsized icons, which may lead to downscaling, but some apps are
                // still using it.
                static const char pixmaps_fmt[] = "/usr/share/pixmaps/%s.*";

                char *pattern = g_strdup_printf(pixmaps_fmt, icon_name);

                glob_t icon_glob = {0};
                int found = glob(pattern, GLOB_NOSORT, NULL, &icon_glob);

                if (found == 0 && icon_glob.gl_pathc > 0) {
                        icon_path = strdup(icon_glob.gl_pathv[0]);
                }
                free(pattern);
                globfree(&icon_glob);
        }

        free(search);
        return icon_path;
}

GdkPixbuf *get_pixbuf_from_icon(const char *iconname, double scale)
{
        char *path = get_path_from_icon_name(iconname);
        if (!path) {
                LOG_W("Could not find icon %s in path.\nThe way icon path works recently changed. Take a look at the dunst(5) man page for more info.", iconname);
                return NULL;
        }

        GdkPixbuf *pixbuf = NULL;

        pixbuf = get_pixbuf_from_file(path, scale);
        g_free(path);

        if (!pixbuf)
                LOG_W("Unable to convert icon to pixbuf: '%s'", path);

        return pixbuf;
}

GdkPixbuf *icon_get_for_name(const char *name, char **id, double scale)
{
        ASSERT_OR_RET(name, NULL);
        ASSERT_OR_RET(id, NULL);

        GdkPixbuf *pb = get_pixbuf_from_icon(name, scale);
        if (pb)
                *id = g_strdup(name);
        return pb;
}

GdkPixbuf *icon_get_for_data(GVariant *data, char **id, double dpi_scale)
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

        // g_memdup is deprecated in glib 2.67.4 and higher.
        // g_memdup2 is a safer alternative
#if GLIB_CHECK_VERSION(2,67,3)
        data_pb = (guchar *) g_memdup2(g_variant_get_data(data_variant), len_actual);
#else
        data_pb = (guchar *) g_memdup(g_variant_get_data(data_variant), len_actual);
#endif

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

        pixbuf = icon_pixbuf_scale(pixbuf, dpi_scale);

        return pixbuf;
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

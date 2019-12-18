#include <gdk-pixbuf/gdk-pixbuf.h>

#include "helpers.h"

GVariant *notification_setup_raw_image(const char *path)
{
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, NULL);

        if (!pb)
                return NULL;

        GVariant *hint_data = g_variant_new_from_data(
                                G_VARIANT_TYPE("ay"),
                                gdk_pixbuf_read_pixels(pb),
                                gdk_pixbuf_get_byte_length(pb),
                                TRUE,
                                (GDestroyNotify) g_object_unref,
                                g_object_ref(pb));

        GVariant *hint = g_variant_new(
                                "(iiibii@ay)",
                                gdk_pixbuf_get_width(pb),
                                gdk_pixbuf_get_height(pb),
                                gdk_pixbuf_get_rowstride(pb),
                                gdk_pixbuf_get_has_alpha(pb),
                                gdk_pixbuf_get_bits_per_sample(pb),
                                gdk_pixbuf_get_n_channels(pb),
                                hint_data);

        g_object_unref(pb);

        return hint;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

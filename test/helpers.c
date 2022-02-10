#include <gdk-pixbuf/gdk-pixbuf.h>

#include "helpers.h"
#include "../src/notification.h"
#include "../src/utils.h"

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

struct notification *test_notification_uninitialized(const char *name)
{
        struct notification *n = notification_create();

        n->dbus_client = g_strconcat(":", name, NULL);
        n->appname =     g_strconcat("app of ", name, NULL);
        n->summary =     g_strconcat(name, NULL);
        n->body =        g_strconcat("See, ", name, ", I've got a body for you!", NULL);

        return n;
}

struct notification *test_notification(const char *name, gint64 timeout)
{
        struct notification *n = test_notification_uninitialized(name);

        notification_init(n);

        n->format = "%s\n%b";

        if (timeout != -1)
                n->timeout = S2US(timeout);

        return n;
}

struct notification *test_notification_with_icon(const char *name, gint64 timeout)
{
        struct notification *n = test_notification(name, timeout);
        n->icon = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        return n;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

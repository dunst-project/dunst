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
        notification_replace_format(n, "%s\n%b");

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

GSList *get_dummy_notifications(int count)
{
        GSList *notifications = NULL;

        int message_size = 24;
        for (int i = 0; i < count; i++) {
                char msg[message_size];
                snprintf(msg, message_size, "test %d", i);
                struct notification *n = test_notification(msg, 10);
                n->icon_position = ICON_LEFT;
                n->text_to_render = g_strdup("dummy layout");
                notifications = g_slist_append(notifications, n);
        }
        return notifications;
}

void free_dummy_notification(void *notification)
{
        // wrapper function to work with g_slist_free_full
        notification_unref((struct notification *) notification);
}

bool gradient_same(struct gradient *grad, struct gradient *grad2)
{
        if (grad == grad2)
                return true;

        if (grad->length != grad2->length)
                return false;

        for (size_t i = 0; i < grad->length; i++) {
                if (!COLOR_SAME(grad->colors[i], grad2->colors[i]))
                        return false;
        }

        return true;
}

bool gradient_check_color(struct gradient *grad, struct color col)
{
        if (grad == NULL || grad->length != 1)
                return false;

        return COLOR_SAME(col, grad->colors[0]);
}

struct gradient *gradient_from_color(struct color col)
{
        struct gradient *grad = gradient_alloc(1);
        grad->colors[0] = col;
        gradient_pattern(grad);
        return grad;
}

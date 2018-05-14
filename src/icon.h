#ifndef DUNST_ICON_H
#define DUNST_ICON_H

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "notification.h"

cairo_surface_t *gdk_pixbuf_to_cairo_surface(GdkPixbuf *pixbuf);

/** Retrieve an icon by its name sent via the notification bus
 *
 * @param iconname A string describing a `file://` URL, an arbitary filename
 *                 or an icon name, which then gets searched for in the
 *                 settings.icon_path
 *
 * @return an instance of `GdkPixbuf` or `NULL` if not found
 */
GdkPixbuf *get_pixbuf_from_icon(const char *iconname);

/** Convert a RawImage to a `GdkPixbuf`
 */
GdkPixbuf *get_pixbuf_from_raw_image(const RawImage *raw_image);

/**
 * Get a cairo surface with the appropriate icon for the notification, scaled
 * according to the current settings
 *
 * @return a cairo_surface_t pointer or NULL if no icon could be retrieved.
 */
cairo_surface_t *icon_get_for_notification(const notification *n);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

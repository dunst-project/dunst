#ifndef DUNST_ICON_H
#define DUNST_ICON_H

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "notification.h"

cairo_surface_t *gdk_pixbuf_to_cairo_surface(GdkPixbuf *pixbuf);

/**
 * Scales the given GdkPixbuf if necessary according to the settings.
 *
 * @param pixbuf (nullable) The pixbuf, which may be too big.
 *                          Takes ownership of the reference.
 * @return the scaled version of the pixbuf. If scaling wasn't
 *         necessary, it returns the same pixbuf. Transfers full
 *         ownership of the reference.
 */
GdkPixbuf *icon_pixbuf_scale(GdkPixbuf *pixbuf);

/** Retrieve an icon by its full filepath.
 *
 * @param filename A string representing a readable file path
 *
 * @return an instance of `GdkPixbuf` or `NULL` if file does not exist
 */
GdkPixbuf *get_pixbuf_from_file(const char *filename);

/** Retrieve an icon by its name sent via the notification bus
 *
 * @param iconname A string describing a `file://` URL, an arbitary filename
 *                 or an icon name, which then gets searched for in the
 *                 settings.icon_path
 *
 * @return an instance of `GdkPixbuf` or `NULL` if not found
 */
GdkPixbuf *get_pixbuf_from_icon(const char *iconname);

/** Convert a struct raw_image to a `GdkPixbuf`
 */
GdkPixbuf *get_pixbuf_from_raw_image(const struct raw_image *raw_image);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

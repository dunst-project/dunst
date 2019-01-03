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

/** Read an icon from disk and convert it to a GdkPixbuf.
 *
 * The returned id will be a unique identifier. To check if two given
 * GdkPixbufs are equal, it's sufficient to just compare the id strings.
 *
 * @param name A string describing and icon. May be a full path, a file path or
 *             just a simple name. If it's a name without a slash, the icon will
 *             get searched in the folders of the icon_path setting.
 * @param id   (necessary) A unique identifier of the returned pixbuf. Only filled,
 *             if the return value is non-NULL.
 * @return     a pixbuf representing name's image.
 *             If an invalid path given, it will return NULL.
 */
GdkPixbuf *icon_get_for_name(const char *name, char **id);

/** Convert a GVariant like described in GdkPixbuf
 *
 * The returned id will be a unique identifier. To check if two given
 * GdkPixbufs are equal, it's sufficient to just compare the id strings.
 *
 * @param data A GVariant in the format "(iiibii@ay)" filled with values
 *             like described in the notification spec.
 * @param id   (necessary) A unique identifier of the returned pixbuf.
 *             Only filled, if the return value is non-NULL.
 * @return     a pixbuf representing name's image.
 *             If an invalid GVariant is passed, it will return NULL.
 */
GdkPixbuf *icon_get_for_data(GVariant *data, char **id);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

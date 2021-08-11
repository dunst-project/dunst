#ifndef DUNST_ICON_H
#define DUNST_ICON_H

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "notification.h"

cairo_surface_t *gdk_pixbuf_to_cairo_surface(GdkPixbuf *pixbuf);

/** Retrieve an icon by its full filepath, scaled according to settings.
 *
 * @param filename A string representing a readable file path
 * @param scale An integer representing the output dpi scaling.
 *
 * @return an instance of `GdkPixbuf`
 * @retval NULL: file does not exist, not readable, etc..
 */
GdkPixbuf *get_pixbuf_from_file(const char *filename, double scale);


/**
 * Get the unscaled icon width.
 *
 * If scale is 2 for example, the icon will render in twice the size, but
 * get_icon_width still returns the same size as when scale is 1.
 */
int get_icon_width(cairo_surface_t *icon, double scale);

/**
 * Get the unscaled icon height, see get_icon_width.
 */
int get_icon_height(cairo_surface_t *icon, double scale);

/** Retrieve a path from an icon name.
 *
 * @param iconname A string describing a `file://` URL, an arbitary filename
 *                 or an icon name, which then gets searched for in the
 *                 settings.icon_path
 *
 * @return a newly allocated string with the icon path
 * @retval NULL: file does not exist, not readable, etc..
 */
char *get_path_from_icon_name(const char *iconname);

/** Retrieve an icon by its name sent via the notification bus, scaled according to settings
 *
 * @param iconname A string describing a `file://` URL, an arbitary filename
 *                 or an icon name, which then gets searched for in the
 *                 settings.icon_path
 * @param scale An integer representing the output dpi scaling.
 *
 * @return an instance of `GdkPixbuf`
 * @retval NULL: file does not exist, not readable, etc..
 */
GdkPixbuf *get_pixbuf_from_icon(const char *iconname, double scale);

/** Read an icon from disk and convert it to a GdkPixbuf, scaled according to settings
 *
 * The returned id will be a unique identifier. To check if two given
 * GdkPixbufs are equal, it's sufficient to just compare the id strings.
 *
 * @param name A string describing and icon. May be a full path, a file path or
 *             just a simple name. If it's a name without a slash, the icon will
 *             get searched in the folders of the icon_path setting.
 * @param id   (necessary) A unique identifier of the returned pixbuf. Only filled,
 *             if the return value is non-NULL.
 * @param dpi_scale An integer representing the output dpi scaling.
 * @return an instance of `GdkPixbuf`, representing the name's image
 * @retval NULL: Invalid path given
 */
GdkPixbuf *icon_get_for_name(const char *name, char **id, double dpi_scale);

/** Convert a GVariant like described in GdkPixbuf, scaled according to settings
 *
 * The returned id will be a unique identifier. To check if two given
 * GdkPixbufs are equal, it's sufficient to just compare the id strings.
 *
 * @param data A GVariant in the format "(iiibii@ay)" filled with values
 *             like described in the notification spec.
 * @param id   (necessary) A unique identifier of the returned pixbuf.
 *             Only filled, if the return value is non-NULL.
 * @param scale An integer representing the output dpi scaling.
 * @return an instance of `GdkPixbuf` derived from the GVariant
 * @retval NULL: GVariant parameter nulled, invalid or in wrong format
 */
GdkPixbuf *icon_get_for_data(GVariant *data, char **id, double scale);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

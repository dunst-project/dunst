#ifndef DUNST_ICON_H
#define DUNST_ICON_H

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "notification.h"

cairo_surface_t *gdk_pixbuf_to_cairo_surface(GdkPixbuf *pixbuf);
GdkPixbuf *get_pixbuf_from_path(char *icon_path);
GdkPixbuf *get_pixbuf_from_raw_image(const RawImage *raw_image);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

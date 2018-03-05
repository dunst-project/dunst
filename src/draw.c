#include "draw.h"

#include <cairo.h>
#include <pango/pango-attributes.h>
#include <pango/pango-font.h>
#include <pango/pango-layout.h>
#include <pango/pango-types.h>
#include <pango/pangocairo.h>

#include "notification.h"
#include "x11/x.h"

typedef struct {
        PangoLayout *l;
        color_t fg;
        color_t bg;
        color_t frame;
        char *text;
        PangoAttrList *attr;
        cairo_surface_t *icon;
        notification *n;
} colored_layout;

cairo_surface_t *root_surface;
cairo_t *c_context;

PangoFontDescription *pango_fdesc;

void draw_setup(void)
{
        x_setup();

        root_surface = x_create_cairo_surface();
        c_context = cairo_create(root_surface);
        pango_fdesc = pango_font_description_from_string(settings.font);
}

void draw(void)
{
        x_win_draw();
}

void draw_deinit(void)
{
        cairo_destroy(c_context);
        cairo_surface_destroy(root_surface);

        x_free();
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

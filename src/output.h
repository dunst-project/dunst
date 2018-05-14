#ifndef DUNST_OUTPUT_H
#define DUNST_OUTPUT_H

#include <stdbool.h>
#include <glib.h>
#include <cairo.h>

typedef gpointer window;

struct dimensions {
        int x;
        int y;
        int w;
        int h;

        int corner_radius;
};

#endif

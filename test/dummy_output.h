#ifndef DUNST_TEST_DUMMY_OUTPUT_H
#include "../src/output.h"

#include <cairo.h>

struct dummy_window {
        bool visible;
        cairo_surface_t *srf;
        cairo_t *cairo_ctx;
};

struct output_params {
        bool idle;
        bool fullscreen;
        struct screen_info *screen;
};

void dummy_set_params(struct output_params*);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

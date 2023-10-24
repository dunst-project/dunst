#ifndef DUNST_DRAW_H
#define DUNST_DRAW_H

#include <stdbool.h>
#include <cairo.h>
#include "output.h"

extern window win; // Temporary
extern const struct output *output;

void draw_setup(void);

void draw(void);

/**
 * Specify which corner to draw in draw_rouned_rect
 *
 *  C_TOP_LEFT   0001
 *  C_TOP_RIGHT  0010
 *  C_BOT_LEFT   0100
 *  C_BOT_RIGHT  1000
 */
enum corner_pos {
        C_NONE = 0,
        C_TOP_LEFT = 1 << 0,
        C_TOP_RIGHT = 1 << 1,
        C_BOT_LEFT = 1 << 2,
        C_BOT_RIGHT = 1 << 3,

        // Handy combinations of the corners
        C_TOP = C_TOP_LEFT | C_TOP_RIGHT,
        C_BOT = C_BOT_LEFT | C_BOT_RIGHT,
        C_LEFT = C_TOP_LEFT | C_BOT_LEFT,
        C_RIGHT = C_TOP_RIGHT | C_BOT_RIGHT,
        C_ALL = C_TOP | C_BOT,

        // These two values are internal only and
        // should not be used outside of draw.c !
        _C_FIRST = 1 << 4,
        _C_LAST = 1 << 5,
};

void draw_rounded_rect(cairo_t *c, float x, float y, int width, int height, int corner_radius, double scale, enum corner_pos corners);

// TODO get rid of this function by passing scale to everything that needs it.
double draw_get_scale(void);

void draw_deinit(void);

void calc_window_pos(const struct screen_info *scr, int width, int height, int *ret_x, int *ret_y);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

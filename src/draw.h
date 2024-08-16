#ifndef DUNST_DRAW_H
#define DUNST_DRAW_H

#include <stdbool.h>
#include <cairo.h>
#include "output.h"

extern window win; // Temporary
extern const struct output *output;

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

struct color {
        double r;
        double g;
        double b;
        double a;
};

#define COLOR_UNINIT { -1, -1, -1, -1 }
#define COLOR_VALID(c) ((c).r >= 0 && (c).g >= 0 && (c).b >= 0 && (c).a >= 0 && (c).r <= 1 && (c).g <= 1 && (c).b <= 1 && (c).a <= 1)
#define COLOR_SAME(c1, c2) ((c1).r == (c2).r && (c1).g == (c2).g && (c1).b == (c2).b && (c1).a == (c2).a)

/**
 * Stringify a color struct to a RRGGBBAA string.
 * Returns the buffer on success and NULL if the struct is invalid.
 */
char *color_to_string(struct color c, char buf[10]);

struct gradient {
        cairo_pattern_t *pattern;
        size_t length;
        struct color colors[];
};

#define GRADIENT_VALID(g) ((g) != NULL && (g)->length != 0)

struct gradient *gradient_alloc(size_t length);

void gradient_free(struct gradient *grad);

void gradient_pattern(struct gradient *grad);

char *gradient_to_string(struct gradient *grad);


void draw_setup(void);

void draw(void);

void draw_rounded_rect(cairo_t *c, float x, float y, int width, int height, int corner_radius, double scale, enum corner_pos corners);

// TODO get rid of this function by passing scale to everything that needs it.
double draw_get_scale(void);

void draw_deinit(void);

void calc_window_pos(const struct screen_info *scr, int width, int height, int *ret_x, int *ret_y);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

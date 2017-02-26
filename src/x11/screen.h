/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_SCREEN_H
#define DUNST_SCREEN_H

#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))

typedef struct _dimension_t {
        int x;
        int y;
        unsigned int h;
        unsigned int mmh;
        unsigned int w;
        int mask;
        int negative_width;
} dimension_t;

typedef struct _screen_info {
        int scr;
        dimension_t dim;
} screen_info;

void x_screen_info(screen_info * scr);

#endif

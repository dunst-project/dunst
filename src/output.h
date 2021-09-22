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
        int text_width;
        int text_height;

        int corner_radius;
};

struct screen_info {
        int id;
        int x;
        int y;
        unsigned int h;
        unsigned int mmh;
        unsigned int w;
        int dpi;
};

struct output {
        bool (*init)(void);
        void (*deinit)(void);

        window (*win_create)(void);
        void (*win_destroy)(window);

        void (*win_show)(window);
        void (*win_hide)(window);

        void (*display_surface)(cairo_surface_t *srf, window win, const struct dimensions*);

        cairo_t* (*win_get_context)(window);

        const struct screen_info* (*get_active_screen)(void);

        bool (*is_idle)(void);
        bool (*have_fullscreen_window)(void);

        double (*get_scale)(void);
};

/**
 * return an initialized output, selecting the correct output type from either
 * wayland or X11 according to the settings and environment.
 * When the wayland output fails to initilize, it falls back to X11 output.
 */
const struct output* output_create(bool force_xwayland);

bool is_running_wayland(void);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

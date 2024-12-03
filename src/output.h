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
        char *name;
        int id;
        int x;
        int y;
        unsigned int h;
        unsigned int mmh;
        unsigned int w;
        int dpi;
};

// NOTE: Refactor the output struct
struct output {
        bool (*init)(void);
        void (*deinit)(void);

        window (*win_create)(void);
        void (*win_destroy)(window);

        void (*win_show)(window);
        void (*win_hide)(window);

        void (*display_surface)(cairo_surface_t *srf, window win, const struct dimensions*);

        cairo_t* (*win_get_context)(window);
        cairo_surface_t* (*win_get_surface)(window);

        const struct screen_info* (*get_active_screen)(void);

        bool (*is_idle)(void);
        bool (*have_fullscreen_window)(void);

        double (*get_scale)(void);
};

#ifdef ENABLE_X11
#define X11_SUPPORT 1
#else
#define X11_SUPPORT 0
#endif

#ifdef ENABLE_WAYLAND
#define WAYLAND_SUPPORT 1
#else
#define WAYLAND_SUPPORT 0
#endif

#if !WAYLAND_SUPPORT && !X11_SUPPORT
#error "You have to compile at least one output (X11, Wayland)"
#endif

/**
 * return an initialized output, selecting the correct output type from either
 * wayland or X11 according to the settings and environment.
 * When the wayland output fails to initilize, it falls back to X11 output.
 *
 * Either output is skipped if it was not compiled.
 */
const struct output* output_create(bool force_xwayland);

bool is_running_wayland(void);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

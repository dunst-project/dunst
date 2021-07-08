#ifndef DUNST_WL_H
#define DUNST_WL_H

#include <stdbool.h>
#include <cairo.h>
#include <glib.h>

#include "../output.h"

bool wl_init(void);
void wl_deinit(void);

window wl_win_create(void);
void wl_win_destroy(window);

void wl_win_show(window);
void wl_win_hide(window);

void wl_display_surface(cairo_surface_t *srf, window win, const struct dimensions*);
cairo_t* wl_win_get_context(window);

const struct screen_info* wl_get_active_screen(void);

bool wl_is_idle(void);
bool wl_have_fullscreen_window(void);

// Return the dpi scaling of the current output. Everything that's rendered
// should be multiplied by this value, but don't use it to multiply other
// values. All sizes should be in unscaled units.
double wl_get_scale(void);
#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

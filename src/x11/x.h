/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file
 * @ingroup x11
 * @brief Xorg output wrapper
 * @copyright Copyright 2013-2014 Sascha Kruse
 * @copyright Copyright 2014-2026 Dunst contributors
 * @license BSD-3-Clause
 */

#ifndef DUNST_X_H
#define DUNST_X_H

#define XLIB_ILLEGAL_ACCESS
#include <cairo.h>
#include <glib.h>
#include <stdbool.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "../output.h"
#include "screen.h"

struct x_context {
        Display *dpy;
        XScreenSaverInfo *screensaver_info;
};

extern struct x_context xctx;

/* window */
window x_win_create(void);
void x_win_destroy(window);

void x_win_show(window);
void x_win_hide(window);

void x_display_surface(cairo_surface_t *srf, window, const struct dimensions *dim);

cairo_t* x_win_get_context(window);

/* X misc */
bool x_is_idle(void);
bool x_setup(void);
void x_free(void);

double x_get_scale(void);

#endif

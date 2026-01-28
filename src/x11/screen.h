/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file
 * @ingroup x11
 * @brief Xorg screen managment
 * @copyright Copyright 2013-2014 Sascha Kruse
 * @copyright Copyright 2014-2026 Dunst contributors
 * @license BSD-3-Clause
 */

#ifndef DUNST_SCREEN_H
#define DUNST_SCREEN_H

#include <stdbool.h>
#include <X11/Xlib.h>

#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))

void init_screens(void);
void screen_dpi_xft_cache_purge(void);
bool screen_check_event(XEvent *ev);

const struct screen_info *get_active_screen(void);
double screen_dpi_get(const struct screen_info *scr);

/**
 * Find the currently focused window and check if it's in
 * fullscreen mode
 *
 * @see window_is_fullscreen()
 * @see get_focused_window()
 *
 * @retval true: the focused window is in fullscreen mode
 * @retval false: otherwise
 */
bool have_fullscreen_window(void);

/**
 * Check if window is in fullscreen mode
 *
 * @param window the x11 window object
 * @retval true: \p window is in fullscreen mode
 * @retval false: otherwise
 */
bool window_is_fullscreen(Window window);

#endif

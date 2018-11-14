/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_SCREEN_H
#define DUNST_SCREEN_H

#include <stdbool.h>
#include <X11/Xlib.h>

#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))

struct screen_info {
        int id;
        int x;
        int y;
        unsigned int h;
        unsigned int mmh;
        unsigned int w;
};

void init_screens(void);
void screen_check_event(XEvent event);

struct screen_info *get_active_screen(void);
double get_dpi_for_screen(struct screen_info *scr);

/**
 * Find the currently focused window and check if it's in
 * fullscreen mode
 *
 * @see window_is_fullscreen()
 * @see get_focused_window()
 *
 * @return `true` if the focused window is in fullscreen mode
 */
bool have_fullscreen_window(void);

/**
 * Check if window is in fullscreen mode
 *
 * @param window the x11 window object
 * @return `true` if `window` is in fullscreen mode
 */
bool window_is_fullscreen(Window window);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

#ifndef DUNST_INPUT_H
#define DUNST_INPUT_H

#include <stdbool.h>

/**
 * Handle incoming mouse click events
 *
 * @param button code, A linux input event code
 * @param button_down State of the button
 * @param mouse_x X-position of the mouse, relative to the window
 * @param mouse_y Y-position of the mouse, relative to the window
 *
 */
void input_handle_click(unsigned int button, bool button_down, int mouse_x, int mouse_y);

/**
* Handle incoming key events
* @param keysym The key symbol
* @param True if the key is pressed, false if it is released
*/
void input_handle_key(unsigned int keysym, bool pressed);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

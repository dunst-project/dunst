/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file
 * @ingroup input
 * @brief Input handling for mouse events
 * @copyright Copyright 2020-2026 Dunst contributors
 * @license BSD-3-Clause
 */

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

#endif

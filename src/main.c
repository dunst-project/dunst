/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file
 * @brief Glue that starts dunst_main()
 * @copyright Copyright 2026 Dunst contributors
 * @license BSD-3-Clause
 */

#include "dunst.h"

/**
 * @defgroup config Settings and options
 * Parsing and validation of configuration options.
 *
 * @defgroup graphics Graphics and rendering
 * Drawing logic and generic output code.
 *
 * @defgroup utils Utilities
 * String helpers, icon lookup, and logging.
 *
 * @defgroup x11 Xorg backend
 * Subsystem for supporting X11 input and rendering.
 *
 * @defgroup wayland Wayland backend
 * Subsystem for supporting Wayland input and rendering.
 */

int main(int argc, char *argv[])
{
        return dunst_main(argc, argv);
}

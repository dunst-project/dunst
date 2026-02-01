/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file
 * @ingroup main
 * @brief Main event loop logic
 * @copyright Copyright 2011-2014 Sascha Kruse
 * @copyright Copyright 2014-2026 Dunst contributors
 * @license BSD-3-Clause
 */

#ifndef DUNST_DUNST_H
#define DUNST_DUNST_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#include "notification.h"

#define MAX_PAUSE_LEVEL 100

//!< A structure to describe dunst's global window status
struct dunst_status {
        bool fullscreen; //!< a fullscreen window is currently focused
        int pause_level;    //!< current pause level. 0 = all notifications come through, 100 = no notifications come through
        bool idle;       //!< set true if the user is idle
        bool mouse_over; //!< set true if the mouse is over the notification window
};

enum dunst_status_field {
        S_FULLSCREEN,
        S_IDLE,
        S_PAUSE_LEVEL,
        S_MOUSE_OVER,
};

extern char **config_paths;

/**
 * Modify the current status of dunst
 * @param field The field to change in the global status structure
 * @param value Anything boolean or DO_TOGGLE to toggle the current value
 */
void dunst_status(const enum dunst_status_field field,
                  bool value);
void dunst_status_int(const enum dunst_status_field field,
                  int value);

struct dunst_status dunst_status_get(void);

void wake_up(void);
void reload(char **const configs);

int dunst_main(int argc, char *argv[]);

void usage(int exit_status);
void print_version(void);

gboolean pause_signal(gpointer data);
gboolean unpause_signal(gpointer data);
gboolean quit_signal(gpointer data);

#endif

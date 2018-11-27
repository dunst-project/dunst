/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#ifndef DUNST_DUNST_H
#define DUNST_DUNST_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#include "notification.h"

//!< A structure to describe dunst's global window status
struct dunst_status {
        bool fullscreen; //!< a fullscreen window is currently focused
        bool running;    //!< set true if dunst is currently running
        bool idle;       //!< set true if the user is idle
};

enum dunst_status_field {
        S_FULLSCREEN,
        S_IDLE,
        S_RUNNING,
};

/**
 * Modify the current status of dunst
 * @param field The field to change in the global status structure
 * @param value Anything boolean or DO_TOGGLE to toggle the current value
 */
void dunst_status(const enum dunst_status_field field,
                  bool value);

struct dunst_status dunst_status_get(void);

void wake_up(void);

int dunst_main(int argc, char *argv[]);

void usage(int exit_status);
void print_version(void);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

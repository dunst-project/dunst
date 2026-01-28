/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file
 * @ingroup wayland
 * @brief Wayland output wrapper
 * @copyright Copyright 2026 Dunst contributors
 * @license BSD-3-Clause
 */

#ifndef DUNST_WL_OUTPUT_H
#define DUNST_WL_OUTPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-util.h>

struct dunst_output {
        uint32_t global_name;
        char *name;
        struct wl_output *wl_output;
        struct wl_list link;

        uint32_t scale;
        uint32_t subpixel; // TODO do something with it
        int32_t width, height;
        bool fullscreen;
        struct zwlr_foreign_toplevel_handle_v1 *fullscreen_toplevel; // the toplevel that is fullscreened on this output
};

void create_output(struct wl_registry *registry, uint32_t global_name, uint32_t version);
void destroy_output(struct dunst_output *seat);

#endif

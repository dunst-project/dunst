#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "protocols/wlr-foreign-toplevel-management-unstable-v1-client-header.h"
/* #include "protocols/wlr-foreign-toplevel-management-unstable-v1.h" */

#include "foreign_toplevel.h"
#include "../dunst.h"
#include "wl_output.h"
#include "wl.h"

struct wl_list toplevel_list;

static void noop() {
        // This space intentionally left blank
}

static void copy_state(struct toplevel_state *current,
                struct toplevel_state *pending) {
        if (!(pending->state & TOPLEVEL_STATE_INVALID)) {
                current->state = pending->state;
        }

        pending->state = TOPLEVEL_STATE_INVALID;
}

static uint32_t global_id = 0;

static void toplevel_handle_output_enter(void *data,
                struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
                struct wl_output *wl_output) {
        struct toplevel_v1 *toplevel = data;
        struct toplevel_output *toplevel_output = calloc(1, sizeof(struct toplevel_output));
        struct dunst_output *dunst_output = wl_output_get_user_data(wl_output);
        toplevel_output->dunst_output = dunst_output;

        wl_list_insert(&toplevel->output_list, &toplevel_output->link);
}

static void toplevel_handle_output_leave(void *data,
                struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
                struct wl_output *wl_output) {
        struct toplevel_v1 *toplevel = data;

        struct dunst_output *output = wl_output_get_user_data(wl_output);
        struct toplevel_output *pos, *tmp;
        wl_list_for_each_safe(pos, tmp, &toplevel->output_list, link){
                if (pos->dunst_output->name == output->name) {
                        wl_list_remove(&pos->link);
                }
        }
}

static uint32_t array_to_state(struct wl_array *array) {
        uint32_t state = 0;
        uint32_t *entry;
        wl_array_for_each(entry, array) {
                if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
                        state |= TOPLEVEL_STATE_ACTIVATED;
                if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)
                        state |= TOPLEVEL_STATE_FULLSCREEN;
        }

        return state;
}

static void toplevel_handle_state(void *data,
                struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel,
                struct wl_array *state) {
        struct toplevel_v1 *toplevel = data;
        toplevel->pending.state = array_to_state(state);
}

static void toplevel_handle_done(void *data,
                struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel) {
        struct toplevel_v1 *toplevel = data;

        bool was_fullscreen = wl_have_fullscreen_window();
        copy_state(&toplevel->current, &toplevel->pending);
        bool is_fullscreen = wl_have_fullscreen_window();

        if (was_fullscreen != is_fullscreen) {
                wake_up();
        }
}

static void toplevel_handle_closed(void *data,
                struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel) {
        struct toplevel_v1 *toplevel = data;

        wl_list_remove(&toplevel->link);
        struct toplevel_output *pos, *tmp;
        wl_list_for_each_safe(pos, tmp, &toplevel->output_list, link){
                free(pos);
        }
        free(toplevel);
        zwlr_foreign_toplevel_handle_v1_destroy(zwlr_toplevel);
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_impl = {
        .title = noop,
        .app_id = noop,
        .output_enter = toplevel_handle_output_enter,
        .output_leave = toplevel_handle_output_leave,
        .state = toplevel_handle_state,
        .done = toplevel_handle_done,
        .closed = toplevel_handle_closed,
};

static void toplevel_manager_handle_toplevel(void *data,
                struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager,
                struct zwlr_foreign_toplevel_handle_v1 *zwlr_toplevel) {
        struct toplevel_v1 *toplevel = calloc(1, sizeof(struct toplevel_v1));
        if (!toplevel) {
                fprintf(stderr, "Failed to allocate memory for toplevel\n");
                return;
        }

        toplevel->id = global_id++;
        toplevel->zwlr_toplevel = zwlr_toplevel;
        wl_list_insert(&toplevel_list, &toplevel->link);
        wl_list_init(&toplevel->output_list);

        zwlr_foreign_toplevel_handle_v1_add_listener(zwlr_toplevel, &toplevel_impl,
                toplevel);
}

static void toplevel_manager_handle_finished(void *data,
                struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager) {
        zwlr_foreign_toplevel_manager_v1_destroy(toplevel_manager);
}

const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl = {
        .toplevel = toplevel_manager_handle_toplevel,
        .finished = toplevel_manager_handle_finished,
};
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

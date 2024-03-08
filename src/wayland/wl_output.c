#define _POSIX_C_SOURCE 200112L
#include "wl_output.h"

#include "../dunst.h"
#include "../log.h"
#include "wl_ctx.h"

static void output_handle_geometry(void *data, struct wl_output *wl_output,
                int32_t x, int32_t y, int32_t phy_width, int32_t phy_height,
                int32_t subpixel, const char *make, const char *model,
                int32_t transform) {
        //TODO do something with the subpixel data
        struct dunst_output *output = data;
        output->subpixel = subpixel;
}
static void output_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                int32_t width, int32_t height, int32_t refresh) {
        struct dunst_output *output = data;
        output->width = width;
        output->height = height;
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
                int32_t factor) {
        struct dunst_output *output = data;
        output->scale = factor;

        wake_up();
}

#ifdef WL_OUTPUT_NAME_SINCE_VERSION
static void output_handle_name(void *data, struct wl_output *wl_output,
                const char *name) {
        struct dunst_output *output = data;
        output->name = g_strdup(name);
        LOG_D("Output global %" PRIu32 " name %s", output->global_name, name);
}

static void output_handle_description(void *data, struct wl_output *output, const char* description) {
        // do nothing
}
#endif

static void output_listener_done_handler(void *data, struct wl_output *output) {
        // do nothing
}

static const struct wl_output_listener output_listener = {
        .geometry = output_handle_geometry,
        .mode = output_handle_mode,
        .done = output_listener_done_handler,
        .scale = output_handle_scale,
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
        .name = output_handle_name,
        .description = output_handle_description,
#endif
};

void create_output(struct wl_registry *registry, uint32_t global_name, uint32_t version) {
        struct dunst_output *output = g_malloc0(sizeof(struct dunst_output));
        if (output == NULL) {
                LOG_E("allocation failed");
                return;
        }

        uint32_t max_version = 3;
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
        max_version = WL_OUTPUT_NAME_SINCE_VERSION;
#endif
        bool recreate_surface = false;
        static int number = 0;
        LOG_I("New output found - id %i", number);
        output->global_name = global_name;
        output->wl_output = wl_registry_bind(registry, global_name, &wl_output_interface,
                        CLAMP(version, 3, max_version));
        output->scale = 1;
        output->fullscreen = false;

        recreate_surface = wl_list_empty(&ctx.outputs);

        wl_list_insert(&ctx.outputs, &output->link);

        wl_output_set_user_data(output->wl_output, output);
        wl_output_add_listener(output->wl_output, &output_listener, output);
        number++;

        if (recreate_surface) {
                // We had no outputs, force our surface to redraw
                set_dirty();
        }
}

void destroy_output(struct dunst_output *output) {
        if (ctx.surface_output == output) {
                ctx.surface_output = NULL;
        }
        if (ctx.layer_surface_output == output) {
                ctx.layer_surface_output = NULL;
        }
        wl_list_remove(&output->link);
        wl_output_destroy(output->wl_output);
        g_free(output->name);
        g_free(output);
}
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

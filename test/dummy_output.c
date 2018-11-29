#include <cairo.h>

#include "../src/output.h"
#include "dummy_output.h"

struct output_params *params;

void dummy_init(void)
{
}

void dummy_deinit(void)
{
}

window dummy_win_create(void)
{
        return g_malloc0(sizeof(struct dummy_window));
}

void dummy_win_destroy(window win)
{
        g_free(win);
}

void dummy_win_show(window win)
{
        ((struct dummy_window*)win)->visible = true;
}

void dummy_win_hide(window win)
{
        ((struct dummy_window*)win)->visible = false;
}

void dummy_display_surface(cairo_surface_t *srf, window win, const struct dimensions *dim)
{

}

bool dummy_win_visible(window win)
{
        return ((struct dummy_window*)win)->visible;
}

cairo_t *dummy_win_get_context(window winptr)
{
        struct dummy_window *win = (struct dummy_window*)winptr;
        if (!win->srf) {
                win->srf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 500, 500);
                win->cairo_ctx = cairo_create(win->srf);
        }
        return win->cairo_ctx;
}

const struct screen_info *dummy_get_active_screen(void)
{
        return params->screen;
}

double dummy_get_dpi_for_screen(const struct screen_info *scrinfo)
{
        return 96;
}

bool dummy_is_idle(void)
{
        return params->idle;
}

bool dummy_have_fullscreen_window(void)
{
        return params->fullscreen;
}

const struct output output_dummy = {
        dummy_init,
        dummy_deinit,

        dummy_win_create,
        dummy_win_destroy,

        dummy_win_show,
        dummy_win_hide,

        dummy_display_surface,

        dummy_win_visible,
        dummy_win_get_context,

        dummy_get_active_screen,
        dummy_get_dpi_for_screen,

        dummy_is_idle,
        dummy_have_fullscreen_window
};

void dummy_set_params(struct output_params *p)
{
        params = p;
        output_set(output_dummy);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

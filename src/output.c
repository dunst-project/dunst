#include "output.h"

#include "x11/x.h"
#include "x11/screen.h"

const struct output output_x11 = {
        x_setup,
        x_free,

        x_win_create,
        x_win_destroy,

        x_win_show,
        x_win_hide,

        x_display_surface,
        x_win_visible,
        x_win_get_context,

        get_active_screen,
        get_dpi_for_screen,

        x_is_idle,
        have_fullscreen_window
};

struct output selected_out = {0};

void output_set(struct output out)
{
        selected_out = out;
}

const struct output* output_create(void)
{
        if (selected_out.init != NULL) { // Hacky way to check if there are actual pointers assigned
                return &selected_out;
        } else {
                return &output_x11;
        }
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

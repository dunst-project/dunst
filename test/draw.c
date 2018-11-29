#include "../src/draw.c"

#include "greatest.h"
#include "dummy_output.h"

#include "../src/output.h"

static void test_init() {
        queues_init();
        draw_setup();
}
static void test_teardown() {
        draw_deinit();
        queues_teardown();
}

TEST test_draw_calculate_dimensions(const struct output *output)
{
        test_init();

        test_teardown();
        PASS();
}

SUITE(suite_draw)
{
        struct screen_info screen = {
                .id = 0, .x = 0, .y = 0,
                .w = 1920,
                .h = 1080
        };
        struct output_params out_struct = {
                .idle = false,
                .fullscreen = false,
                .screen = &screen
        };
        dummy_set_params(&out_struct);
        const struct output *output = output_create();
        RUN_TESTp(test_draw_calculate_dimensions, output);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

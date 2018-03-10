#include "greatest.h"

#include <stdbool.h>

#include "src/log.h"

SUITE_EXTERN(suite_utils);
SUITE_EXTERN(suite_option_parser);
SUITE_EXTERN(suite_notification);
SUITE_EXTERN(suite_markup);
SUITE_EXTERN(suite_icon);

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) {
        // do not print out warning messages, when executing tests
        dunst_log_init(true);

        GREATEST_MAIN_BEGIN();
        RUN_SUITE(suite_utils);
        RUN_SUITE(suite_option_parser);
        RUN_SUITE(suite_notification);
        RUN_SUITE(suite_markup);
        RUN_SUITE(suite_icon);
        GREATEST_MAIN_END();
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

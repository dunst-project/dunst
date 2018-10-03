#include "greatest.h"

#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../src/log.h"

const char *base;

SUITE_EXTERN(suite_utils);
SUITE_EXTERN(suite_option_parser);
SUITE_EXTERN(suite_notification);
SUITE_EXTERN(suite_markup);
SUITE_EXTERN(suite_misc);
SUITE_EXTERN(suite_icon);
SUITE_EXTERN(suite_queues);
SUITE_EXTERN(suite_dunst);

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) {
        char *prog = realpath(argv[0], NULL);
        if (!prog) {
                fprintf(stderr, "Cannot determine actual path of test executable: %s\n", strerror(errno));
                exit(1);
        }
        base = dirname(prog);

        // do not print out warning messages, when executing tests
        dunst_log_init(true);

        GREATEST_MAIN_BEGIN();
        RUN_SUITE(suite_utils);
        RUN_SUITE(suite_option_parser);
        RUN_SUITE(suite_notification);
        RUN_SUITE(suite_markup);
        RUN_SUITE(suite_misc);
        RUN_SUITE(suite_icon);
        RUN_SUITE(suite_queues);
        RUN_SUITE(suite_dunst);
        GREATEST_MAIN_END();

        base = NULL;
        free(prog);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

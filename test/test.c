#include "greatest.h"

#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../src/log.h"
#include "../src/settings.h"

const char *base;

SUITE_EXTERN(suite_settings_data);
SUITE_EXTERN(suite_utils);
SUITE_EXTERN(suite_option_parser);
SUITE_EXTERN(suite_notification);
SUITE_EXTERN(suite_markup);
SUITE_EXTERN(suite_misc);
SUITE_EXTERN(suite_icon);
SUITE_EXTERN(suite_queues);
SUITE_EXTERN(suite_dunst);
SUITE_EXTERN(suite_log);
SUITE_EXTERN(suite_menu);
SUITE_EXTERN(suite_dbus);
SUITE_EXTERN(suite_setting);
SUITE_EXTERN(suite_ini);
SUITE_EXTERN(suite_icon_lookup);
SUITE_EXTERN(suite_draw);
SUITE_EXTERN(suite_rules);

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) {
        char *prog = realpath(argv[0], NULL);
        if (!prog) {
                fprintf(stderr, "Cannot determine actual path of test executable: %s\n", strerror(errno));
                exit(1);
        }
        base = dirname(prog);

        /* By default do not print out warning messages, when executing tests.
         * But allow, if DUNST_TEST_LOG=1 is set in environment. */
        const char *log = getenv("DUNST_TEST_LOG");
        bool printlog = log && atoi(log) ? true : false;
        dunst_log_init(!printlog);


        // initialize settings
        char *config_path = g_strconcat(base, "/data/dunstrc.default", NULL);
        load_settings(config_path);

        GREATEST_MAIN_BEGIN();
        RUN_SUITE(suite_utils);
        RUN_SUITE(suite_option_parser);
        RUN_SUITE(suite_notification);
        RUN_SUITE(suite_markup);
        RUN_SUITE(suite_misc);
        RUN_SUITE(suite_icon);
        RUN_SUITE(suite_queues);
        RUN_SUITE(suite_dunst);
        RUN_SUITE(suite_log);
        RUN_SUITE(suite_menu);
        RUN_SUITE(suite_settings_data);
        RUN_SUITE(suite_dbus);
        RUN_SUITE(suite_setting);
        RUN_SUITE(suite_icon_lookup);
        RUN_SUITE(suite_draw);
        RUN_SUITE(suite_rules);

        base = NULL;
        g_free(config_path);
        free(prog);

        // this returns the error code
        GREATEST_MAIN_END();
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* see example dunstrc for additional explanations about these options */

struct rule default_rules[] = {
        /* name can be any unique string. It is used to identify
         * the rule in dunstrc to override it there
         */

        /* an empty rule with no effect */
        {
                .name = "empty",
                .appname         = NULL,
                .summary         = NULL,
                .body            = NULL,
                .icon            = NULL,
                .category        = NULL,
                .msg_urgency     = -1,
                .timeout         = -1,
                .urgency         = -1,
                .markup          = MARKUP_NULL,
                .history_ignore  = -1,
                .match_transient = -1,
                .set_transient   = -1,
                .skip_display    = -1,
                .new_icon        = NULL,
                .fg              = NULL,
                .bg              = NULL,
                .format          = NULL,
                .script          = NULL,
        }
};

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

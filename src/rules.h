/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_RULES_H
#define DUNST_RULES_H

#include <glib.h>
#include <stdbool.h>

#include "notification.h"
#include "settings.h"

struct rule {
        char *name;
        /* filters */
        char *appname;
        char *summary;
        char *body;
        char *icon;
        char *category;
        char *stack_tag;
        char *desktop_entry;
        int msg_urgency;

        /* actions */
        gint64 timeout;
        enum urgency urgency;
        enum markup_mode markup;
        int history_ignore;
        int match_transient;
        int set_transient;
        int skip_display;
        char *new_icon;
        char *fg;
        char *bg;
        char *fc;
        const char *format;
        const char *script;
        enum behavior_fullscreen fullscreen;
        char *set_stack_tag;
};

extern GSList *rules;

/**
 * Allocate a new rule. The rule is fully initialised.
 *
 * @returns A new initialised rule.
 */
struct rule *rule_new(void);

void rule_apply(struct rule *r, struct notification *n);
void rule_apply_all(struct notification *n);
bool rule_matches_notification(struct rule *r, struct notification *n);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

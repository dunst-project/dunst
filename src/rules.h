/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_RULES_H
#define DUNST_RULES_H

#include <glib.h>
#include <stdbool.h>

#include "notification.h"
#include "settings.h"

struct rule {
        // Since there's heavy use of offsets from this class, both in rules.c
        // and in settings_data.h the layout of the class should not be
        // changed, unless it's well considered and tested. See the comments
        // below for what should not be changed.

        // This has to be the first member, see struct setting.rule_offset.
        char *name;

        /* filters */
        char *appname; // this has to be the first filter, see rules.c
        char *summary;
        char *body;
        char *icon;
        char *category;
        char *stack_tag;
        char *desktop_entry;
        int msg_urgency;

        /* modifying */
        gint64 timeout; // this has to be the first modifying rule
        enum urgency urgency;
        char *action_name;
        enum markup_mode markup;
        int history_ignore;
        int match_transient;
        int set_transient;
        int skip_display;
        int word_wrap;
        int ellipsize;
        int alignment;
        int hide_text;
        int icon_position;
        int set_icon_size;
        char *new_icon;
        char *fg;
        char *bg;
        char *highlight;
        char *default_icon;
        char *fc;
        char *set_category;
        const char *format;
        const char *script;
        enum behavior_fullscreen fullscreen;
        bool enabled;
        int progress_bar_alignment;
        char *set_stack_tag; // this has to be the last modifying rule
};

extern GSList *rules;

/**
 * Allocate a new rule with given name. The rule is fully initialised. If the
 * name is one of a special section (see settings_data.h), the rule is
 * initialized with some filters, and you should not add any filters after
 * that.
 *
 * @param name Name of the rule.
 *
 * @returns A new initialised rule.
 */
struct rule *rule_new(const char *name);

void rule_apply(struct rule *r, struct notification *n);
void rule_apply_all(struct notification *n);
bool rule_matches_notification(struct rule *r, struct notification *n);

/**
 * Get rule with this name from rules
 *
 * @returns the rule that matches. Null if no rule matches
 */
struct rule *get_rule(const char* name);

/**
 * Check if a rule is an action
 *
 * @returns a boolean if the rule is an action
 */
bool rule_offset_is_modifying(const size_t offset);

/**
 * Check if a rule is an filter
 *
 * @returns a boolean if the rule is an filter
 */
bool rule_offset_is_filter(const size_t offset);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "rules.h"

#include <fnmatch.h>
#include <glib.h>
#include <stddef.h>
#include <regex.h>

#include "dunst.h"
#include "utils.h"
#include "settings_data.h"
#include "log.h"

GSList *rules = NULL;

/*
 * Apply rule to notification.
 */
void rule_apply(struct rule *r, struct notification *n)
{
        if (r->timeout != -1)
                n->timeout = r->timeout;
        if (r->urgency != URG_NONE)
                n->urgency = r->urgency;
        if (r->fullscreen != FS_NULL)
                n->fullscreen = r->fullscreen;
        if (r->history_ignore != -1)
                n->history_ignore = r->history_ignore;
        if (r->set_transient != -1)
                n->transient = r->set_transient;
        if (r->skip_display != -1)
                n->skip_display = r->skip_display;
        if (r->word_wrap != -1)
                n->word_wrap = r->word_wrap;
        if (r->ellipsize != -1)
                n->ellipsize = r->ellipsize;
        if (r->alignment != -1)
                n->alignment = r->alignment;
        if (r->hide_text != -1)
                n->hide_text = r->hide_text;
        if (r->progress_bar_alignment != -1)
                n->progress_bar_alignment = r->progress_bar_alignment;
        if (r->action_name) {
                g_free(n->default_action_name);
                n->default_action_name = g_strdup(r->action_name);
        }
        if (r->set_category) {
                g_free(n->category);
                n->category = g_strdup(r->set_category);
        }
        if (r->markup != MARKUP_NULL)
                n->markup = r->markup;
        if (r->icon_position != -1)
                n->icon_position = r->icon_position;
        if (r->set_icon_size > 0)
                n->icon_size = r->set_icon_size;
        if (r->fg) {
                g_free(n->colors.fg);
                n->colors.fg = g_strdup(r->fg);
        }
        if (r->bg) {
                g_free(n->colors.bg);
                n->colors.bg = g_strdup(r->bg);
        }
        if (r->highlight) {
                g_free(n->colors.highlight);
                n->colors.highlight = g_strdup(r->highlight);
        }
        if (r->fc) {
                g_free(n->colors.frame);
                n->colors.frame = g_strdup(r->fc);
        }
        if (r->format)
                n->format = r->format;
        if (r->default_icon) {
                g_free(n->default_icon_name);
                n->default_icon_name = g_strdup(r->default_icon);
        }
        if (r->new_icon) {
                // FIXME This is not efficient when the icon is replaced
                // multiple times for the same notification. To fix this, a
                // separate variable is needed to track if the icon is
                // replaced, like in 86cbc1d34bb0f551461dbd466cd9e4860ae01817.
                notification_icon_replace_path(n, r->new_icon);
        }
        if (r->script){
                n->scripts = g_renew(const char*,n->scripts,n->script_count + 1);
                n->scripts[n->script_count] = r->script;

                n->script_count++;
        }
        if (r->set_stack_tag) {
                g_free(n->stack_tag);
                n->stack_tag = g_strdup(r->set_stack_tag);
        }
}

/*
 * Check all rules if they match n and apply.
 */
void rule_apply_all(struct notification *n)
{
        for (GSList *iter = rules; iter; iter = iter->next) {
                struct rule *r = iter->data;
                if (rule_matches_notification(r, n)) {
                        rule_apply(r, n);
                }
        }
}

bool rule_apply_special_filters(struct rule *r, const char *name) {
        if (is_deprecated_section(name)) // shouldn't happen, but just in case
                return false;

        if (strcmp(name, "global") == 0) {
                // no filters for global section
                return true;
        }
        if (strcmp(name, "urgency_low") == 0) {
                r->msg_urgency = URG_LOW;
                return true;
        }
        if (strcmp(name, "urgency_normal") == 0) {
                r->msg_urgency = URG_NORM;
                return true;
        }
        if (strcmp(name, "urgency_critical") == 0) {
                r->msg_urgency = URG_CRIT;
                return true;
        }

        return false;
}

struct rule *rule_new(const char *name)
{
        struct rule *r = g_malloc0(sizeof(struct rule));
        *r = empty_rule;
        rules = g_slist_insert(rules, r, -1);
        r->name = g_strdup(name);
        if (is_special_section(name)) {
                bool success = rule_apply_special_filters(r, name);
                if (!success) {
                        LOG_M("Could not apply special filters for section %s", name);
                }
        }
        return r;
}

static inline bool rule_field_matches_string(const char *value, const char *pattern)
{
        if (settings.enable_regex) {
                if (!pattern) {
                        return true;
                }
                if (!value) {
                        return false;
                }
                regex_t     regex;

                // TODO compile each regex only once
                int err = regcomp(&regex, pattern, REG_NEWLINE | REG_EXTENDED | REG_NOSUB);
                if (err) {
                        size_t err_size = regerror(err, &regex, NULL, 0);
                        char *err_buf = malloc(err_size);
                        regerror(err, &regex, err_buf, err_size);
                        LOG_W("%s: \"%s\"", err_buf, pattern);
                        free(err_buf);
                        return false;
                }

                for (int i = 0; ; i++) {
                        if (regexec(&regex, value, 0, NULL, 0))
                                break;
                        regfree(&regex);
                        return true;
                }
                regfree(&regex);
                return false;
        } else {
                return !pattern || (value && !fnmatch(pattern, value, 0));
        }
}

/*
 * Check whether rule should be applied to n.
 */
bool rule_matches_notification(struct rule *r, struct notification *n)
{
        return  r->enabled
                && (r->msg_urgency == URG_NONE || r->msg_urgency == n->urgency)
                && (r->match_transient == -1 || (r->match_transient == n->transient))
                && rule_field_matches_string(n->appname,        r->appname)
                && rule_field_matches_string(n->desktop_entry,  r->desktop_entry)
                && rule_field_matches_string(n->summary,        r->summary)
                && rule_field_matches_string(n->body,           r->body)
                && rule_field_matches_string(n->iconname,       r->icon)
                && rule_field_matches_string(n->category,       r->category)
                && rule_field_matches_string(n->stack_tag,      r->stack_tag);
}

/**
 * Check if a rule exists with that name
 */
struct rule *get_rule(const char* name) {
        for (GSList *iter = rules; iter; iter = iter->next) {
                struct rule *r = iter->data;
                if (r->name && STR_EQ(r->name, name))
                        return r;
        }
        return NULL;
}

/**
 * see rules.h
 */
bool rule_offset_is_modifying(const size_t offset) {
        const size_t first_action = offsetof(struct rule, timeout);
        const size_t last_action = offsetof(struct rule, set_stack_tag);
        return (offset >= first_action) && (offset <= last_action);
}

/**
 * see rules.h
 */
bool rule_offset_is_filter(const size_t offset) {
        const size_t first_filter = offsetof(struct rule, appname);
        return (offset >= first_filter) && !rule_offset_is_modifying(offset);
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

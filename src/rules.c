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

#define APPLY(nprop, rprop, defval) \
        do { \
                if (n->original->rprop == (defval)) n->original->rprop = n->nprop; \
                n->nprop = r->rprop; \
        } while (0)

/*
 * Apply rule to notification.
 */
void rule_apply(struct rule *r, struct notification *n)
{
        notification_keep_original(n);

        if (r->timeout != -1)
                APPLY(timeout, timeout, -1);
        if (r->override_dbus_timeout != -1)
                APPLY(dbus_timeout, override_dbus_timeout, -1);
        if (r->urgency != URG_NONE)
                APPLY(urgency, urgency, URG_NONE);
        if (r->fullscreen != FS_NULL)
                APPLY(fullscreen, fullscreen, FS_NULL);
        if (r->history_ignore != -1)
                APPLY(history_ignore, history_ignore, -1);
        if (r->set_transient != -1)
                APPLY(transient, set_transient, -1);
        if (r->skip_display != -1)
                APPLY(skip_display, skip_display, -1);
        if (r->word_wrap != -1)
                APPLY(word_wrap, word_wrap, -1);
        if (r->ellipsize != -1)
                APPLY(ellipsize, ellipsize, -1);
        if (r->alignment != -1)
                APPLY(alignment, alignment, -1);
        if (r->hide_text != -1)
                APPLY(hide_text, hide_text, -1);
        if (r->progress_bar_alignment != -1)
                APPLY(progress_bar_alignment, progress_bar_alignment, -1);
        if (r->min_icon_size != -1)
                APPLY(min_icon_size, min_icon_size, -1);
        if (r->max_icon_size != -1)
                APPLY(max_icon_size, max_icon_size, -1);
        if (r->markup != MARKUP_NULL)
                APPLY(markup, markup, MARKUP_NULL);
        if (r->icon_position != -1)
                APPLY(icon_position, icon_position, -1);
        if (COLOR_VALID(r->fg)) {
                if (!COLOR_VALID(n->original->fg)) n->original->fg = n->colors.fg;
                n->colors.fg = r->fg;
        }
        if (COLOR_VALID(r->bg)) {
                if (!COLOR_VALID(n->original->bg)) n->original->bg = n->colors.bg;
                n->colors.bg = r->bg;
        }
        if (COLOR_VALID(r->highlight)) {
                if (!COLOR_VALID(n->original->highlight)) n->original->highlight = n->colors.highlight;
                n->colors.highlight = r->highlight;
        }
        if (COLOR_VALID(r->fc)) {
                if (!COLOR_VALID(n->original->fc)) n->original->fc = n->colors.frame;
                n->colors.frame = r->fc;
        }
        if (r->format)
                APPLY(format, format, NULL);
        if (r->action_name) {
                if (n->original->action_name)
                        g_free(n->default_action_name);
                else
                        n->original->action_name = n->default_action_name;

                n->default_action_name = g_strdup(r->action_name);
        }
        if (r->set_category) {
                if (n->original->set_category)
                        g_free(n->category);
                else
                        n->original->set_category = n->category;

                n->category = g_strdup(r->set_category);
        }
        if (r->default_icon) {
                if (n->original->default_icon)
                        g_free(n->default_icon_name);
                else
                        n->original->default_icon = n->default_icon_name;

                n->default_icon_name = g_strdup(r->default_icon);
        }
        if (r->set_stack_tag) {
                if (n->original->set_stack_tag)
                        g_free(n->stack_tag);
                else
                        n->original->set_stack_tag = n->stack_tag;

                n->stack_tag = g_strdup(r->set_stack_tag);
        }
        if (r->new_icon) {
                if (!n->original->new_icon)
                        n->original->new_icon = g_strdup(n->iconname);

                // FIXME This is not efficient when the icon is replaced
                // multiple times for the same notification. To fix this, a
                // separate variable is needed to track if the icon is
                // replaced, like in 86cbc1d34bb0f551461dbd466cd9e4860ae01817.
                notification_icon_replace_path(n, r->new_icon);
                n->receiving_raw_icon = false;
        }
        if (r->script) {
                n->scripts = g_renew(char *, n->scripts, n->script_count + 2);
                n->scripts[n->script_count] = g_strdup(r->script);
                n->scripts[n->script_count + 1] = NULL;
                n->script_count++;
        }
        if (r->override_pause_level != -1)
                APPLY(override_pause_level, override_pause_level, -1);
}

void rule_print(const struct rule *r)
{
        printf("{\n");
        printf("\tname: '%s'\n", STR_NN(r->name));
        printf("\tenabled: %d\n", r->enabled);

        // filters
        if (r->appname != NULL) printf("\tappname: '%s'\n", r->appname);
        if (r->summary != NULL) printf("\tsummary: '%s'\n", r->summary);
        if (r->body != NULL) printf("\tbody: '%s'\n", r->body);
        if (r->icon != NULL) printf("\ticon: '%s'\n", r->icon);
        if (r->category != NULL) printf("\tcategory: '%s'\n", r->category);
        if (r->msg_urgency != URG_NONE) printf("\tmsg_urgency: '%s'\n", notification_urgency_to_string(r->msg_urgency));
        if (r->stack_tag != NULL) printf("\tstack_tag: '%s'\n", r->stack_tag);
        if (r->desktop_entry != NULL) printf("\tdesktop_entry: '%s'\n", r->desktop_entry);
        if (r->match_dbus_timeout != -1) printf("\tmatch_dbus_timeout: %ld\n", r->match_dbus_timeout);
        if (r->match_transient != -1) printf("\tmatch_transient: %d\n", r->match_transient);

        // modifiers
        if (r->timeout != -1) printf("\ttimeout: %ld\n", r->timeout);
        if (r->override_dbus_timeout != -1) printf("\toverride_dbus_timeout: %ld\n", r->override_dbus_timeout);
        if (r->markup != -1) printf("\tmarkup: %d\n", r->markup);
        if (r->action_name != NULL) printf("\taction_name: '%s'\n", r->action_name);
        if (r->urgency != URG_NONE) printf("\turgency: '%s'\n", notification_urgency_to_string(r->urgency));
        if (r->history_ignore != -1) printf("\thistory_ignore: %d\n", r->history_ignore);
        if (r->set_transient != -1) printf("\tset_transient: %d\n", r->set_transient);
        if (r->skip_display != -1) printf("\tskip_display: %d\n", r->skip_display);
        if (r->word_wrap != -1) printf("\tword_wrap: %d\n", r->word_wrap);
        if (r->ellipsize != -1) printf("\tellipsize: %d\n", r->ellipsize);
        if (r->alignment != -1) printf("\talignment: %d\n", r->alignment);
        if (r->hide_text != -1) printf("\thide_text: %d\n", r->hide_text);
        if (r->icon_position != -1) printf("\ticon_position: %d\n", r->icon_position);
        if (r->min_icon_size != -1) printf("\tmin_icon_size: %d\n", r->min_icon_size);
        if (r->max_icon_size != -1) printf("\tmax_icon_size: %d\n", r->max_icon_size);
        if (r->override_pause_level != -1) printf("\toverride_pause_level: %d\n", r->override_pause_level);
        if (r->new_icon != NULL) printf("\tnew_icon: '%s'\n", r->new_icon);
        if (r->default_icon != NULL) printf("\tdefault_icon: '%s'\n", r->default_icon);

        char buf[10];
        if (COLOR_VALID(r->fg)) printf("\tfg: %s\n", color_to_string(r->fg, buf));
        if (COLOR_VALID(r->bg)) printf("\tbg: %s\n", color_to_string(r->bg, buf));
        if (COLOR_VALID(r->highlight)) printf("\thighlight: %s\n", color_to_string(r->highlight, buf));
        if (COLOR_VALID(r->fc)) printf("\tframe: %s\n", color_to_string(r->fc, buf));
        if (r->set_category != NULL) printf("\tset_category: '%s'\n", r->set_category);
        if (r->format != NULL) printf("\tformat: '%s'\n", r->format);
        if (r->script != NULL) printf("\tscript: '%s'\n", r->script);
        if (r->fullscreen != FS_NULL) printf("\tfullscreen: %s\n", enum_to_string_fullscreen(r->fullscreen));
        if (r->progress_bar_alignment != -1) printf("\tprogress_bar_alignment: %d\n", r->progress_bar_alignment);
        if (r->set_stack_tag != NULL) printf("\tset_stack_tag: %s\n", r->set_stack_tag);
        printf("}\n");
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
                        char *err_buf = g_malloc(err_size);
                        regerror(err, &regex, err_buf, err_size);
                        LOG_W("%s: \"%s\"", err_buf, pattern);
                        g_free(err_buf);
                        return false;
                }

                while (true) {
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
                && (r->match_dbus_timeout < 0 || (r->match_dbus_timeout == n->dbus_timeout))
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

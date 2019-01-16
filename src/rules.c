/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "rules.h"

#include <fnmatch.h>
#include <glib.h>

#include "dunst.h"

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
        if (r->markup != MARKUP_NULL)
                n->markup = r->markup;
        if (r->new_icon)
                notification_icon_replace_path(n, r->new_icon);
        if (r->fg) {
                g_free(n->colors.fg);
                n->colors.fg = g_strdup(r->fg);
        }
        if (r->bg) {
                g_free(n->colors.bg);
                n->colors.bg = g_strdup(r->bg);
        }
        if (r->fc) {
                g_free(n->colors.frame);
                n->colors.frame = g_strdup(r->fc);
        }
        if (r->format)
                n->format = r->format;
        if (r->script)
                n->script = r->script;
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

struct rule *rule_new(void)
{
        struct rule *r = g_malloc0(sizeof(struct rule));

        r->msg_urgency = URG_NONE;
        r->timeout = -1;
        r->urgency = URG_NONE;
        r->fullscreen = FS_NULL;
        r->markup = MARKUP_NULL;
        r->history_ignore = false;
        r->match_transient = -1;
        r->set_transient = -1;
        r->skip_display = -1;

        return r;
}

static inline bool rule_field_matches_string(const char *value, const char *pattern)
{
        return !pattern || (value && !fnmatch(pattern, value, 0));
}

/*
 * Check whether rule should be applied to n.
 */
bool rule_matches_notification(struct rule *r, struct notification *n)
{
        return     (r->msg_urgency == URG_NONE || r->msg_urgency == n->urgency)
                && (r->match_transient == -1 || (r->match_transient == n->transient))
                && rule_field_matches_string(n->appname,        r->appname)
                && rule_field_matches_string(n->desktop_entry,  r->desktop_entry)
                && rule_field_matches_string(n->summary,        r->summary)
                && rule_field_matches_string(n->body,           r->body)
                && rule_field_matches_string(n->iconname,       r->icon)
                && rule_field_matches_string(n->category,       r->category)
                && rule_field_matches_string(n->stack_tag,      r->stack_tag);
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

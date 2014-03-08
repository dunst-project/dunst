/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include <glib.h>
#include <fnmatch.h>

#include "dunst.h"
#include "rules.h"

/*
 * Apply rule to notification.
 */
void rule_apply(rule_t * r, notification * n)
{
        if (r->timeout != -1)
                n->timeout = r->timeout;
        if (r->urgency != -1)
                n->urgency = r->urgency;
        if (r->new_icon)
                n->icon = r->new_icon;
        if (r->fg)
                n->color_strings[ColFG] = r->fg;
        if (r->bg)
                n->color_strings[ColBG] = r->bg;
        if (r->format)
                n->format = r->format;
        if (r->script)
                n->script = r->script;
}

/*
 * Check all rules if they match n and apply.
 */
void rule_apply_all(notification * n)
{
        for (GSList * iter = rules; iter; iter = iter->next) {
                rule_t *r = iter->data;
                if (rule_matches_notification(r, n)) {
                        rule_apply(r, n);
                }
        }
}

/*
 * Initialize rule with default values.
 */
void rule_init(rule_t * r)
{
        r->name = NULL;
        r->appname = NULL;
        r->summary = NULL;
        r->body = NULL;
        r->icon = NULL;
        r->category = NULL;
        r->msg_urgency = -1;
        r->timeout = -1;
        r->urgency = -1;
        r->new_icon = NULL;
        r->fg = NULL;
        r->bg = NULL;
        r->format = NULL;
}

/*
 * Check whether rule should be applied to n.
 */
bool rule_matches_notification(rule_t * r, notification * n)
{

        return ((!r->appname || !fnmatch(r->appname, n->appname, 0))
                && (!r->summary || !fnmatch(r->summary, n->summary, 0))
                && (!r->body || !fnmatch(r->body, n->body, 0))
                && (!r->icon || !fnmatch(r->icon, n->icon, 0))
                && (!r->category || !fnmatch(r->category, n->category, 0))
                && (r->msg_urgency == -1 || r->msg_urgency == n->urgency));
}
/* vim: set ts=8 sw=8 tw=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "rules.h"

#include <fnmatch.h>
#include <glib.h>

#include "dunst.h"

/*
 * Apply rule to notification.
 */
void rule_apply(rule_t *r, notification *n)
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
        if (r->markup != MARKUP_NULL)
                n->markup = r->markup;
        if (r->new_icon) {
                g_free(n->icon);
                n->icon = g_strdup(r->new_icon);
                rawimage_free(n->raw_icon);
                n->raw_icon = NULL;
        }
        if (r->fg) {
                g_free(n->colors[ColFG]);
                n->colors[ColFG] = g_strdup(r->fg);
        }
        if (r->bg) {
                g_free(n->colors[ColBG]);
                n->colors[ColBG] = g_strdup(r->bg);
        }
        if (r->fc) {
                g_free(n->colors[ColFrame]);
                n->colors[ColFrame] = g_strdup(r->fc);
        }
        if (r->format)
                n->format = r->format;
        if (r->script)
                n->script = r->script;
}

/*
 * Check all rules if they match n and apply.
 */
void rule_apply_all(notification *n)
{
        for (GSList *iter = rules; iter; iter = iter->next) {
                rule_t *r = iter->data;
                if (rule_matches_notification(r, n)) {
                        rule_apply(r, n);
                }
        }
}

/*
 * Initialize rule with default values.
 */
void rule_init(rule_t *r)
{
        r->name = NULL;
        r->appname = NULL;
        r->summary = NULL;
        r->body = NULL;
        r->icon = NULL;
        r->category = NULL;
        r->msg_urgency = URG_NONE;
        r->timeout = -1;
        r->urgency = URG_NONE;
        r->fullscreen = FS_NULL;
        r->markup = MARKUP_NULL;
        r->new_icon = NULL;
        r->history_ignore = false;
        r->match_transient = -1;
        r->set_transient = -1;
        r->fg = NULL;
        r->bg = NULL;
        r->fc = NULL;
        r->format = NULL;
}

/*
 * Check whether rule should be applied to n.
 */
bool rule_matches_notification(rule_t *r, notification *n)
{
        return ((!r->appname || !fnmatch(r->appname, n->appname, 0))
                && (!r->summary || !fnmatch(r->summary, n->summary, 0))
                && (!r->body || !fnmatch(r->body, n->body, 0))
                && (!r->icon || !fnmatch(r->icon, n->icon, 0))
                && (!r->category || !fnmatch(r->category, n->category, 0))
                && (r->match_transient == -1 || (r->match_transient == n->transient))
                && (r->msg_urgency == URG_NONE || r->msg_urgency == n->urgency));
}
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

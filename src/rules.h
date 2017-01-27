/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_RULES_H
#define DUNST_RULES_H

#include <glib.h>

#include "dunst.h"
#include "notification.h"

typedef struct _rule_t {
        char *name;
        /* filters */
        char *appname;
        char *summary;
        char *body;
        char *icon;
        char *category;
        int msg_urgency;

        /* actions */
        int timeout;
        int urgency;
        int allow_markup;
        int plain_text;
        char *new_icon;
        char *fg;
        char *bg;
        const char *format;
        const char *script;
} rule_t;

extern GSList *rules;

void rule_init(rule_t * r);
void rule_apply(rule_t * r, notification * n);
void rule_apply_all(notification * n);
bool rule_matches_notification(rule_t * r, notification * n);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

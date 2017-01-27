/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_MENU_H
#define DUNST_MENU_H

#include "dunst.h"
#include <regex.h>

char *extract_urls(const char *to_match);
void open_browser(const char *url);
void invoke_action(const char *action);
void regex_teardown(void);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

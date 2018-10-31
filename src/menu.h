/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_MENU_H
#define DUNST_MENU_H

char *extract_urls(const char *to_match);
void open_browser(const char *in);
void invoke_action(const char *action);
void regex_teardown(void);

/**
 * Open the context menu that lets the user select urls/actions/etc.
 */
void context_menu(void);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

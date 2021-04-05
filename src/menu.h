/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_MENU_H
#define DUNST_MENU_H

#include <glib.h>

/**
 * Extract all urls from the given string.
 *
 * @param to_match (nullable) String to extract URLs
 * @return a string of urls separated by '\n'
 * @retval NULL: No URLs found
 */
char *extract_urls(const char *to_match);

void open_browser(const char *in);
void invoke_action(const char *action);
void regex_teardown(void);

/**
 * Open the context menu that lets the user select urls/actions/etc for all displayed notifications.
 */
void context_menu(void);

/**
 * Open the context menu that lets the user select urls/actions/etc for the specified notifications.
 * @param notifications (nullable) List of notifications for which the context menu should be opened
 */
void context_menu_for(GList *notifications);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

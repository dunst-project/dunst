/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_MENU_H
#define DUNST_MENU_H

char *extract_urls(const char *to_match);
void open_browser(const char *in);
void invoke_action(const char *action);
void regex_teardown(void);

/** Split the command into its shell equivalent parts
 *
 * @return a NULL terminated string vector containing
 *         the command split into its arguments
 *         with additional non-splitted arguments,
 *         which got passed as the variadic arguments
 */
char **split_into_cmd(const char *command, ...);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

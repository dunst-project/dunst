/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_MENU_H
#define DUNST_MENU_H

#include "notification.h"
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

struct menu {
        char *key;
        char *value;
        int height;
        int width;
        int x;
        int y;
};

/**
 * Initialize the menu structure for a given notification.
 * This function creates a new array of menu items based on the actions.
 * @param n The notification for which to initialize the menu
 * @return TRUE if the initialization was successful, FALSE otherwise
 */
gboolean menu_init(struct notification *n);

/**
 * Get the label (display text) for a menu item at a specific index.
 * @param n The notification containing the menu
 * @param index The index of the menu item
 * @return The label of the menu item at the specified index
 */
char *menu_get_label(struct notification *n, int index);

/**
 * Get the number of menu items for a given notification.
 * @param n The notification containing the menu
 * @return The number of menu items
 */
int menu_get_count(struct notification *n);

/**
 * Set the position and size of a menu item for a given notification.
 * @param n The notification containing the menu
 * @param index The index of the menu item to update
 * @param x The x-coordinate of the menu item
 * @param y The y-coordinate of the menu item
 * @param width The width of the menu item
 * @param height The height of the menu item
 */
void menu_set_position(struct notification *n, int index, int x, int y,
                       int width, int height);

/**
 * Get the menu item at a specific coordinate.
 * @param n The notification containing the menu
 * @param x The x-coordinate to check
 * @param y The y-coordinate to check
 * @return A pointer to the menu item at the specified coordinates,
 * or NULL if not found.
 */
struct menu *menu_get_at(struct notification *n, int x, int y);

/**
 * Free the memory allocated for the menu array in a notification.
 * @param n The notification containing the menu array to be freed.
 */
void menu_free_array(struct notification *n);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

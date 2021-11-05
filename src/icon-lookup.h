#ifndef DUNST_ICON_LOOKUP_H
#define DUNST_ICON_LOOKUP_H

struct icon_theme {
        char *name;
        char *location; // full path to the theme
        char *subdir_theme; // name of the directory in which the theme is located

        int inherits_count;
        int *inherits_index;

        int dirs_count;
        struct icon_theme_dir *dirs;
};

enum theme_dir_type { THEME_DIR_FIXED, THEME_DIR_SCALABLE, THEME_DIR_THRESHOLD };

struct icon_theme_dir {
        char *name;
        int size;
        int scale;
        int min_size, max_size;
        int threshold;
        enum theme_dir_type type;
};


/**
 * Load a theme with given name from a standard icon directory. Don't call this
 * function if the theme is already loaded.
 *
 * @param name Name of the directory in which the theme is located. Note that
 *             it is NOT the name of the theme as specified in index.theme.
 * @returns The index of the theme, which can be used to set it as default.
 * @retval -1 if the icon theme cannot be loaded.
 */
int load_icon_theme(char *name);


/**
 * Add theme to the list of default themes. The theme that's added first will
 * be used first for lookup. After that the inherited themes will be used and
 * only after that the next default theme will be used.
 *
 * @param theme_index The index of the theme as returned by #load_icon_theme
 */
void add_default_theme(int theme_index);

/**
 * Find icon of specified size in selected theme. This function will not return
 * icons that cannot be scaled to \p size according to index.theme.
 *
 * @param name         Name of the icon or full path to it.
 * @param theme_index  Index of the theme to use.
 * @param size         Size of the icon.
 * @returns The full path to the icon.
 * @retval NULL if the icon cannot be found or is not readable.
 */
char *find_icon_in_theme(const char *name, int theme_index, int size);
char *find_icon_path(const char *name, int size);
void set_default_theme(int theme_index);

/**
 * Find icon of specified size in the default theme or an inherited theme. This
 * function will not return icons that cannot be scaled to \p size according to
 * index.theme.

 *
 * @param name Name of the icon or full path to it.
 * @param size Size of the icon.
 * @returns The full path to the icon.
 * @retval NULL if the icon cannot be found or is not readable.
 */
char *find_icon_path(const char *name, int size);

/**
 * Free all icon themes.
 */
void free_all_themes();

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

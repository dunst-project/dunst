#ifndef DUNST_ICON_LOOKUP_H
#define DUNST_ICON_LOOKUP_H

struct icon_theme {
        char *name;
        char *location; // full path to the theme
        char *subdir_theme; // name of the directory in which the theme is located
        int inherits_count;

        // FIXME don't use a pointer to the theme,
        // since it may be invalidated after realloc
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


int load_icon_theme(char *name);
char *find_icon_in_theme(const char *name, int theme_index, int size);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

#define _GNU_SOURCE
#include "icon-lookup.h"

#include <glib.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <assert.h>

#include "ini.h"
#include "utils.h"
#include "log.h"

struct icon_theme *icon_themes = NULL;
int icon_themes_count = 0;
int *default_themes_index = NULL;
int default_themes_count = 0;

int get_icon_theme(char *name) {
        for (int i = 0; i < icon_themes_count; i++) {
                if (STR_EQ(icon_themes[i].subdir_theme, name)){
                        return i;
                }
        }
        return -1;
}

/**
 * Load a theme from a directory. Don't call this function if the theme is
 * already loaded. It also loads the inherited themes. If there are no
 * inherited themes, the theme "hicolor" is inherited.
 *
 * If it succeeds loading the theme, it adds theme to the list "icon_themes".
 *
 * @param icon_dir A directory where icon themes are stored
 * @param subdir_theme The subdirectory in which the theme is located
 *
 * @returns the index to the theme that was loaded
 * @retval -1 means no index was found
 */
int load_icon_theme_from_dir(const char *icon_dir, const char *subdir_theme) {
        LOG_D("Loading theme %s/%s\n", icon_dir, subdir_theme);
        char *theme_index_dir = g_build_filename(icon_dir, subdir_theme, "index.theme", NULL);
        FILE *theme_index = fopen(theme_index_dir, "r");
        g_free(theme_index_dir);
        if (!theme_index)
                return -1;

        struct ini *ini = load_ini_file(theme_index);
        fclose(theme_index);
        if (ini->section_count == 0) {
                finish_ini(ini);
                free(ini);
                return -1;
        }

        icon_themes_count++;
        icon_themes = realloc(icon_themes, icon_themes_count * sizeof(struct icon_theme));
        int index = icon_themes_count - 1;
        icon_themes[index].name = g_strdup(section_get_value(ini, &ini->sections[0], "Name"));
        icon_themes[index].location = g_strdup(icon_dir);
        icon_themes[index].subdir_theme = g_strdup(subdir_theme);
        icon_themes[index].inherits_index = NULL;
        icon_themes[index].inherits_count = 0;

        // load theme directories
        icon_themes[index].dirs_count = ini->section_count - 1;
        icon_themes[index].dirs = calloc(icon_themes[index].dirs_count, sizeof(struct icon_theme_dir));

        for (int i = 0; i < icon_themes[index].dirs_count; i++) {
                struct section section = ini->sections[i+1];
                icon_themes[index].dirs[i].name = g_strdup(section.name);

                // read size
                const char *size_str = section_get_value(ini, &section, "Size");
                safe_string_to_int(&icon_themes[index].dirs[i].size, size_str);

                // read optional scale, defaulting to 1
                const char *scale_str = section_get_value(ini, &section, "Scale");
                icon_themes[index].dirs[i].scale = 1;
                if (scale_str){
                        safe_string_to_int(&icon_themes[index].dirs[i].scale, scale_str);
                }

                // read type
                const char *type = section_get_value(ini, &section, "Type");
                if (STR_EQ(type, "Fixed")) {
                        icon_themes[index].dirs[i].type = THEME_DIR_FIXED;
                } else if (STR_EQ(type, "Scalable")) {
                        icon_themes[index].dirs[i].type = THEME_DIR_SCALABLE;
                } else if (STR_EQ(type, "Threshold")) {
                        icon_themes[index].dirs[i].type = THEME_DIR_THRESHOLD;
                } else {
                        // default to type threshold
                        icon_themes[index].dirs[i].type = THEME_DIR_THRESHOLD;
                }

                // read type-specific data
                if (icon_themes[index].dirs[i].type == THEME_DIR_SCALABLE) {
                        const char *min_size = section_get_value(ini, &section, "MinSize");
                        if (min_size)
                                safe_string_to_int(&icon_themes[index].dirs[i].min_size, min_size);
                        else
                                icon_themes[index].dirs[i].min_size = icon_themes[index].dirs[i].size;

                        const char *max_size = section_get_value(ini, &section, "MaxSize");
                        if (max_size)
                                safe_string_to_int(&icon_themes[index].dirs[i].max_size, max_size);
                        else
                                icon_themes[index].dirs[i].max_size = icon_themes[index].dirs[i].size;

                } else if (icon_themes[index].dirs[i].type == THEME_DIR_THRESHOLD) {
                        icon_themes[index].dirs[i].threshold = 2;
                        const char *threshold = section_get_value(ini, &section, "Threshold");
                        if (threshold){
                                safe_string_to_int(&icon_themes[index].dirs[i].threshold, threshold);
                        }
                }
        }


        // load inherited themes
        if (!STR_EQ(icon_themes[index].name, "Hicolor"))
        {
                char **inherits = string_to_array(get_value(ini, "Icon Theme", "Inherits"), ",");
                icon_themes[index].inherits_count = string_array_length(inherits);
                LOG_D("Theme has %i inherited themes\n", icon_themes[index].inherits_count);
                if (icon_themes[index].inherits_count <= 0) {
                        // set fallback theme to hicolor if there are no inherits
                        g_strfreev(inherits);
                        inherits = calloc(2, sizeof(char*));
                        inherits[0] = g_strdup("hicolor");
                        inherits[1] = NULL;
                        icon_themes[index].inherits_count = 1;
                }

                icon_themes[index].inherits_index = calloc(icon_themes[index].inherits_count, sizeof(int));

                for (int i = 0; inherits[i] != NULL; i++) {
                        LOG_D("inherits: %s\n", inherits[i]);
                        icon_themes[index].inherits_index[i] = get_icon_theme(inherits[i]);
                        if (icon_themes[index].inherits_index[i] == -1) {
                                LOG_D("Loading inherited theme\n");
                                // FIXME don't use a pointer to the theme,
                                // since it may be invalidated after realloc. Use an index instead
                                icon_themes[index].inherits_index[i] = load_icon_theme(inherits[i]);
                        }
                }
                g_strfreev(inherits);
        }



        finish_ini(ini);
        free(ini);
        return index;
}

// a list of directories where icon themes might be located
GPtrArray *theme_path = NULL;

void get_theme_path() {
        theme_path = g_ptr_array_new_full(5, g_free);
        const char *home = g_get_home_dir();
        g_ptr_array_add(theme_path, g_build_filename(home, ".icons", NULL));

        char *data_home_default = g_build_filename(home, ".local", "share", NULL);
        add_paths_from_env(theme_path, "XDG_DATA_HOME", "icons", data_home_default);
        g_free(data_home_default);

        add_paths_from_env(theme_path, "XDG_DATA_DIRS", "icons", "/usr/local/share/:/usr/share/");
        g_ptr_array_add(theme_path, g_strdup("/usr/share/pixmaps"));
        for (int i = 0; i < theme_path->len; i++) {
                LOG_D("Theme locations: %s\n", (char*)theme_path->pdata[i]);
        }
}

// see icon-lookup.h
int load_icon_theme(char *name) {
        if(!theme_path) {
                get_theme_path();
        }

        for (int i = 0; i < theme_path->len; i++) {
                int theme_index = load_icon_theme_from_dir(theme_path->pdata[i], name);
                if (theme_index != -1)
                        return theme_index;
        }

        LOG_W("Could not find theme %s", name);
        return -1;
}

void finish_icon_theme_dir(struct icon_theme_dir *dir) {
        if (!dir)
                return;
        free(dir->name);
}

void finish_icon_theme(struct icon_theme *theme) {
        if (!theme)
                return;
        for (int i = 0; i < theme->dirs_count; i++) {
                finish_icon_theme_dir(&theme->dirs[i]);
        }
        free(theme->name);
        free(theme->location);
        free(theme->subdir_theme);
        free(theme->inherits_index);
        free(theme->dirs);
}

void free_all_themes() {
        free(default_themes_index);
        default_themes_index = NULL;
        default_themes_count = 0;
        LOG_D("Finishing %i themes\n", icon_themes_count);
        for (int i = 0; i < icon_themes_count; i++) {
                finish_icon_theme(&icon_themes[i]);
        }
        free(icon_themes);
        icon_themes_count = 0;
        icon_themes = NULL;
        g_ptr_array_unref(theme_path);
        theme_path = NULL;
}

// see icon-lookup.h
void add_default_theme(int theme_index) {
        if (theme_index < 0) {
                LOG_W("Invalid theme index: %i", theme_index);
                return;
        }
        if (theme_index >= icon_themes_count) {
                LOG_W("Invalid theme index: %i. Theme does not exists.",
                                theme_index);
                return;
        }
        default_themes_count++;
        default_themes_index = realloc(default_themes_index,
                        default_themes_count * sizeof(int));
        default_themes_index[default_themes_count - 1] = theme_index;
}

// see icon-lookup.h
char *find_icon_in_theme(const char *name, int theme_index, int size) {
        struct icon_theme *theme = &icon_themes[theme_index];
        LOG_D("Finding icon %s in theme %s\n", name, theme->name);
        for (int i = 0; i < theme->dirs_count; i++) {
                bool match_size = false;
                struct icon_theme_dir dir = theme->dirs[i];
                switch (dir.type) {
                        case THEME_DIR_FIXED:
                                match_size = dir.size == size;
                                break;

                        case THEME_DIR_SCALABLE:
                                match_size = dir.min_size <= size && dir.max_size >= size;
                                break;

                        case THEME_DIR_THRESHOLD:
                                match_size = (float)dir.size / dir.threshold <= size
                                        && dir.size * dir.threshold >= size;
                                break;
                }
                if (match_size) {
                        const char *suffixes[] = { ".svg", ".svgz", ".png", ".xpm", NULL };
                        for (const char **suf = suffixes; *suf; suf++) {
                                char *name_with_extension = g_strconcat(name, *suf, NULL);
                                char *icon = g_build_filename(theme->location, theme->subdir_theme,
                                                dir.name, name_with_extension,
                                                NULL);
                                if (is_readable_file(icon)) {
                                        g_free(name_with_extension);
                                        return icon;
                                }
                                g_free(name_with_extension);
                                g_free(icon);
                        }
                }
        }
        return NULL;
}

char *find_icon_in_theme_with_inherit(const char *name, int theme_index, int size) {
        char *icon = find_icon_in_theme(name, theme_index, size);
        if (icon)
                return icon;

        for (int i = 0; i < icon_themes[theme_index].inherits_count; i++) {
                if (icon_themes[theme_index].inherits_index[i] <= 0)
                        continue; // inherited theme could not be found
                icon = find_icon_in_theme(name,
                                icon_themes[theme_index].inherits_index[i],
                                size);
                if (icon)
                        return icon;
        }
        return NULL;
}

/* see icon-lookup.h */
char *find_icon_path(const char *name, int size) {
        if (STR_EMPTY(name))
                return NULL;

        gchar *uri_path = NULL;

        if (g_str_has_prefix(name, "file://")) {
                uri_path = g_filename_from_uri(name, NULL, NULL);
                if (is_readable_file(uri_path))
                        return uri_path;
                else
                        return NULL;
        }

        /* absolute path? */
        if (name[0] == '/' || name[0] == '~') {
                if (is_readable_file(name))
                        return g_strdup(name);
                else
                        return NULL;
        }

        if (!default_themes_index) {
                LOG_W("No icon theme has been set.\n");
                return NULL;
        }
        for (int i = 0; i < default_themes_count; i++) {
                char *icon = find_icon_in_theme_with_inherit(name,
                                default_themes_index[i], size);
                if (icon)
                        return icon;

        }
        return NULL;
}

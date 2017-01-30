/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#define _GNU_SOURCE
#include "option_parser.h"

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

typedef struct _entry_t {
        char *key;
        char *value;
} entry_t;

typedef struct _section_t {
        char *name;
        int entry_count;
        entry_t *entries;
} section_t;

static int section_count = 0;
static section_t *sections;

static section_t *new_section(char *name);
static section_t *get_section(char *name);
static void add_entry(char *section_name, char *key, char *value);
static char *get_value(char *section, char *key);
static char *clean_value(char *value);

static int cmdline_argc;
static char **cmdline_argv;

static char *usage_str = NULL;
static void cmdline_usage_append(char *key, char *type, char *description);

static int cmdline_find_option(char *key);

section_t *new_section(char *name)
{
        for (int i = 0; i < section_count; i++) {
                if(!strcmp(name, sections[i].name)) {
                        die("Duplicated section in dunstrc detected.\n", -1);
                }
        }

        section_count++;
        sections = g_realloc(sections, sizeof(section_t) * section_count);
        if(sections == NULL) die("Unable to allocate memory.\n", 1);
        sections[section_count - 1].name = g_strdup(name);
        sections[section_count - 1].entries = NULL;
        sections[section_count - 1].entry_count = 0;
        return &sections[section_count - 1];
}

void free_ini(void)
{
        for (int i = 0; i < section_count; i++) {
                for (int j = 0; j < sections[i].entry_count; j++) {
                        g_free(sections[i].entries[j].key);
                        g_free(sections[i].entries[j].value);
                }
                g_free(sections[i].entries);
                g_free(sections[i].name);
        }
        g_free(sections);
        section_count = 0;
        sections = NULL;
}

section_t *get_section(char *name)
{
        for (int i = 0; i < section_count; i++) {
                if (strcmp(sections[i].name, name) == 0)
                        return &sections[i];
        }

        return NULL;
}

void add_entry(char *section_name, char *key, char *value)
{
        section_t *s = get_section(section_name);
        if (s == NULL) {
                s = new_section(section_name);
        }

        s->entry_count++;
        int len = s->entry_count;
        s->entries = g_realloc(s->entries, sizeof(entry_t) * len);
        s->entries[s->entry_count - 1].key = g_strdup(key);
        s->entries[s->entry_count - 1].value = clean_value(value);
}

char *get_value(char *section, char *key)
{
        section_t *s = get_section(section);
        if (!s) {
                return NULL;
        }

        for (int i = 0; i < s->entry_count; i++) {
                if (strcmp(s->entries[i].key, key) == 0) {
                        return s->entries[i].value;
                }
        }
        return NULL;
}

char *ini_get_string(char *section, char *key, const char *def)
{
        char *value = get_value(section, key);
        if (value)
                return g_strdup(value);

        return def ? g_strdup(def) : NULL;
}

int ini_get_int(char *section, char *key, int def)
{
        char *value = get_value(section, key);
        if (value == NULL)
                return def;
        else
                return atoi(value);
}

double ini_get_double(char *section, char *key, double def)
{
        char *value = get_value(section, key);
        if (value == NULL)
                return def;
        else
                return atof(value);
}

bool ini_is_set(char *ini_section, char *ini_key)
{
        return get_value(ini_section, ini_key) != NULL;
}

char *next_section(char *section)
{
        if (section_count == 0)
                return NULL;

        if (section == NULL) {
                return sections[0].name;
        }

        for (int i = 0; i < section_count; i++) {
                if (strcmp(section, sections[i].name) == 0) {
                        if (i + 1 >= section_count)
                                return NULL;
                        else
                                return sections[i + 1].name;
                }
        }
        return NULL;
}

int ini_get_bool(char *section, char *key, int def)
{
        char *value = get_value(section, key);
        if (value == NULL)
                return def;
        else {
                switch (value[0]) {
                case 'y':
                case 'Y':
                case 't':
                case 'T':
                case '1':
                        return true;
                case 'n':
                case 'N':
                case 'f':
                case 'F':
                case '0':
                        return false;
                default:
                        return def;
                }
        }
}

char *clean_value(char *value)
{
        char *s;

        if (value[0] == '"')
                s = g_strdup(value + 1);
        else
                s = g_strdup(value);

        if (s[strlen(s) - 1] == '"')
                s[strlen(s) - 1] = '\0';

        return s;

}

int load_ini_file(FILE * fp)
{
        if (!fp)
                return 1;

        char *line = NULL;
        size_t line_len = 0;

        int line_num = 0;
        char *current_section = NULL;
        while (getline(&line, &line_len, fp) != -1) {
                line_num++;

                char *start = g_strstrip(line);

                if (*start == ';' || *start == '#' || strlen(start) == 0)
                        continue;

                if (*start == '[') {
                        char *end = strchr(start + 1, ']');
                        if (!end) {
                                printf
                                    ("Warning: invalid config file at line %d\n",
                                     line_num);
                                printf("Missing ']'\n");
                                continue;
                        }

                        *end = '\0';

                        if (current_section)
                                g_free(current_section);
                        current_section = (g_strdup(start + 1));
                        new_section(current_section);
                        continue;
                }

                char *equal = strchr(start + 1, '=');
                if (!equal) {
                        printf("Warning: invalid config file at line %d\n",
                               line_num);
                        printf("Missing '='\n");
                        continue;
                }

                *equal = '\0';
                char *key = g_strstrip(start);
                char *value = g_strstrip(equal + 1);

                char *quote = strchr(value, '"');
                if (quote) {
                        char *closing_quote = strchr(quote + 1, '"');
                        if (!closing_quote) {
                                printf
                                    ("Warning: invalid config file at line %d\n",
                                     line_num);
                                printf("Missing '\"'\n");
                                continue;
                        }

                        closing_quote = '\0';
                } else {
                        char *comment = strpbrk(value, "#;");
                        if (comment)
                                comment = '\0';
                }
                value = g_strstrip(value);

                if (!current_section) {
                        printf("Warning: invalid config file at line: %d\n",
                               line_num);
                        printf("Key value pair without a section\n");
                        continue;
                }

                add_entry(current_section, key, value);
        }
        free(line);
        if (current_section)
                g_free(current_section);
        return 0;
}

void cmdline_load(int argc, char *argv[])
{
        cmdline_argc = argc;
        cmdline_argv = argv;
}

int cmdline_find_option(char *key)
{
        if (!key) {
                return -1;
        }
        char *key1 = g_strdup(key);
        char *key2 = strchr(key1, '/');

        if (key2) {
                *key2 = '\0';
                key2++;
        }

        /* look for first key */
        for (int i = 0; i < cmdline_argc; i++) {
                if (strcmp(key1, cmdline_argv[i]) == 0) {
                        g_free(key1);
                        return i;
                }
        }

        /* look for second key if one was specified */
        if (key2) {
                for (int i = 0; i < cmdline_argc; i++) {
                        if (strcmp(key2, cmdline_argv[i]) == 0) {
                                g_free(key1);
                                return i;
                        }
                }
        }

        g_free(key1);
        return -1;
}

static char *cmdline_get_value(char *key)
{
        int idx = cmdline_find_option(key);
        if (idx < 0) {
                return NULL;
        }

        if (idx + 1 >= cmdline_argc) {
                /* the argument is missing */
                fprintf(stderr, "Warning: %s, missing argument. Ignoring\n",
                        key);
                return NULL;
        }
        return cmdline_argv[idx + 1];
}

char *cmdline_get_string(char *key, const char *def, char *description)
{
        cmdline_usage_append(key, "string", description);
        char *str = cmdline_get_value(key);

        if (str)
                return g_strdup(str);
        if (def == NULL)
                return NULL;
        else
                return g_strdup(def);
}

int cmdline_get_int(char *key, int def, char *description)
{
        cmdline_usage_append(key, "double", description);
        char *str = cmdline_get_value(key);

        if (str == NULL)
                return def;
        else
                return atoi(str);
}

double cmdline_get_double(char *key, double def, char *description)
{
        cmdline_usage_append(key, "double", description);
        char *str = cmdline_get_value(key);
        if (str == NULL)
                return def;
        else
                return atof(str);
}

int cmdline_get_bool(char *key, int def, char *description)
{
        cmdline_usage_append(key, "", description);
        int idx = cmdline_find_option(key);
        if (idx > 0)
                return true;
        else
                return def;
}

bool cmdline_is_set(char *key)
{
        return cmdline_get_value(key) != NULL;
}

char *option_get_string(char *ini_section, char *ini_key, char *cmdline_key,
                        const char *def, char *description)
{
        char *val = NULL;

        if (cmdline_key) {
                val = cmdline_get_string(cmdline_key, NULL, description);
        }


        if (val) {
                return val;
        } else {
                return ini_get_string(ini_section, ini_key, def);
        }

}

int option_get_int(char *ini_section, char *ini_key, char *cmdline_key, int def,
                   char *description)
{
        /* *str is only used to check wether the cmdline option is actually set. */
        char *str = cmdline_get_value(cmdline_key);

        /* we call cmdline_get_int even when the option isn't set in order to
         * add the usage info */
        int val = cmdline_get_int(cmdline_key, def, description);

        if (!str)
                return ini_get_int(ini_section, ini_key, def);
        else
                return val;
}

double option_get_double(char *ini_section, char *ini_key, char *cmdline_key,
                         double def, char *description)
{
        char *str = cmdline_get_value(cmdline_key);
        double val = cmdline_get_double(cmdline_key, def, description);

        if (!str)
                return ini_get_double(ini_section, ini_key, def);
        else
                return val;
}

int option_get_bool(char *ini_section, char *ini_key, char *cmdline_key,
                    int def, char *description)
{
        int val = false;

        if (cmdline_key)
                val = cmdline_get_bool(cmdline_key, false, description);

        if (cmdline_key && val) {
                /* this can only be true if the value has been set,
                 * so we can return */
                return true;
        }

        return ini_get_bool(ini_section, ini_key, def);
}

void cmdline_usage_append(char *key, char *type, char *description)
{
        char *key_type;
        if (type && strlen(type) > 0)
                key_type = g_strdup_printf("%s (%s)", key, type);
        else
                key_type = g_strdup(key);

        if (!usage_str) {
                usage_str =
                    g_strdup_printf("%-40s - %s\n", key_type, description);
                g_free(key_type);
                return;
        }

        char *tmp;
        tmp =
            g_strdup_printf("%s%-40s - %s\n", usage_str, key_type, description);
        g_free(key_type);

        g_free(usage_str);
        usage_str = tmp;

}

char *cmdline_create_usage(void)
{
        return g_strdup(usage_str);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

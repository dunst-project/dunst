/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>

#include "options.h"
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





static int section_count;
static section_t *sections;

static section_t *new_section(char *name);
static section_t *get_section(char *name);
static void add_entry(char *section_name, char *key, char *value);
static char *get_value(char *section, char *key);
static char *clean_value(char *value);

section_t *new_section(char *name)
{
        section_count++;
        sections = realloc(sections, sizeof(section_t) *section_count);
        sections[section_count - 1].name = strdup(name);
        sections[section_count - 1].entries = NULL;
        sections[section_count - 1].entry_count = 0;
        return &sections[section_count -1];
}

void free_ini(void)
{
        for(int i = 0; i < section_count; i++) {
                for (int j = 0; j < sections[i].entry_count; j++) {
                        free(sections[i].entries[j].key);
                        free(sections[i].entries[j].value);
                }
                free(sections[i].entries);
        }
        free(sections);
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
        s->entries = realloc(s->entries, sizeof(entry_t) * len);
        s->entries[s->entry_count - 1].key = strdup(key);
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
        if (value == NULL)
                return def;
        else
                return strdup(value);
}

int ini_get_int(char *section, char *key, int def)
{
        char *value = get_value(section, key);
        if (value == NULL)
                return def;
        else
                return atoi(value);
}

double ini_get_double(char *section, char *key, int def)
{
        char *value = get_value(section, key);
        if (value == NULL)
                return def;
        else
                return atof(value);
}

char *next_section(char *section)
{
        if (section_count == 0)
                return NULL;

        for (int i = 0; i < section_count; i++) {
                if (strcmp(section, sections[i].name) == 0) {
                        if (i + 1 >= section_count)
                                return NULL;
                        else
                                return sections[i+1].name;
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
                        return false;
                }
        }
}

char *clean_value(char *value)
{
        char *s;

        if (value[0] == '"')
                s = strdup(value + 1);
        else
                s = strdup(value);

        if (s[strlen(s) - 1] == '"')
                s[strlen(s) - 1] = '\0';

        return s;

}

int load_ini_file(FILE *fp)
{
        char line[BUFSIZ];

        int line_num = 0;
        char *current_section = NULL;
        while (fgets(line, sizeof(line), fp) != NULL) {
                line_num++;

                char *start = lskip(rstrip(line));

                if (*start == ';' || *start == '#' || strlen(start) == 0)
                        continue;

                if (*start == '[') {
                        char *end = strstr(start + 1, "]");
                        if (!end) {
                                printf("Warning: invalid config file at line %d\n", line_num);
                                printf("Missing ']'\n");
                                continue;
                        }

                        *end = '\0';

                        if (current_section)
                                free(current_section);
                        current_section = (strdup(start+1));
                        new_section(strdup(current_section));
                        continue;
                }

                char *equal = strstr(start + 1, "=");
                if (!equal) {
                        printf("Warning: invalid config file at line %d\n", line_num);
                        printf("Missing '='\n");
                        continue;
                }

                *equal = '\0';
                char *key = rstrip(start);
                char *value = lskip(equal+1);

                char *quote = strstr(value, "\"");
                if (quote) {
                        char *closing_quote = strstr(quote + 1, "\"");
                        if (!closing_quote) {
                                printf("Warning: invalid config file at line %d\n", line_num);
                                printf("Missing '\"'\n");
                                continue;
                        }

                        closing_quote = '\0';
                } else {
                        char *comment = strstr(value, "#");
                        if (!comment)
                                comment = strstr(value, ";");
                        if (comment)
                                comment = '\0';
                }
                value = rstrip(value);

                if (!current_section) {
                        printf("Warning: invalid config file at line: %d\n", line_num);
                        printf("Key value pair without a section\n");
                        continue;
                }

                add_entry(current_section, key, value);
        }
        return 0;
}
/* vim: set ts=8 sw=8 tw=0: */

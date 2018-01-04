/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "option_parser.h"

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dunst.h"
#include "log.h"
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

static section_t *new_section(const char *name);
static section_t *get_section(const char *name);
static void add_entry(const char *section_name, const char *key, const char *value);
static const char *get_value(const char *section, const char *key);
static char *clean_value(const char *value);

static int cmdline_argc;
static char **cmdline_argv;

static char *usage_str = NULL;
static void cmdline_usage_append(const char *key, const char *type, const char *description);

static int cmdline_find_option(const char *key);

section_t *new_section(const char *name)
{
        for (int i = 0; i < section_count; i++) {
                if (!strcmp(name, sections[i].name)) {
                        DIE("Duplicated section in dunstrc detected.");
                }
        }

        section_count++;
        sections = g_realloc(sections, sizeof(section_t) * section_count);
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

section_t *get_section(const char *name)
{
        for (int i = 0; i < section_count; i++) {
                if (strcmp(sections[i].name, name) == 0)
                        return &sections[i];
        }

        return NULL;
}

void add_entry(const char *section_name, const char *key, const char *value)
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

const char *get_value(const char *section, const char *key)
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

const char *ini_get_string(const char *section, const char *key, const char *def)
{
        const char *value = get_value(section, key);
        return value ? value : def;
}

gint64 ini_get_time(const char *section, const char *key, gint64 def)
{
        const char *timestring = get_value(section, key);
        gint64 val = def;

        if (timestring) {
                val = string_to_time(timestring);
        }

        return val;
}

int ini_get_int(const char *section, const char *key, int def)
{
        const char *value = get_value(section, key);
        if (value == NULL)
                return def;
        else
                return atoi(value);
}

double ini_get_double(const char *section, const char *key, double def)
{
        const char *value = get_value(section, key);
        if (value == NULL)
                return def;
        else
                return atof(value);
}

bool ini_is_set(const char *ini_section, const char *ini_key)
{
        return get_value(ini_section, ini_key) != NULL;
}

const char *next_section(const char *section)
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

int ini_get_bool(const char *section, const char *key, int def)
{
        const char *value = get_value(section, key);
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

char *clean_value(const char *value)
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

int load_ini_file(FILE *fp)
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
                                LOG_W("Invalid config file at line %d: Missing ']'.", line_num);
                                continue;
                        }

                        *end = '\0';

                        g_free(current_section);
                        current_section = (g_strdup(start + 1));
                        new_section(current_section);
                        continue;
                }

                char *equal = strchr(start + 1, '=');
                if (!equal) {
                        LOG_W("Invalid config file at line %d: Missing '='.", line_num);
                        continue;
                }

                *equal = '\0';
                char *key = g_strstrip(start);
                char *value = g_strstrip(equal + 1);

                char *quote = strchr(value, '"');
                if (quote) {
                        char *closing_quote = strchr(quote + 1, '"');
                        if (!closing_quote) {
                                LOG_W("Invalid config file at line %d: Missing '\"'.", line_num);
                                continue;
                        }
                } else {
                        char *comment = strpbrk(value, "#;");
                        if (comment)
                                *comment = '\0';
                }
                value = g_strstrip(value);

                if (!current_section) {
                        LOG_W("Invalid config file at line %d: Key value pair without a section.", line_num);
                        continue;
                }

                add_entry(current_section, key, value);
        }
        free(line);
        g_free(current_section);
        return 0;
}

void cmdline_load(int argc, char *argv[])
{
        cmdline_argc = argc;
        cmdline_argv = argv;
}

int cmdline_find_option(const char *key)
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

static const char *cmdline_get_value(const char *key)
{
        int idx = cmdline_find_option(key);
        if (idx < 0) {
                return NULL;
        }

        if (idx + 1 >= cmdline_argc) {
                /* the argument is missing */
                LOG_W("%s: Missing argument. Ignoring.", key);
                return NULL;
        }
        return cmdline_argv[idx + 1];
}

const char *cmdline_get_string(const char *key, const char *def, const char *description)
{
        cmdline_usage_append(key, "string", description);
        const char *str = cmdline_get_value(key);

        if (str)
                return str;
        return def;
}

gint64 cmdline_get_time(const char *key, gint64 def, const char *description)
{
        cmdline_usage_append(key, "time", description);
        const char *timestring = cmdline_get_value(key);
        gint64 val = def;

        if (timestring) {
                val = string_to_time(timestring);
        }

        return val;
}

int cmdline_get_int(const char *key, int def, const char *description)
{
        cmdline_usage_append(key, "int", description);
        const char *str = cmdline_get_value(key);

        if (str == NULL)
                return def;
        else
                return atoi(str);
}

double cmdline_get_double(const char *key, double def, const char *description)
{
        cmdline_usage_append(key, "double", description);
        const char *str = cmdline_get_value(key);

        if (str == NULL)
                return def;
        else
                return atof(str);
}

int cmdline_get_bool(const char *key, int def, const char *description)
{
        cmdline_usage_append(key, "", description);
        int idx = cmdline_find_option(key);

        if (idx > 0)
                return true;
        else
                return def;
}

bool cmdline_is_set(const char *key)
{
        return cmdline_get_value(key) != NULL;
}

const char *option_get_string(const char *ini_section,
                        const char *ini_key,
                        const char *cmdline_key,
                        const char *def,
                        const char *description)
{
        const char *val = NULL;

        if (cmdline_key) {
                val = cmdline_get_string(cmdline_key, NULL, description);
        }

        if (val) {
                return val;
        } else {
                return ini_get_string(ini_section, ini_key, def);
        }
}

gint64 option_get_time(const char *ini_section,
                       const char *ini_key,
                       const char *cmdline_key,
                       gint64 def,
                       const char *description)
{
        gint64 ini_val = ini_get_time(ini_section, ini_key, def);
        return cmdline_get_time(cmdline_key, ini_val, description);
}

int option_get_int(const char *ini_section,
                   const char *ini_key,
                   const char *cmdline_key,
                   int def,
                   const char *description)
{
        /* *str is only used to check wether the cmdline option is actually set. */
        const char *str = cmdline_get_value(cmdline_key);

        /* we call cmdline_get_int even when the option isn't set in order to
         * add the usage info */
        int val = cmdline_get_int(cmdline_key, def, description);

        if (!str)
                return ini_get_int(ini_section, ini_key, def);
        else
                return val;
}

double option_get_double(const char *ini_section,
                         const char *ini_key,
                         const char *cmdline_key,
                         double def,
                         const char *description)
{
        const char *str = cmdline_get_value(cmdline_key);
        double val = cmdline_get_double(cmdline_key, def, description);

        if (!str)
                return ini_get_double(ini_section, ini_key, def);
        else
                return val;
}

int option_get_bool(const char *ini_section,
                    const char *ini_key,
                    const char *cmdline_key,
                    int def,
                    const char *description)
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

void cmdline_usage_append(const char *key, const char *type, const char *description)
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

const char *cmdline_create_usage(void)
{
        return usage_str;
}

/* see option_parser.h */
enum behavior_fullscreen parse_enum_fullscreen(const char *string, enum behavior_fullscreen def)
{
        if (!string)
                return def;

        if (strcmp(string, "show") == 0)
                return FS_SHOW;
        else if (strcmp(string, "delay") == 0)
                return FS_DELAY;
        else if (strcmp(string, "pushback") == 0)
                return FS_PUSHBACK;
        else {
                LOG_W("Unknown fullscreen value: '%s'\n", string);
                return def;
        }
}

/* see option_parser.h */
enum alignment parse_alignment(const char *string, enum alignment def)
{
        if (!string)
                return def;
        if (strcmp(string, "left") == 0)
                return left;
        else if (strcmp(string, "center") == 0)
                return center;
        else if (strcmp(string, "right") == 0)
                return right;

        LOG_W("Unknown alignment value: '%s'", string);
        return def;
}

/* see option_parser.h */
enum ellipsize parse_ellipsize(const char *string, enum ellipsize def)
{
        if (!string)
                return def;

        if (strcmp(string, "start") == 0)
                return start;
        else if (strcmp(string, "middle") == 0)
                return middle;
        else if (strcmp(string, "end") == 0)
                return end;

        LOG_W("Unknown ellipsize value: '%s'", string);
        return def;
}

/* see option_parser.h */
enum follow_mode parse_follow_mode(const char *string, enum follow_mode def)
{
        if (!string)
                return def;

        if (strcmp(string, "mouse") == 0)
                return FOLLOW_MOUSE;
        else if (strcmp(string, "keyboard") == 0)
                return FOLLOW_KEYBOARD;
        else if (strcmp(string, "none") == 0)
                return FOLLOW_NONE;

        LOG_W("Unknown follow mode: '%s'", string);
        return def;
}

/* see option_parser.h */
enum icon_position_t parse_icon_position(const char *string, enum icon_position_t def)
{
        if (!string)
                return def;

        if (strcmp(string, "left") == 0)
                return icons_left;
        else if (strcmp(string, "right") == 0)
                return icons_right;
        else if (strcmp(string, "off") == 0)
                return icons_off;

        LOG_W("Unknown icon position: '%s'", string);
        return def;
}

/* see option_parser.h */
enum markup_mode parse_markup_mode(const char *string, enum markup_mode def)
{
        if (!string)
                return def;

        if (strcmp(string, "strip") == 0)
                return MARKUP_STRIP;
        else if (strcmp(string, "no") == 0)
                return MARKUP_NO;
        else if (strcmp(string, "full") == 0 || strcmp(string, "yes") == 0)
                return MARKUP_FULL;

        LOG_W("Unknown markup mode: '%s'", string);
        return def;
}

/* see option_parser.h */
enum separator_color parse_sepcolor(const char *string)
{
        //TODO: map good empty separator color
        if (strcmp(string, "auto") == 0)
                return SEP_AUTO;
        else if (strcmp(string, "foreground") == 0)
                return SEP_FOREGROUND;
        else if (strcmp(string, "frame") == 0)
                return SEP_FRAME;
        else
                return SEP_CUSTOM;
}

/* see option_parser.h */
enum urgency parse_urgency(const char *string, enum urgency def)
{
        if (!string)
                return def;

        if (strcmp(string, "low") == 0)
                return URG_LOW;
        else if (strcmp(string, "normal") == 0)
                return URG_NORM;
        else if (strcmp(string, "critical") == 0)
                return URG_CRIT;

        LOG_W("Unknown urgency: '%s'", string);
        return def;
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

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
#include "settings.h"

struct entry {
        char *key;
        char *value;
};

struct section {
        char *name;
        int entry_count;
        struct entry *entries;
};

static int section_count = 0;
static struct section *sections;

static struct section *new_section(const char *name);
static struct section *get_section(const char *name);
static void add_entry(const char *section_name, const char *key, const char *value);
static const char *get_value(const char *section, const char *key);

static int cmdline_argc;
static char **cmdline_argv;

static char *usage_str = NULL;
static void cmdline_usage_append(const char *key, const char *type, const char *description);

static int cmdline_find_option(const char *key);

#define STRING_PARSE_RET(string, value) if (STR_EQ(s, string)) { *ret = value; return true; }

bool string_parse_alignment(const char *s, enum alignment *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("left",   ALIGN_LEFT);
        STRING_PARSE_RET("center", ALIGN_CENTER);
        STRING_PARSE_RET("right",  ALIGN_RIGHT);

        return false;
}

bool string_parse_ellipsize(const char *s, enum ellipsize *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("start",  ELLIPSE_START);
        STRING_PARSE_RET("middle", ELLIPSE_MIDDLE);
        STRING_PARSE_RET("end",    ELLIPSE_END);

        return false;
}

bool string_parse_follow_mode(const char *s, enum follow_mode *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("mouse",    FOLLOW_MOUSE);
        STRING_PARSE_RET("keyboard", FOLLOW_KEYBOARD);
        STRING_PARSE_RET("none",     FOLLOW_NONE);

        return false;
}


bool string_parse_fullscreen(const char *s, enum behavior_fullscreen *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("show",     FS_SHOW);
        STRING_PARSE_RET("delay",    FS_DELAY);
        STRING_PARSE_RET("pushback", FS_PUSHBACK);

        return false;
}

bool string_parse_icon_position(const char *s, enum icon_position *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("left",  ICON_LEFT);
        STRING_PARSE_RET("right", ICON_RIGHT);
        STRING_PARSE_RET("off",   ICON_OFF);

        return false;
}

bool string_parse_vertical_alignment(const char *s, enum vertical_alignment *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("top",     VERTICAL_TOP);
        STRING_PARSE_RET("center",  VERTICAL_CENTER);
        STRING_PARSE_RET("bottom",  VERTICAL_BOTTOM);

        return false;
}

bool string_parse_markup_mode(const char *s, enum markup_mode *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("strip", MARKUP_STRIP);
        STRING_PARSE_RET("no",    MARKUP_NO);
        STRING_PARSE_RET("full",  MARKUP_FULL);
        STRING_PARSE_RET("yes",   MARKUP_FULL);

        return false;
}

bool string_parse_mouse_action(const char *s, enum mouse_action *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("none",           MOUSE_NONE);
        STRING_PARSE_RET("do_action",      MOUSE_DO_ACTION);
        STRING_PARSE_RET("close_current",  MOUSE_CLOSE_CURRENT);
        STRING_PARSE_RET("close_all",      MOUSE_CLOSE_ALL);

        return false;
}

bool string_parse_sepcolor(const char *s, struct separator_color_data *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("auto",       (struct separator_color_data){.type = SEP_AUTO});
        STRING_PARSE_RET("foreground", (struct separator_color_data){.type = SEP_FOREGROUND});
        STRING_PARSE_RET("frame",      (struct separator_color_data){.type = SEP_FRAME});

        ret->type = SEP_CUSTOM;
        ret->sep_color = g_strdup(s);

        return true;
}

bool string_parse_urgency(const char *s, enum urgency *ret)
{
        ASSERT_OR_RET(STR_FULL(s), false);
        ASSERT_OR_RET(ret, false);

        STRING_PARSE_RET("low",      URG_LOW);
        STRING_PARSE_RET("normal",   URG_NORM);
        STRING_PARSE_RET("critical", URG_CRIT);

        return false;
}

struct section *new_section(const char *name)
{
        for (int i = 0; i < section_count; i++) {
                if (STR_EQ(name, sections[i].name)) {
                        DIE("Duplicated section in dunstrc detected.");
                }
        }

        section_count++;
        sections = g_realloc(sections, sizeof(struct section) * section_count);
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
        g_clear_pointer(&sections, g_free);
        section_count = 0;
}

struct section *get_section(const char *name)
{
        for (int i = 0; i < section_count; i++) {
                if (STR_EQ(sections[i].name, name))
                        return &sections[i];
        }

        return NULL;
}

void add_entry(const char *section_name, const char *key, const char *value)
{
        struct section *s = get_section(section_name);
        if (!s)
                s = new_section(section_name);

        s->entry_count++;
        int len = s->entry_count;
        s->entries = g_realloc(s->entries, sizeof(struct entry) * len);
        s->entries[s->entry_count - 1].key = g_strdup(key);
        s->entries[s->entry_count - 1].value = string_strip_quotes(value);
}

const char *get_value(const char *section, const char *key)
{
        struct section *s = get_section(section);
        ASSERT_OR_RET(s, NULL);

        for (int i = 0; i < s->entry_count; i++) {
                if (STR_EQ(s->entries[i].key, key)) {
                        return s->entries[i].value;
                }
        }
        return NULL;
}

char *ini_get_path(const char *section, const char *key, const char *def)
{
        return string_to_path(ini_get_string(section, key, def));
}

char *ini_get_string(const char *section, const char *key, const char *def)
{
        const char *value = get_value(section, key);
        if (value)
                return g_strdup(value);

        return def ? g_strdup(def) : NULL;
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
        if (value)
                return atoi(value);
        else
                return def;
}

double ini_get_double(const char *section, const char *key, double def)
{
        const char *value = get_value(section, key);
        if (value)
                return atof(value);
        else
                return def;
}

bool ini_is_set(const char *ini_section, const char *ini_key)
{
        return get_value(ini_section, ini_key) != NULL;
}

const char *next_section(const char *section)
{
        ASSERT_OR_RET(section_count > 0, NULL);
        ASSERT_OR_RET(section, sections[0].name);

        for (int i = 0; i < section_count; i++) {
                if (STR_EQ(section, sections[i].name)) {
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
        if (value) {
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
        } else {
                return def;
        }
}

int load_ini_file(FILE *fp)
{
        ASSERT_OR_RET(fp, 1);

        char *line = NULL;
        size_t line_len = 0;

        int line_num = 0;
        char *current_section = NULL;
        while (getline(&line, &line_len, fp) != -1) {
                line_num++;

                char *start = g_strstrip(line);

                if (*start == ';' || *start == '#' || STR_EMPTY(start))
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
                char *value_end = NULL;
                if (quote) {
                        value_end = strchr(quote + 1, '"');
                        if (!value_end) {
                                LOG_W("Invalid config file at line %d: Missing '\"'.", line_num);
                                continue;
                        }
                } else {
                        value_end = value;
                }

                char *comment = strpbrk(value_end, "#;");
                if (comment)
                        *comment = '\0';

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
        ASSERT_OR_RET(key, -1);

        char *key1 = g_strdup(key);
        char *key2 = strchr(key1, '/');

        if (key2) {
                *key2 = '\0';
                key2++;
        }

        /* look for first key */
        for (int i = 0; i < cmdline_argc; i++) {
                if (STR_EQ(key1, cmdline_argv[i])) {
                        g_free(key1);
                        return i;
                }
        }

        /* look for second key if one was specified */
        if (key2) {
                for (int i = 0; i < cmdline_argc; i++) {
                        if (STR_EQ(key2, cmdline_argv[i])) {
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

char *cmdline_get_string(const char *key, const char *def, const char *description)
{
        cmdline_usage_append(key, "string", description);
        const char *str = cmdline_get_value(key);

        if (str)
                return g_strdup(str);
        if (def)
                return g_strdup(def);
        else
                return NULL;
}

char *cmdline_get_path(const char *key, const char *def, const char *description)
{
        cmdline_usage_append(key, "string", description);
        const char *str = cmdline_get_value(key);

        if (str)
                return string_to_path(g_strdup(str));
        else
                return string_to_path(g_strdup(def));
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

        if (str)
                return atoi(str);
        else
                return def;
}

double cmdline_get_double(const char *key, double def, const char *description)
{
        cmdline_usage_append(key, "double", description);
        const char *str = cmdline_get_value(key);

        if (str)
                return atof(str);
        else
                return def;
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

char *option_get_path(const char *ini_section,
                      const char *ini_key,
                      const char *cmdline_key,
                      const char *def,
                      const char *description)
{
        char *val = NULL;

        if (cmdline_key) {
                val = cmdline_get_path(cmdline_key, NULL, description);
        }

        if (val) {
                return val;
        } else {
                return ini_get_path(ini_section, ini_key, def);
        }
}

char *option_get_string(const char *ini_section,
                        const char *ini_key,
                        const char *cmdline_key,
                        const char *def,
                        const char *description)
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
        if (STR_FULL(type))
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

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

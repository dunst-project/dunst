/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "option_parser.h"

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "x11/x.h"
#include "dunst.h"
#include "log.h"
#include "utils.h"
#include "settings.h"
#include "rules.h"
#include "settings_data.h"

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

int string_parse_enum(const void *data, const char *s, void * ret) {
        struct string_to_enum_def *string_to_enum = (struct string_to_enum_def*)data;
        for (int i = 0; string_to_enum[i].string != NULL; i++) {
                if (strcmp(s, string_to_enum[i].string) == 0){
                        *(int*) ret = string_to_enum[i].enum_value;
                        LOG_D("Setting enum to %i (%s)", *(int*) ret, string_to_enum[i].string);
                        return true;
                }
        }
        return false;
}

int string_parse_enum_list(const void *data, char **s, void *ret_void)
{
        int **ret = (int **) ret_void;
        int *tmp;
        ASSERT_OR_RET(s, false);
        ASSERT_OR_RET(ret, false);

        int len = 0;
        while (s[len])
                len++;

        tmp = g_malloc_n((len + 1), sizeof(int));
        for (int i = 0; i < len; i++) {
                if (!string_parse_enum(data, s[i], tmp + i)) {
                        LOG_W("Unknown mouse action value: '%s'", s[i]);
                        g_free(tmp);
                        return false;
                }
        }
        tmp[len] = MOUSE_ACTION_END; // sentinel end value
        g_free(*ret);
        *ret = tmp;
        return true;
}

int string_parse_list(const void *data, const char *s, void *ret) {
        const enum list_type type = GPOINTER_TO_INT(data);
        char **arr = NULL;
        int success = false;
        switch (type) {
                case MOUSE_LIST:
                        arr = string_to_array(s, ",");
                        success = string_parse_enum_list(&mouse_action_enum_data,
                                        arr, ret);
                        break;
                default:
                        LOG_W("Don't know this list type: %i", type);
                        break;
        }
        g_strfreev(arr);
        return success;
}

int string_parse_sepcolor(const void *data, const char *s, void *ret)
{
        LOG_D("parsing sep_color");
        struct separator_color_data *sep_color = (struct separator_color_data*) ret;

        enum separator_color type;
        bool is_enum = string_parse_enum(data, s, &type);
        if (is_enum) {
                sep_color->type = type;
                g_free(sep_color->sep_color);
                sep_color->sep_color = NULL;
                return true;
        } else {
                if (STR_EMPTY(s)) {
                        LOG_W("Sep color is empty, make sure to quote the value if it's a color.");
                        return false;
                }
                if (s[0] != '#') {
                        LOG_W("Sep color should start with a '#'");
                        return false;
                }
                if (strlen(s) < 4) {
                        LOG_W("Make sure the sep color is formatted correctly");
                        return false;
                }
                // TODO add more checks for if the color is valid

                sep_color->type = SEP_CUSTOM;
                g_free(sep_color->sep_color);
                sep_color->sep_color = g_strdup(s);
                return true;
        }
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

int get_setting_id(const char *key, const char *section) {
        int error_code = 0;
        int partial_match_id = -1;
        bool match_section = section && is_special_section(section);
        if (!match_section) {
                LOG_D("not matching section %s", section);
        }
        for (int i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                if (strcmp(allowed_settings[i].name, key) == 0) {
                        bool is_rule = allowed_settings[i].rule_offset > 0;

                        // a rule matches every section
                        if (is_rule || strcmp(section, allowed_settings[i].section) == 0) {
                                return i;
                        } else {
                                // name matches, but in wrong section. Continueing to see
                                // if we find the same setting name with another section
                                error_code = -2;
                                partial_match_id = i;
                                continue;
                        }
                }
        }

        if (error_code == -2) {
                LOG_W("Setting %s is in the wrong section (%s, should be %s)",
                                key, section,
                                allowed_settings[partial_match_id].section);
                // found, but in wrong section
                return -2;
        }

        // not found
        return -1;
}

bool set_from_string(void *target, struct setting setting, const char *value) {
        GError *error = NULL;

        if (!strlen(value)) {
                LOG_W("Cannot set empty value for setting %s", setting.name);
                return false;
        }

        bool success = false;
        // Do not use setting.value, since we might want to set a rule. Use
        // target instead
        switch (setting.type) {
                case TYPE_INT:
                        return safe_string_to_int(target, value);
                case TYPE_BOOLEAN: ;
                        // this is needed, since string_parse_enum assumses a
                        // variable of size int is passed
                        int tmp_int = -1;
                        success = string_parse_enum(boolean_enum_data, value, &tmp_int);

                        if (!success) LOG_W("Unknown %s value: '%s'. It should be a valid boolean",
                                        setting.name, value);

                        if (tmp_int < 0 || tmp_int > 1)
                        {
                                // should not happen if boolean_enum_data is correct
                                LOG_W("TYPE_BOOLEAN out of range");
                                return false;
                        }

                        *(bool*) target = (bool) tmp_int;
                        return success;
                case TYPE_STRING:
                        g_free(*(char**) target);
                        *(char**) target = g_strdup(value);
                        return true;
                case TYPE_CUSTOM:
                        if (setting.parser == NULL) {
                                LOG_W("Setting %s doesn't have parser", setting.name);
                                return false;
                        }
                        success = setting.parser(setting.parser_data, value, target);

                        if (!success) LOG_W("Invalid %s value: '%s'", setting.name, value);
                        return success;
                case TYPE_PATH: ;
                        g_free(*(char**) target);
                        *(char**) target = string_to_path(g_strdup(value));

                        // TODO make scripts take arguments in the config and
                        // deprecate the arguments that are now passed to the
                        // scripts
                        if (!setting.parser_data)
                                return true;
                        g_strfreev(*(char***)setting.parser_data);
                        if (!g_shell_parse_argv(*(char**) target, NULL, (char***)setting.parser_data, &error)) {
                                LOG_W("Unable to parse %s command: '%s'. "
                                                "It's functionality will be disabled.",
                                                setting.name, error->message);
                                g_error_free(error);
                                return false;
                        }
                        return true;
                case TYPE_TIME: ;
                        gint64 tmp_time = string_to_time(value);
                        if (errno != 0) {
                                return false;
                        }
                        *(gint64*) target = tmp_time;
                        return true;
                case TYPE_GEOMETRY:
                        *(struct geometry*) target = x_parse_geometry(value);
                        return true;
                case TYPE_LIST: ;
                        LOG_D("list type %i", GPOINTER_TO_INT(setting.parser_data));
                        return string_parse_list(setting.parser_data, value, target);
                default:
                        LOG_W("Setting type of '%s' is not known (type %i)", setting.name, setting.type);
                        return false;
        }
}

bool set_setting(struct setting setting, char* value) {
        LOG_D("Trying to set %s to %s", setting.name, value);
        if (setting.value == NULL) {
                // setting.value is NULL, so it must be only a rule
                return true;
        }

        return set_from_string(setting.value, setting, value);
}

int set_rule_value(struct rule* r, struct setting setting, char* value) {
        // Apply rule member offset. Converting to char* because it's
        // guaranteed to be 1 byte
        void *target = (char*)r + setting.rule_offset;

        return set_from_string(target, setting, value);
}

bool set_rule(struct setting setting, char* value, char* section) {
        struct rule *r = get_rule(section);
        if (!r) {
                r = rule_new(section);
                LOG_D("Creating new rule '%s'", section);
        }

        return set_rule_value(r, setting, value);
}

void set_defaults() {
        for (int i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
                // FIXME Rule settings can only have a default if they have an
                // working entry in the settings struct as well. Make an
                // alternative way of setting defaults for rules.

                if (!allowed_settings[i].value) // don't set default if it's only a rule
                        continue;

                if(!set_setting(allowed_settings[i], allowed_settings[i].default_value)) {
                        LOG_E("Could not set default of setting %s", allowed_settings[i].name);
                }
        }
}

void save_settings() {
        for (int i = 0; i < section_count; i++) {
                const struct section curr_section = sections[i];

                if (is_deprecated_section(curr_section.name)) {
                        LOG_W("Section %s is deprecated. Ignoring", curr_section.name);
                        continue;
                }

                for (int j = 0; j < curr_section.entry_count; j++) {
                        const struct entry curr_entry = curr_section.entries[j];
                        int setting_id = get_setting_id(curr_entry.key, curr_section.name);
                        struct setting curr_setting = allowed_settings[setting_id];
                        if (setting_id < 0){
                                if (setting_id == -1) {
                                        LOG_W("Setting %s in section %s doesn't exist", curr_entry.key, curr_section.name);
                                }
                                continue;
                        }

                        bool is_rule = curr_setting.rule_offset > 0;
                        if (is_special_section(curr_section.name)) {
                                if (is_rule) {
                                        // set as a rule, but only if it's not a filter
                                        if (rule_offset_is_action(curr_setting.rule_offset)) {
                                                LOG_D("Adding rule '%s = %s' to special section %s",
                                                                curr_entry.key,
                                                                curr_entry.value,
                                                                curr_section.name);
                                                set_rule(curr_setting, curr_entry.value, curr_section.name);
                                        } else {
                                                LOG_W("Cannot use filtering rules in special section. Ignoring %s in section %s.",
                                                                curr_entry.key,
                                                                curr_section.name);
                                        }
                                } else {
                                        // set as a regular setting
                                        set_setting(curr_setting, curr_entry.value);
                                }
                        } else {
                                // interpret this section as a rule
                                LOG_D("Adding rule '%s = %s' to section %s",
                                                curr_entry.key,
                                                curr_entry.value,
                                                curr_section.name);
                                set_rule(curr_setting, curr_entry.value, curr_section.name);
                        }
                }
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

char **cmdline_get_list(const char *key, const char *def, const char *description)
{
        cmdline_usage_append(key, "list", description);
        const char *str = cmdline_get_value(key);

        if (str)
                return string_to_array(str, ",");
        else
                return string_to_array(def, ",");
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

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

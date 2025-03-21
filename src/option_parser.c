/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include "option_parser.h"

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "dunst.h"
#include "log.h"
#include "utils.h"
#include "settings.h"
#include "rules.h"
#include "settings_data.h"

static int cmdline_argc;
static char **cmdline_argv;
static char *usage_str = NULL;

#define STRING_PARSE_RET(string, value) if (STR_EQ(s, string)) { *ret = value; return true; }

int string_parse_enum(const void *data, const char *s, void * ret) {
        struct string_to_enum_def *string_to_enum = (struct string_to_enum_def*)data;
        for (int i = 0; string_to_enum[i].string != NULL; i++) {
                if (strcmp(s, string_to_enum[i].string) == 0) {
                        *(int*) ret = string_to_enum[i].enum_value;
                        LOG_D("Setting enum to %i (%s)", *(int*) ret, string_to_enum[i].string);
                        return true;
                }
        }
        return false;
}

// Only changes the return when succesful
//
// @returns True for success, false otherwise.
int string_parse_enum_list(const void *data, char **s, void *ret_void)
{
        int **ret = (int **) ret_void;
        int *tmp;
        ASSERT_OR_RET(s, false);
        ASSERT_OR_RET(ret, false);

        int len = string_array_length(s);

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

// Parse a string list of enum values and return a single integer with the
// values bit-flipped into it. This is only useful when the enum values all
// occupy different bits (for exampel 1<<0, 1<<1 and 1<<2) and the order
// doesn't matter.
// Only changes the return when succesful
//
// @returns True for success, false otherwise.
int string_parse_enum_list_to_single(const void *data, char **s, int *ret)
{
        int tmp = 0, tmp_ret = 0;
        ASSERT_OR_RET(s, false);
        ASSERT_OR_RET(ret, false);

        int len = string_array_length(s);
        for (int i = 0; i < len; i++) {
                if (!string_parse_enum(data, s[i], &tmp)) {
                        LOG_W("Unknown value: '%s'", s[i]);
                        return false;
                }
                tmp_ret |= tmp;
        }
        *ret = tmp_ret;
        return true;
}

int string_parse_corners(const void *data, const char *s, void *ret)
{
        char **s_arr = string_to_array(s, ",");
        int success = string_parse_enum_list_to_single(data, s_arr, ret);
        g_strfreev(s_arr);
        return success;
}

// When allow empty is true, empty strings are interpreted as -1
bool string_parse_int_list(char **s, int **ret, bool allow_empty) {
        int len = string_array_length(s);
        ASSERT_OR_RET(s, false);

        int *tmp = g_malloc_n((len + 1), sizeof(int));
        for (int i = 0; i < len; i++) {
                if (allow_empty && STR_EMPTY(s[i])) {
                        tmp[i] = -1;
                        continue;
                }
                bool success = safe_string_to_int(&tmp[i], s[i]);
                if (!success) {
                        LOG_W("Invalid int value: '%s'", s[i]);
                        g_free(tmp);
                        return false;
                }

        }

        tmp[len] = LIST_END;
        g_free(*ret);
        *ret = tmp;
        return true;
}

// Only changes the return when succesful
int string_parse_list(const void *data, const char *s, void *ret) {
        const enum list_type type = GPOINTER_TO_INT(data);
        char **arr = NULL;
        int success = false;
        switch (type) {
                case MOUSE_LIST:
                        arr = string_to_array(s, ",");
                        success = string_parse_enum_list(&mouse_action_enum_data, arr, ret);
                        break;
                case OFFSET_LIST:
                        arr = string_to_array(s, "x");
                        int len = string_array_length(arr);
                        if (len != 2) {
                                success = false;
                                break;
                        }
                        int *int_arr = NULL;
                        success = string_parse_int_list(arr, &int_arr, false);
                        if (!success)
                                break;

                        struct position* offset = (struct position*) ret;
                        offset->x = int_arr[0];
                        offset->y = int_arr[1];
                        g_free(int_arr);
                        break;
                case STRING_LIST: ;
                        g_strfreev(*(char ***) ret);
                        *(char ***) ret = string_to_array(s, ",");
                        success = true;
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
        struct color invalid = COLOR_UNINIT;

        enum separator_color type;
        bool is_enum = string_parse_enum(data, s, &type);
        if (is_enum) {
                sep_color->type = type;
                sep_color->color = invalid;
                return true;
        } else {
                if (STR_EMPTY(s)) {
                        LOG_W("Sep color is empty, make sure to quote the value if it's a color.");
                        return false;
                }

                if (string_parse_color(s, &sep_color->color)) {
                        sep_color->type = SEP_CUSTOM;
                        return true;
                }
        }
        return false;
}

#define UINT_MAX_N(bits) ((1 << bits) - 1)

// Parse a #RRGGBB[AA] string
int string_parse_color(const char *s, struct color *ret)
{
        if (STR_EMPTY(s) || *s != '#') {
                LOG_W("A color string should start with '#' and contain at least 3 hex characters");
                return false;
        }

        char *end = NULL;
        unsigned long val = strtoul(s + 1, &end, 16);

        if (end[0] != '\0' && end[1] != '\0') {
                LOG_W("Invalid color string: '%s'", s);
                return false;
        }

        int bpc = 0;
        switch (end - (s + 1)) {
                case 3:
                        bpc = 4;
                        val = (val << 4) | 0xF;
                        break;
                case 6:
                        bpc = 8;
                        val = (val << 8) | 0xFF;
                        break;
                case 4:
                        bpc = 4;
                        break;
                case 8:
                        bpc = 8;
                        break;
                default:
                        LOG_W("Invalid color string: '%s'", s);
                        return false;
        }

        const unsigned single_max = UINT_MAX_N(bpc);

        ret->r = ((val >> 3 * bpc) & single_max) / (double)single_max;
        ret->g = ((val >> 2 * bpc) & single_max) / (double)single_max;
        ret->b = ((val >> 1 * bpc) & single_max) / (double)single_max;
        ret->a = ((val)            & single_max) / (double)single_max;

        return true;
}

int string_parse_gradient(const char *s, struct gradient **ret)
{
        struct color colors[16];
        size_t length = 0;

        gchar **strs = g_strsplit(s, ",", -1);
        for (int i = 0; strs[i] != NULL; i++) {
                if (i > 16) {
                        LOG_W("Do you really need so many colors? ;)");
                        break;
                }

                if (!string_parse_color(g_strstrip(strs[i]), &colors[length++])) {
                        g_strfreev(strs);
                        return false;
                }
        }

        g_strfreev(strs);
        if (length == 0) {
                LOG_W("Provide at least one color");
                return false;
        }

        *ret = gradient_alloc(length);
        memcpy((*ret)->colors, colors, length * sizeof(struct color));
        gradient_pattern(*ret);

        return true;
}

int string_parse_bool(const void *data, const char *s, void *ret)
{
        // this is needed, since string_parse_enum assumses a
        // variable of size int is passed
        int tmp_int = -1;
        bool success = string_parse_enum(data, s, &tmp_int);

        *(bool*) ret = (bool) tmp_int;
        return success;
}

// Parse a string that may represent an integer value
int string_parse_maybe_int(const void *data, const char *s, void *ret)
{
        int *intval = (int *)data;
        if (!safe_string_to_int(intval, s)) {
               *intval = INT_MIN;
        }

        g_free(*(char**) ret);
        *(char**) ret = g_strdup(s);
        return true;
}

int get_setting_id(const char *key, const char *section) {
        int error_code = 0;
        int partial_match_id = -1;
        bool match_section = section && is_special_section(section);
        if (!match_section) {
                LOG_D("not matching section %s", section);
        }
        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
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

// NOTE: We do minimal checks in this function so the values should be sanitized somewhere else
int string_parse_length(void *ret_in, const char *s) {
        struct length *ret = (struct length*) ret_in;
        char *s_stripped = string_strip_brackets(s);

        // single int without brackets
        if (!s_stripped) {
                int val = 0;
                bool success = safe_string_to_int(&val, s);
                if (!success) {
                        LOG_W("Specify either a single value or two comma-separated values between parentheses");
                        return false;
                }

                ret->min = val;
                ret->max = val;
                return true;
        }

        char **s_arr = string_to_array(s_stripped, ",");
        g_free(s_stripped);

        int len = string_array_length(s_arr);
        if (len != 2) {
                g_strfreev(s_arr);
                LOG_W("Specify either a single value or two comma-separated values between parentheses");
                return false;
        }

        int *int_arr = NULL;
        bool success = string_parse_int_list(s_arr, &int_arr, true);
        g_strfreev(s_arr);

        if (!success)
                return false;

        ret->min = int_arr[0] == -1 ? INT_MIN : int_arr[0];
        ret->max = int_arr[1] == -1 ? INT_MAX : int_arr[1];

        g_free(int_arr);
        return success;
}

bool set_from_string(void *target, struct setting setting, const char *value) {
        GError *error = NULL;

        if (!strlen(value) && setting.type != TYPE_STRING) {
                LOG_W("Cannot set empty value for setting %s", setting.name);
                return false;
        }

        bool success = false;
        // Do not use setting.value, since we might want to set a rule. Use
        // target instead
        switch (setting.type) {
                case TYPE_INT:
                        return safe_string_to_int(target, value);
                case TYPE_DOUBLE:
                        return safe_string_to_double(target, value);
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
                case TYPE_LIST: ;
                        LOG_D("list type %i", GPOINTER_TO_INT(setting.parser_data));
                        return string_parse_list(setting.parser_data, value, target);
                case TYPE_LENGTH:
                        // Keep compatibility with old offset syntax
                        if (STR_EQ(setting.name, "offset") && string_parse_list(GINT_TO_POINTER(OFFSET_LIST), value, target)) {
                                LOG_M("Using legacy offset syntax NxN, you should switch to the new syntax (N, N)");
                                return true;
                        }

                        // Keep compatibility with old height semantics
                        if (STR_EQ(setting.name, "height") && string_is_int(value)) {
                                LOG_M("Setting 'height' has changed behaviour after dunst 1.12.0, see https://dunst-project.org/release/#v1.12.0.");
                                LOG_M("Legacy height support may be dropped in the future. If you want to hide this message transition to");
                                LOG_M("'height = (0, X)' for dynamic height (old behaviour equivalent) or to 'height = (X, X)' for a fixed height.");

                                int height;
                                if (!safe_string_to_int(&height, value))
                                        return false;

                                ((struct length *)target)->min = 0;
                                ((struct length *)target)->max = height;
                                return true;
                        }
                        return string_parse_length(target, value);
                case TYPE_COLOR:
                        return string_parse_color(value, target);
                case TYPE_GRADIENT:
                        return string_parse_gradient(value, target);
                default:
                        LOG_W("Setting type of '%s' is not known (type %i)", setting.name, setting.type);
                        return false;
        }
}

bool set_setting(struct setting setting, char* value) {
        LOG_D("[%s] Trying to set %s to %s", setting.section, setting.name, value);
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

void set_defaults(void) {
        LOG_D("Initializing settings");
        settings = (struct settings) {0};

        for (size_t i = 0; i < G_N_ELEMENTS(allowed_settings); i++) {
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

void save_settings(struct ini *ini) {
        for (int i = 0; i < ini->section_count; i++) {
                const struct section curr_section = ini->sections[i];

                if (is_deprecated_section(curr_section.name)) {
                        LOG_W("Section %s is deprecated.\n%s\nIgnoring this section.",
                                        curr_section.name,
                                        get_section_deprecation_message(curr_section.name));
                        continue;
                }

                LOG_D("Entering section [%s]", curr_section.name);
                for (int j = 0; j < curr_section.entry_count; j++) {
                        const struct entry curr_entry = curr_section.entries[j];
                        int setting_id = get_setting_id(curr_entry.key, curr_section.name);
                        struct setting curr_setting = allowed_settings[setting_id];
                        if (setting_id < 0) {
                                if (setting_id == -1) {
                                        LOG_W("Setting %s in section %s doesn't exist", curr_entry.key, curr_section.name);
                                }
                                continue;
                        }

                        bool is_rule = curr_setting.rule_offset > 0;
                        if (is_special_section(curr_section.name)) {
                                if (is_rule) {
                                        // set as a rule, but only if it's not a filter
                                        if (rule_offset_is_modifying(curr_setting.rule_offset)) {
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
                                        char *value = g_strstrip(curr_entry.value);
                                        set_setting(curr_setting, value);
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

void cmdline_load(int argc, char *argv[])
{
        cmdline_argc = argc;
        cmdline_argv = argv;
}

static int cmdline_find_option(const char *key, int start)
{
        ASSERT_OR_RET(key, -1);

        gchar **keys = g_strsplit(key, "/", -1);

        for (int i = 0; keys[i] != NULL; i++) {
                for (int j = start; j < cmdline_argc; j++) {
                        if (STR_EQ(keys[i], cmdline_argv[j])) {
                                g_strfreev(keys);
                                return j;
                        }
                }
        }

        g_strfreev(keys);
        return -1;
}

static const char *cmdline_get_value(const char *key, int start, int *found)
{
        int idx = cmdline_find_option(key, start);
        if (idx < 0) {
                return NULL;
        }

        if (found)
                *found = idx + 1;

        if (idx + 1 >= cmdline_argc) {
                /* the argument is missing */
                LOG_W("%s: Missing argument. Ignoring.", key);
                return NULL;
        }
        return cmdline_argv[idx + 1];
}

char *cmdline_get_string_offset(const char *key, const char *def, int start, int *found)
{
        const char *str = cmdline_get_value(key, start, found);

        if (str)
                return g_strdup(str);
        if (def)
                return g_strdup(def);
        else
                return NULL;
}

char *cmdline_get_string(const char *key, const char *def, const char *description)
{
        cmdline_usage_append(key, "string", description);
        return cmdline_get_string_offset(key, def, 1, NULL);
}

char *cmdline_get_path(const char *key, const char *def, const char *description)
{
        cmdline_usage_append(key, "string", description);
        const char *str = cmdline_get_value(key, 1, NULL);

        if (str)
                return string_to_path(g_strdup(str));
        else
                return string_to_path(g_strdup(def));
}

char **cmdline_get_list(const char *key, const char *def, const char *description)
{
        cmdline_usage_append(key, "list", description);
        const char *str = cmdline_get_value(key, 1, NULL);

        if (str)
                return string_to_array(str, ",");
        else
                return string_to_array(def, ",");
}

gint64 cmdline_get_time(const char *key, gint64 def, const char *description)
{
        cmdline_usage_append(key, "time", description);
        const char *timestring = cmdline_get_value(key, 1, NULL);
        gint64 val = def;

        if (timestring) {
                val = string_to_time(timestring);
        }

        return val;
}

int cmdline_get_int(const char *key, int def, const char *description)
{
        cmdline_usage_append(key, "int", description);
        const char *str = cmdline_get_value(key, 1, NULL);

        if (str)
                return atoi(str);
        else
                return def;
}

double cmdline_get_double(const char *key, double def, const char *description)
{
        cmdline_usage_append(key, "double", description);
        const char *str = cmdline_get_value(key, 1, NULL);

        if (str)
                return atof(str);
        else
                return def;
}

int cmdline_get_bool(const char *key, int def, const char *description)
{
        cmdline_usage_append(key, "", description);
        int idx = cmdline_find_option(key, 1);

        if (idx > 0)
                return true;
        else
                return def;
}

bool cmdline_is_set(const char *key)
{
        return cmdline_get_value(key, 1, NULL) != NULL;
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
                    g_strdup_printf("%-50s - %s\n", key_type, description);
                g_free(key_type);
                return;
        }

        char *tmp;
        tmp = g_strdup_printf("%s%-50s - %s\n", usage_str, key_type, description);
        g_free(key_type);

        g_free(usage_str);
        usage_str = tmp;

}

const char *cmdline_create_usage(void)
{
        return usage_str;
}

/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

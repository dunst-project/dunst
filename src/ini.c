#include "ini.h"

#include "utils.h"
#include "log.h"

struct section *get_section(struct ini *ini, const char *name)
{
        for (int i = 0; i < ini->section_count; i++) {
                if (STR_EQ(ini->sections[i].name, name))
                        return &ini->sections[i];
        }

        return NULL;
}

struct section *get_or_create_section(struct ini *ini, const char *name)
{
        struct section *s = get_section(ini, name);
        if (!s) {
                ini->section_count++;
                ini->sections = g_realloc(ini->sections, sizeof(struct section) * ini->section_count);

                s = &ini->sections[ini->section_count - 1];
                s->name = g_strdup(name);
                s->entries = NULL;
                s->entry_count = 0;
        }
        return s;
}

void add_entry(struct ini *ini, const char *section_name, const char *key, const char *value)
{
        struct section *s = get_or_create_section(ini, section_name);

        s->entry_count++;
        int len = s->entry_count;
        s->entries = g_realloc(s->entries, sizeof(struct entry) * len);
        s->entries[s->entry_count - 1].key = g_strdup(key);
        s->entries[s->entry_count - 1].value = string_strip_quotes(value);
}

const char *section_get_value(struct ini *ini, const struct section *s, const char *key)
{
        ASSERT_OR_RET(s, NULL);

        for (int i = 0; i < s->entry_count; i++) {
                if (STR_EQ(s->entries[i].key, key)) {
                        return s->entries[i].value;
                }
        }
        return NULL;
}

const char *get_value(struct ini *ini, const char *section, const char *key)
{
        struct section *s = get_section(ini, section);
        return section_get_value(ini, s, key);
}

bool ini_is_set(struct ini *ini, const char *ini_section, const char *ini_key)
{
        return get_value(ini, ini_section, ini_key) != NULL;
}

const char *next_section(const struct ini *ini,const char *section)
{
        ASSERT_OR_RET(ini->section_count > 0, NULL);
        ASSERT_OR_RET(section, ini->sections[0].name);

        for (int i = 0; i < ini->section_count; i++) {
                if (STR_EQ(section, ini->sections[i].name)) {
                        if (i + 1 >= ini->section_count)
                                return NULL;
                        else
                                return ini->sections[i + 1].name;
                }
        }
        return NULL;
}

struct ini *load_ini_file(FILE *fp)
{
        if (!fp)
                return NULL;

        struct ini *ini = calloc(1, sizeof(struct ini));
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

                add_entry(ini, current_section, key, value);
        }
        free(line);
        g_free(current_section);
        return ini;
}


void finish_ini(struct ini *ini)
{
        for (int i = 0; i < ini->section_count; i++) {
                for (int j = 0; j < ini->sections[i].entry_count; j++) {
                        g_free(ini->sections[i].entries[j].key);
                        g_free(ini->sections[i].entries[j].value);
                }
                g_free(ini->sections[i].entries);
                g_free(ini->sections[i].name);
        }
        g_clear_pointer(&ini->sections, g_free);
        ini->section_count = 0;
}


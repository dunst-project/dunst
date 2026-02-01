/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file
 * @ingroup config
 * @brief Parser for INI config files
 * @copyright Copyright 2013-2014 Sascha Kruse
 * @copyright Copyright 2014-2026 Dunst contributors
 * @license BSD-3-Clause
 */

#ifndef DUNST_INI_H
#define DUNST_INI_H

#include <stdbool.h>
#include <stdio.h>

struct entry {
        char *key;
        char *value;
};

struct section {
        char *name;
        int entry_count;
        struct entry *entries;
};

struct ini {
        int section_count;
        struct section *sections;
};

/**
 * @return the next known section
 * @retval if section == NULL returns first section
 * @retval NULL if no more sections are available
 */
const char *next_section(const struct ini *ini, const char *section);
const char *section_get_value(struct ini *ini, const struct section *s, const char *key);
const char *get_value(struct ini *ini, const char *section, const char *key);
struct ini *load_ini_file(FILE *fp);
void finish_ini(struct ini *ini);

#endif

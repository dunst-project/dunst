/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
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

/* returns the next known section.
 * if section == NULL returns first section.
 * returns NULL if no more sections are available
 */
const char *next_section(const struct ini *ini,const char *section);
const char *section_get_value(struct ini *ini, const struct section *s, const char *key);
const char *get_value(struct ini *ini, const char *section, const char *key);
struct ini *load_ini_file(FILE *fp);
void finish_ini(struct ini *ini);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

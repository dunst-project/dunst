/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_OPTION_PARSER_H
#define DUNST_OPTION_PARSER_H

#include <stdbool.h>
#include <stdio.h>

int load_ini_file(FILE *);
char *ini_get_path(const char *section, const char *key, const char *def);
char *ini_get_string(const char *section, const char *key, const char *def);
int ini_get_int(const char *section, const char *key, int def);
double ini_get_double(const char *section, const char *key, double def);
int ini_get_bool(const char *section, const char *key, int def);
bool ini_is_set(const char *ini_section, const char *ini_key);
void free_ini(void);

void cmdline_load(int argc, char *argv[]);
/* for all cmdline_get_* key can be either "-key" or "-key/-longkey" */
char *cmdline_get_string(const char *key, const char *def, const char *description);
char *cmdline_get_path(const char *key, const char *def, const char *description);
int cmdline_get_int(const char *key, int def, const char *description);
double cmdline_get_double(const char *key, double def, const char *description);
int cmdline_get_bool(const char *key, int def, const char *description);
bool cmdline_is_set(const char *key);
const char *cmdline_create_usage(void);

char *option_get_string(const char *ini_section,
                        const char *ini_key,
                        const char *cmdline_key,
                        const char *def,
                        const char *description);
char *option_get_path(const char *ini_section,
                      const char *ini_key,
                      const char *cmdline_key,
                      const char *def,
                      const char *description);
int option_get_int(const char *ini_section,
                   const char *ini_key,
                   const char *cmdline_key,
                   int def,
                   const char *description);
double option_get_double(const char *ini_section,
                         const char *ini_key,
                         const char *cmdline_key,
                         double def,
                         const char *description);
int option_get_bool(const char *ini_section,
                    const char *ini_key,
                    const char *cmdline_key,
                    int def,
                    const char *description);

/* returns the next known section.
 * if section == NULL returns first section.
 * returns NULL if no more sections are available
 */
const char *next_section(const char *section);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

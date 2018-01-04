/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_OPTION_PARSER_H
#define DUNST_OPTION_PARSER_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include "dunst.h"

int load_ini_file(FILE *);
const char *ini_get_string(const char *section, const char *key, const char *def);
gint64 ini_get_time(const char *section, const char *key, gint64 def);
int ini_get_int(const char *section, const char *key, int def);
double ini_get_double(const char *section, const char *key, double def);
int ini_get_bool(const char *section, const char *key, int def);
bool ini_is_set(const char *ini_section, const char *ini_key);
void free_ini(void);

void cmdline_load(int argc, char *argv[]);
/* for all cmdline_get_* key can be either "-key" or "-key/-longkey" */
const char *cmdline_get_string(const char *key, const char *def, const char *description);
int cmdline_get_int(const char *key, int def, const char *description);
double cmdline_get_double(const char *key, double def, const char *description);
int cmdline_get_bool(const char *key, int def, const char *description);
bool cmdline_is_set(const char *key);
const char *cmdline_create_usage(void);

const char *option_get_string(const char *ini_section,
                        const char *ini_key,
                        const char *cmdline_key,
                        const char *def,
                        const char *description);
gint64 option_get_time(const char *ini_section,
                       const char *ini_key,
                       const char *cmdline_key,
                       gint64 def,
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

/**
 * Parse the fullscreen behavior value of the given string
 *
 * @param string the string representation of #behavior_fullscreen.
 *               The string must not contain any waste characters.
 * @param def value to return in case of errors
 *
 * @return the #behavior_fullscreen representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
enum behavior_fullscreen parse_enum_fullscreen(const char *string, enum behavior_fullscreen def);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

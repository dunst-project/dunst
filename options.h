/* copyright 2012 Sascha Kruse and contributors (see LICENSE for licensing information) */
#pragma once

#include <stdio.h>


int load_ini_file(FILE *);
char *ini_get_string(char *section, char *key, const char *def);
int ini_get_int(char *section, char *key, int def);
double ini_get_double(char *section, char *key, int def);
int ini_get_bool(char *section, char *key, int def);
void free_ini(void);

/* returns the next known section.
 * if section == NULL returns first section.
 * returns NULL if no more sections are available
 */
char *next_section(char *section);

/* vim: set ts=8 sw=8 tw=0: */

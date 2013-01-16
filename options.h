/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#pragma once

#include <stdio.h>

int load_ini_file(FILE *);
char *ini_get_string(char *section, char *key, const char *def);
int ini_get_int(char *section, char *key, int def);
double ini_get_double(char *section, char *key, double def);
int ini_get_bool(char *section, char *key, int def);
void free_ini(void);

void cmdline_load(int argc, char *argv[]);
/* for all cmdline_get_* key can be either "-key" or "-key/-longkey" */
char *cmdline_get_string(char *key, const char *def, char *description);
int cmdline_get_int(char *key, int def, char *description);
double cmdline_get_double(char *key, double def, char *description);
int cmdline_get_bool(char *key, int def, char *description);
char *cmdline_create_usage(void);

char *option_get_string(char *ini_section, char *ini_key, char *cmdline_key,
                        const char *def, char *description);
int option_get_int(char *ini_section, char *ini_key, char *cmdline_key, int def,
                   char *description);
double option_get_double(char *ini_section, char *ini_key, char *cmdline_key,
                         double def, char *description);
int option_get_bool(char *ini_section, char *ini_key, char *cmdline_key,
                    int def, char *description);

/* returns the next known section.
 * if section == NULL returns first section.
 * returns NULL if no more sections are available
 */
char *next_section(char *section);

/* vim: set ts=8 sw=8 tw=0: */

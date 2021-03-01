/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_OPTION_PARSER_H
#define DUNST_OPTION_PARSER_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include "dunst.h"
#include "settings.h"

int string_parse_enum(void* data, const char *s, void * ret);
int string_parse_sepcolor(void *data, const char *s, void *ret);

bool string_parse_fullscreen(const char *s, enum behavior_fullscreen *ret);
bool string_parse_markup_mode(const char *s, enum markup_mode *ret);
bool string_parse_urgency(const char *s, enum urgency *ret);

int load_ini_file(FILE *);
void set_defaults();
void save_settings();
bool ini_is_set(const char *ini_section, const char *ini_key);
void free_ini(void);

void cmdline_load(int argc, char *argv[]);
/* for all cmdline_get_* key can be either "-key" or "-key/-longkey" */
char *cmdline_get_string(const char *key, const char *def, const char *description);
char *cmdline_get_path(const char *key, const char *def, const char *description);
char **cmdline_get_list(const char *key, const char *def, const char *description);
int cmdline_get_int(const char *key, int def, const char *description);
double cmdline_get_double(const char *key, double def, const char *description);
int cmdline_get_bool(const char *key, int def, const char *description);
bool cmdline_is_set(const char *key);
const char *cmdline_create_usage(void);

/* returns the next known section.
 * if section == NULL returns first section.
 * returns NULL if no more sections are available
 */
const char *next_section(const char *section);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

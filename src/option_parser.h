/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_OPTION_PARSER_H
#define DUNST_OPTION_PARSER_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include "dunst.h"
#include "settings.h"
#include "ini.h"

int string_parse_enum(const void* data, const char *s, void * ret);
int string_parse_sepcolor(const void *data, const char *s, void *ret);
int string_parse_bool(const void *data, const char *s, void *ret);

void set_defaults();
void save_settings(struct ini *ini);

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

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

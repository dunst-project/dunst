/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_OPTION_PARSER_H
#define DUNST_OPTION_PARSER_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include "dunst.h"
#include "settings.h"

bool string_parse_alignment(const char *s, enum alignment *ret);
bool string_parse_ellipsize(const char *s, enum ellipsize *ret);
bool string_parse_follow_mode(const char *s, enum follow_mode *ret);
bool string_parse_fullscreen(const char *s, enum behavior_fullscreen *ret);
bool string_parse_icon_position(const char *s, enum icon_position *ret);
bool string_parse_vertical_alignment(const char *s, enum vertical_alignment *ret);
bool string_parse_markup_mode(const char *s, enum markup_mode *ret);
bool string_parse_mouse_action(const char *s, enum mouse_action *ret);
bool string_parse_sepcolor(const char *s, struct separator_color_data *ret);
bool string_parse_urgency(const char *s, enum urgency *ret);

int load_ini_file(FILE *);
char *ini_get_path(const char *section, const char *key, const char *def);
char *ini_get_string(const char *section, const char *key, const char *def);
gint64 ini_get_time(const char *section, const char *key, gint64 def);
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

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

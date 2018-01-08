/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_OPTION_PARSER_H
#define DUNST_OPTION_PARSER_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include "dunst.h"

int load_ini_file(FILE *);
const char *ini_get_string(const char *section, const char *key, const char *def);
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

/**
 * Parse the alignment value of the given string
 *
 * @param string the string representation of #alignment.
 *               The string must not contain any waste characters.
 * @param def value to return in case of errors
 *
 * @return the #alignment representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
enum alignment parse_alignment(const char *string, enum alignment def);

/**
 * Parse the ellipsize value of the given string
 *
 * @param string the string representation of #ellipsize.
 *               The string must not contain any waste characters.
 * @param def value to return in case of errors
 *
 * @return the #ellipsize representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
enum ellipsize parse_ellipsize(const char *string, enum ellipsize def);

/**
 * Parse the follow mode value of the given string
 *
 * @param string the string representation of #follow_mode.
 *               The string must not contain any waste characters.
 * @param def value to return in case of errors
 *
 * @return the #follow_mode representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
enum follow_mode parse_follow_mode(const char *string, enum follow_mode def);

/**
 * Parse the icon position value of the given string
 *
 * @param string the string representation of #icon_position_t.
 *               The string must not contain any waste characters.
 * @param def value to return in case of errors
 *
 * @return the #icon_position_t representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
enum icon_position_t parse_icon_position(const char *string, enum icon_position_t def);

/**
 * Parse the markup mode value of the given string
 *
 * @param string the string representation of #markup_mode.
 *               The string must not contain any waste characters.
 * @param def value to return in case of errors
 *
 * @return the #markup_mode representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
enum markup_mode parse_markup_mode(const char *string, enum markup_mode def);

/**
 * Parse the separator color of the given string
 *
 * @param string the string representation of #separator_color.
 *               The string must not contain any waste characters.
 * @param def value to return in case of errors
 *
 * @return the #separator_color representation of `string`
 * @return `def` if `string` is `NULL`
 */
struct separator_color_data parse_sepcolor(const char *string, struct separator_color_data def);

/**
 * Parse the urgency value of the given string
 *
 * @param string the string representation of #urgency.
 *               The string must not contain any waste characters.
 * @param def value to return in case of errors
 *
 * @return the #urgency representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
enum urgency parse_urgency(const char *string, enum urgency def);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include <glib.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef DUNST_LOG_H
#define DUNST_LOG_H

#define LOG_E g_error
#define LOG_C g_critical
#define LOG_W g_warning
#define LOG_M g_message
#define LOG_I g_info
#define LOG_D g_debug

#define DIE(...) do { LOG_C(__VA_ARGS__); exit(EXIT_FAILURE); } while (0)


/**
 * Return the corresponding string representation of `level`
 *
 * @param level The level to convert to a string
 *
 * @return the representation of the given level
 * @return `"UNKNOWN"` if `level` is an invalid
 */
const char *log_level_to_string(GLogLevelFlags level);

/**
 * Return the corresponding log level for given string
 *
 * @param level The string to convert. It shall not
 *              contain waste characters.
 * @param def The default to fallback to
 *
 * @return the representation of the given logstring
 * @return `def` if `string` is an invalid value or `NULL`
 */
GLogLevelFlags string_parse_loglevel(const char *level, GLogLevelFlags def);

/**
 * Initialise log handling. Can be called any time.
 *
 * To avoid any issues, it'll set #settings_t.log_level of
 * the #settings variable to `G_LOG_LEVEL_MESSAGE`.
 *
 * @param testing If we're in testing mode and should
 *                suppress all output
 */
void dunst_log_init(bool testing);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

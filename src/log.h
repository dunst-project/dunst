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
 * Set the current loglevel to `level`
 *
 * @param level The desired log level
 *
 * If `level` is `NULL`, nothing will be done.
 * If `level` is an invalid value, nothing will be done.
 */
void log_set_level(GLogLevelFlags level);

/**
 * Set the current loglevel to `level`
 *
 * @param level The desired log level as a string
 *
 * If `level` is `NULL`, nothing will be done.
 * If `level` is an invalid value, nothing will be done.
 */
void log_set_level_from_string(const char* level);

/**
 * Initialise log handling. Can be called any time.
 *
 * @param testing If we're in testing mode and should
 *                suppress all output
 */
void dunst_log_init(bool testing);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

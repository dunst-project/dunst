/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */

#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef DUNST_LOG_H
#define DUNST_LOG_H

/**
 * Prefix message with "[<source path>:<function name>:<line number>] "
 *
 * @param format is either a format string like the first argument
 * of printf() or a string literal.
 * @... are the arguments to above format string.
 *
 * This requires -Wno-gnu-zero-variadic-macro-arguments with clang
 * because __VA_OPTS__ seems not to be officially supported yet.
 * However, the result is the same with both gcc and clang.
 */
#if __GNUC__ >= 8

#define MSG(format, ...) "[%s:%s:%04d] " format, __FILE__, __func__, __LINE__, ## __VA_ARGS__

// These should benefit from more context
#define LOG_E(...) g_error(MSG(__VA_ARGS__))
#define LOG_C(...) g_critical(MSG(__VA_ARGS__))
#define LOG_D(...) g_debug(MSG(__VA_ARGS__))

#else

#define LOG_E g_error
#define LOG_C g_critical
#define LOG_D g_debug

#endif

#define LOG_W g_warning
#define LOG_M g_message
#define LOG_I g_info

#define DIE(...) do { LOG_C(__VA_ARGS__); exit(EXIT_FAILURE); } while (0)

// unified fopen() result messages
#define MSG_FOPEN_SUCCESS(path, fp) "'%s' open, fd: '%d'", path, fileno(fp)
#define MSG_FOPEN_FAILURE(path) "Cannot open '%s': '%s'", path, strerror(errno)

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
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

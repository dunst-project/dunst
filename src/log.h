/* SPDX-License-Identifier: BSD-3-Clause */
/**
 * @file
 * @ingroup utils
 * @brief Logging subsystem and helpers.
 * @copyright Copyright 2013-2014 Sascha Kruse
 * @copyright Copyright 2014-2026 Dunst contributors
 * @license BSD-3-Clause
 */

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
 * because of token pasting ',' and %__VA_ARGS__ being a GNU extension.
 * However, the result is the same with both gcc and clang and since we are
 * compiling with '-std=gnu99', this should be fine.
 */
#if __GNUC__ >= 8 || __clang_major__ >= 6
#define MSG(format, ...) "[%16s:%04d] " format, __func__, __LINE__, ## __VA_ARGS__
#endif

#ifdef MSG
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

// Unified fopen() result messages
#define MSG_FOPEN_SUCCESS(path, fp) "Opened '%s' (fd: '%d')", path, fileno(fp)
#define MSG_FOPEN_FAILURE(path) "Cannot open '%s': %s", path, strerror(errno)

enum log_mask {
        DUNST_LOG_NONE,
        DUNST_LOG_ALL,
        DUNST_LOG_AUTO,
};

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
 * @param mask If we're in testing mode and should
 *                suppress all output or show all
 */
void dunst_log_init(enum log_mask mask);

#endif

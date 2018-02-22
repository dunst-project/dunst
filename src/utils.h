/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_UTILS_H
#define DUNST_UTILS_H

#include <glib.h>
#include <stdbool.h>

/* replace all occurrences of the character needle with the character replacement in haystack */
char *string_replace_char(char needle, char replacement, char *haystack);

/* replace all occurrences of needle with replacement in haystack */
char *string_replace_all(const char *needle, const char *replacement, char *haystack);

/* replace <len> characters with <repl> at position <pos> of the string <buf> */
char *string_replace_at(char *buf, int pos, int len, const char *repl);

/* replace needle with replacement in haystack */
char *string_replace(const char *needle, const char *replacement, char *haystack);

char *string_append(char *a, const char *b, const char *sep);

/* strip content between two delimiter characters (inplace) */
void string_strip_delimited(char *str, char a, char b);

/**
 * Expand leading tilde in a string with contents of `$HOME`
 *
 * Will only copy the string and return its copy, if no tilde found.
 * Free the returned string after use again with `g_free`.
 *
 * @param string the path to expand
 * @return The string with an expaned beginning tilde
 */
char *string_to_path(const char *string);

/**
 * Convert time strings to internal gint64 microseconds
 *
 * The string can contain any number with a suffixed unit of time.
 * Possible units are: `ms`, `s`, `m`, `h` and `d`
 *
 * If no timeunit given, defaults to `s`.
 *
 * @param string the string representation of a number with suffixed timeunit
 * @param def value to return in case of errors
 *
 * @return the `gint64` representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
gint64 string_to_time(const char *string, gint64 def);

/**
 * Get the current monotonic time. In contrast to `g_get_monotonic_time`,
 * this function respects the real monotonic time of the system and
 * counts onwards during system sleep.
 *
 * @returns: A `gint64` monotonic time representation
 */
gint64 time_monotonic_now(void);

/**
 * Convert string to boolean value
 *
 * Booleans of Yes/No, True/False and 0/1 are understood
 *
 * @param string the string representation of the boolean
 * @param def value to return in case of errors
 *
 * @return the `bool` representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
bool string_parse_bool(const char *string, bool def);

/**
 * Convert string to integer value
 *
 * @param string the string representation of the integer
 * @param def value to return in case of errors
 *
 * @return the `int` representation of `string`
 * @return `def` if `string` is invalid or `NULL`
 */
int string_parse_int(const char *string, int def);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

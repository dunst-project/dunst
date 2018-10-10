/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_UTILS_H
#define DUNST_UTILS_H

#include <glib.h>

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
 * Strip quotes from a string. Won't touch inner quotes.
 *
 * @param value The string to strip the quotes from
 * @returns A copy of the string value with the outer quotes removed (if any)
 */
char *string_strip_quotes(const char *value);

/* replace tilde and path-specific values with its equivalents */
char *string_to_path(char *string);

/* convert time units (ms, s, m) to internal gint64 microseconds */
gint64 string_to_time(const char *string);

/**
 * Get the current monotonic time. In contrast to `g_get_monotonic_time`,
 * this function respects the real monotonic time of the system and
 * counts onwards during system sleep.
 *
 * @returns: A `gint64` monotonic time representation
 */
gint64 time_monotonic_now(void);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

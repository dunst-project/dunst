/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_UTILS_H
#define DUNST_UTILS_H

#include <glib.h>
#include <string.h>

//! Test if a string is NULL or empty
#define STR_EMPTY(s) (!s || (*s == '\0'))
//! Test if a string is non-NULL and not empty
#define STR_FULL(s) !(STR_EMPTY(s))
//! Test if string a and b contain the same chars
#define STR_EQ(a, b) (g_strcmp0(a, b) == 0)
//! Test if string a and b are same up to n chars
#define STRN_EQ(a, b, n) (strncmp(a, b, n) == 0)
//! Test if string a and b are the same case-insensitively
#define STR_CASEQ(a, b) (strcasecmp(a, b) == 0)

//! Assert that expr evaluates to true, if not return with val
#define ASSERT_OR_RET(expr, val) if (!(expr)) return val;

//! Convert a second into the internal time representation
#define S2US(s) (((gint64)(s)) * 1000 * 1000)

/**
 * Replaces all occurrences of the char \p needle with the char \p replacement in \p haystack.
 *
 * Does not allocate a new string.
 *
 * @param needle The char to replace with replacement
 * @param replacement The char which is the new one
 * @param haystack (nullable) The string to replace
 *
 * @returns The exact value of the haystack paramater (to allow nesting)
 */
char *string_replace_char(char needle, char replacement, char *haystack);

/**
 * Replace a substring inside a string with another string.
 *
 * May reallocate memory. Free the result with `g_free`.
 *
 * @param buf The string to replace
 * @param pos The position of the substring to replace
 * @param len The length of the substring to replace
 * @param repl The new contents of the substring.
 */
char *string_replace_at(char *buf, int pos, int len, const char *repl);

/**
 * Replace all occurences of a substring.
 *
 * @param needle The substring to search
 * @param replacement The substring to replace
 * @param haystack (nullable) The string to search the substring for
 */
char *string_replace_all(const char *needle, const char *replacement, char *haystack);

/**
 * Append \p b to string \p a. And concatenate both strings with \p concat, if both are non-empty.
 *
 * @param a (nullable) The left side of the string
 * @param b (nullable) The right side of the string
 * @param sep (nullable) The concatenator to concatenate if a and b given
 */
char *string_append(char *a, const char *b, const char *sep);

/**
 * Strip quotes from a string. Won't touch inner quotes.
 *
 * @param value The string to strip the quotes from
 * @returns A copy of the string value with the outer quotes removed (if any)
 */
char *string_strip_quotes(const char *value);

/**
 * Strip content between two delimiter characters
 *
 * @param str The string to operate in place
 * @param a Starting delimiter
 * @param b Ending delimiter
 */
void string_strip_delimited(char *str, char a, char b);

/**
 * Replace tilde and path-specific values with its equivalents
 *
 * The string gets invalidated. The new valid representation is the return value.
 *
 * @param string (nullable) The string to convert to a path.
 * @returns The tilde-replaced string.
 */
char *string_to_path(char *string);

/**
 * Convert time units (ms, s, m) to the internal `gint64` microseconds format
 *
 * If no unit is given, 's' (seconds) is assumed by default.
 *
 * @param string The string to parse the time format from.
 */
gint64 string_to_time(const char *string);

/**
 * Get the current monotonic time. In contrast to `g_get_monotonic_time`,
 * this function respects the real monotonic time of the system and
 * counts onwards during system sleep.
 *
 * @returns: A `gint64` monotonic time representation
 */
gint64 time_monotonic_now(void);

/**
 * Retrieve the HOME directory of the user running dunst
 *
 * @returns: A string of the current home directory
 */
const char *user_get_home(void);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

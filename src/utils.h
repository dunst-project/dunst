/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_UTILS_H
#define DUNST_UTILS_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
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
//! Get a non null string from a possibly null one
#define STR_NN(s) (s == NULL ? "(null)" : s)

//! Stringify the given expression or macro
#define STR_TO(...) _STR_TO(__VA_ARGS__)
#define _STR_TO(...) "" # __VA_ARGS__

//! Make a gboolean from a boolean value
// See https://github.com/dunst-project/dunst/issues/1421
#define BOOL2G(x) ((x) ? TRUE : FALSE)

//! Assert that expr evaluates to true, if not return with val
#define ASSERT_OR_RET(expr, val) if (!(expr)) return val;

//! Convert a second into the internal time representation
#define S2US(s) (((gint64)(s)) * G_USEC_PER_SEC)
#define US2S(s) (((gint64)(s)) / G_USEC_PER_SEC)

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
 * Parse a string into a dynamic array of tokens, using the delimiter string.
 *
 * The string is split on the separator character and strips spaces from
 * tokens. The last element of the array is NULL in order to avoid passing
 * around a length variable.
 *
 * @param string The string to convert to an array
 * @param delimiter The character that separates list entries
 * @returns The array of tokens owned by the caller. Free with g_strfreev.
 */
char **string_to_array(const char *string, const char *delimiter);

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
 * Convert string to int in a safe way. When there is an error the int is not
 * changed and false is returned.
 *
 * @param[out] in The int to save the result in. This is not changed when the
 * parsing does not succeed
 *
 * @param[in] str The string to parse
 * @return a bool if the conversion succeeded
 */
bool safe_string_to_int(int *in, const char *str);

/**
 * Same as safe_string_to_int, but then for a long
 */
bool safe_string_to_long_long(long long *in, const char *str);

/**
 * Same as safe_string_to_int, but then for a double
 */
bool safe_string_to_double(double *in, const char *str);

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

/**
 * Try to set an environment variable safely. If an environment variable with
 * name `key` exists, it will be overwritten.
 * If `value` is null, `key` will be set to an empty string.
 *
 * @param key (nullable) The environment variable to change
 * @param value (nullable) The value to change it to.
 * @returns: A bool that is true when it succeeds
 */
bool safe_setenv(const char* key, const char* value);

/**
 * Some sections are handled differently in dunst. This function tells wether a
 * sections is such a special section.
 *
 * @param s The name of the section
 * @returns A bool wether this section is one of the special sections
 */
bool is_special_section(const char* s);

/**
 * This function tells if a section is deprecated.
 *
 * @param s The name of the section
 * @returns A bool wether this section is deprecated
 */
bool is_deprecated_section(const char* s);

const char *get_section_deprecation_message(const char *s);


/**
 * Strips a string of it's brackets if the first and last character are a
 * bracket. Returns NULL on error.
 *
 * @param s String to strip
 * @returns Newly allocated string without the brackets, or NULL.
 */
char *string_strip_brackets(const char* s);


/**
 * Returns the length of a string array, -1 if the input is NULL.
 */
int string_array_length(char **s);

/** @brief Check if file is readable
 *
 * This is a convenience function to check if @p path can be resolved and makes
 * sense to try and open, like regular files and FIFOs (named pipes). Finally,
 * a preliminary check is done to see if read permission is granted.
 *
 * Do not rely too hard on the result, though, since this is racy. A case can
 * be made that these conditions might not be true anymore by the time the file
 * is acutally opened for reading.
 *
 * Also, no tilde expansion is done. Use the result of `string_to_path()` for
 * @p path.
 *
 * @param path [in] A string representing a path.
 * @retval true in case of success.
 * @retval false in case of failure, errno will be set appropriately.
 */
bool is_readable_file(const char * const path);

/** @brief Open files verbosely.
 *
 * This is a wrapper around fopen() and is_readable_file() that does some
 * preliminary checks and sends log messages.
 *
 * @p path [in] A char* string representing a filesystem path
 * @returns The result of the fopen() call.
 * @retval NULL if the fopen() call failed or @p path does not satisfy the
 * conditions of is_readable_file().
 */
FILE * fopen_verbose(const char * const path);

/**
 * Adds the contents of env_name with subdir to the array, interpreting the
 * environment variable as a colon-separated list of paths. If the environment
 * variable doesn't exist, alternative is used.
 *
 * @param arr The array to add to. This array has to be created first with
 *            g_ptr_array_new() or a similar function.
 * @param env_name The name of the environment variable that contains a
 * colon-separated list of paths.
 * @param subdir The subdirectory to add a the end of each path in env_name
 * @param alternative A colon-separated list of paths to use as alternative
 * when the environment variable doesn't exits.
 */
void add_paths_from_env(GPtrArray *arr, char *env_name, char *subdir, char *alternative);

bool string_is_int(const char *str);

#endif
/* vim: set ft=c tabstop=8 shiftwidth=8 expandtab textwidth=0: */

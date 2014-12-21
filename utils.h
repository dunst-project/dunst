/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef UTIL_H
#define UTIL_H

/* replace all occurences of the character needle with the character replacement in haystack */
char *string_replace_char(char needle, char replacement, char *haystack);

/* replace all occurrences of needle with replacement in haystack */
char *string_replace_all(const char *needle, const char *replacement,
                         char *haystack);

/* replace needle with replacement in haystack */
char *string_replace(const char *needle, const char *replacement,
                     char *haystack);

char *string_append(char *a, const char *b, const char *sep);

char **string_to_argv(const char *s);

/* strip content between two delimiter characters (inplace) */
void string_strip_delimited(char *str, char a, char b);

/* exit with an error message */
void die(char *msg, int exit_value);

int digit_count(int i);
#endif

/* vim: set ts=8 sw=8 tw=0: */

/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#ifndef DUNST_UTILS_H
#define DUNST_UTILS_H

/* replace all occurences of the character needle with the character replacement in haystack */
char *string_replace_char(char needle, char replacement, char *haystack);

/* replace all occurrences of needle with replacement in haystack */
char *string_replace_all(const char *needle, const char *replacement,
                         char *haystack);

/* replace <len> characters with <repl> at position <pos> of the string <buf> */
char *string_replace_at(char *buf, int pos, int len, const char *repl);

/* replace needle with replacement in haystack */
char *string_replace(const char *needle, const char *replacement,
                     char *haystack);

char *string_append(char *a, const char *b, const char *sep);

/* strip content between two delimiter characters (inplace) */
void string_strip_delimited(char *str, char a, char b);

/* exit with an error message */
void die(char *msg, int exit_value);

/* replace tilde and path-specific values with its equivalents */
char *string_to_path(char *string);

#endif
/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

#ifndef UTIL_H
#define UTIL_H
/* remove spaces before and after str */
char *rstrip(char *str);
char *lskip(char *str);

/* replace needle with replacement in haystack */
char *string_replace(const char *needle, const char *replacement,
                     char *haystack);

char *string_append(char *a, const char *b, const char *sep);

char **string_to_argv(const char *s);

/* exit with an error message */
void die(char *msg, int exit_value);

int digit_count(int i);

int sasprintf(char **strp, const char *fmt, ...);
#endif

/* vim: set ts=8 sw=8 tw=0: */

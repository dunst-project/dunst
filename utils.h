#ifndef UTIL_H
#define UTIL_H
/* remove spaces before and after str */
char *strtrim(char *str);

/* replace needle with replacement in haystack */
char *string_replace(const char *needle, const char *replacement,
                     char *haystack);

/* exit with an error message */
void die(char * msg, int exit_value);

/* print depending on verbosity */
void dunst_printf(int level, const char *fmt, ...);

int digit_count(int i);

#endif

/* vim: set ts=8 sw=8 tw=0: */

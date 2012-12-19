#define _GNU_SOURCE

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"
#include "dunst.h"

char *rstrip(char *str)
{
        char *end;
        end = str + strlen(str) - 1;
        while (isspace(*end)) {
                *end = '\0';
                end--;
        }
        return str;
}

char *lskip(char *s)
{
        for (; *s && isspace(*s); s++) ;
        return s;
}

char *string_replace(const char *needle, const char *replacement,
                     char *haystack)
{
        char *tmp, *start;
        int size;
        start = strstr(haystack, needle);
        if (start == NULL) {
                return haystack;
        }

        size = (strlen(haystack) - strlen(needle)) + strlen(replacement) + 1;
        tmp = calloc(sizeof(char), size);
        memset(tmp, '\0', size);

        strncpy(tmp, haystack, start - haystack);
        tmp[start - haystack] = '\0';

        sprintf(tmp + strlen(tmp), "%s%s", replacement, start + strlen(needle));
        free(haystack);

        return tmp;
}

char *string_append(char *a, const char *b, const char *sep)
{
        if (!a)
                return strdup(b);

        char *new;
        if (!sep)
                sasprintf(&new, "%s%s", a, b);
        else
                sasprintf(&new, "%s%s%s", a, sep, b);
        free(a);

        return new;

}

char **string_to_argv(const char *s)
{
        char *str = strdup(s);
        char **argv = NULL;
        char *p = strtok (str, " ");
        int n_spaces = 0;

        while (p) {
                argv = realloc (argv, sizeof (char*) * ++n_spaces);
                argv[n_spaces-1] = p;
                p = strtok (NULL, " ");
        }
        argv = realloc (argv, sizeof (char*) * (n_spaces+1));
        argv[n_spaces] = NULL;

        return argv;
}

int digit_count(int i)
{
        int len = 0;
        if (i == 0) {
                return 1;
        }

        if (i < 0) {
                len++;
                i *= -1;
        }

        while (i > 0) {
                len++;
                i /= 10;
        }

        return len;
}

void die(char *text, int exit_value)
{
        fputs(text, stderr);
        exit(exit_value);
}

int sasprintf(char **strp, const char *fmt, ...)
{
        va_list args;
        int result;

        va_start(args, fmt);
        if ((result = vasprintf(strp, fmt, args)) == -1)
                die("asprintf failed", EXIT_FAILURE);
        va_end(args);
        return result;
}

/* vim: set ts=8 sw=8 tw=0: */

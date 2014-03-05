/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#define _GNU_SOURCE

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "utils.h"
#include "dunst.h"

char *string_replace_char(char needle, char replacement, char *haystack) {
    char *current = haystack;
    while ((current = strchr (current, needle)) != NULL)
        *current++ = replacement;
    return haystack;
}

char *string_replace_all(const char *needle, const char *replacement,
                         char *haystack)
{
        char *start;
        start = strstr(haystack, needle);
        while (start != NULL) {
                haystack = string_replace(needle, replacement, haystack);
                start = strstr(haystack, needle);
        }
        return haystack;
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
                return g_strdup(b);

        char *new;
        if (!sep)
                new = g_strconcat(a, b, NULL);
        else
                new = g_strconcat(a, sep, b, NULL);
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

        free(str);

        return argv;
}

int digit_count(int i)
{
        i = ABS(i);
        int len = 1;

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

/* vim: set ts=8 sw=8 tw=0: */

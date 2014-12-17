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

char *string_replace_at(char *buf, int pos, int len, const char *repl)
{
        char *tmp;
        int size, buf_len, repl_len;

        buf_len = strlen(buf);
        repl_len = strlen(repl);
        size = (buf_len - len) + repl_len + 1;
        tmp = malloc(size);

        memcpy(tmp, buf, pos);
        memcpy(tmp + pos, repl, repl_len);
        memcpy(tmp + pos + repl_len, buf + pos + len, buf_len - (pos + len) + 1);

        free(buf);
        return tmp;
}

char *string_replace(const char *needle, const char *replacement, char *haystack)
{
        char *start;
        start = strstr(haystack, needle);
        if (start == NULL) {
                return haystack;
        }

        return string_replace_at(haystack, (start - haystack), strlen(needle), replacement);
}

char *string_replace_all(const char *needle, const char *replacement,
    char *haystack)
{
        char *start;
        int needle_pos;
        int needle_len, repl_len;

        needle_len = strlen(needle);
        if (needle_len == 0) {
                return haystack;
        }

        start = strstr(haystack, needle);
        repl_len = strlen(replacement);

        while (start != NULL) {
                needle_pos = start - haystack;
                haystack = string_replace_at(haystack, needle_pos, needle_len, replacement);
                start = strstr(haystack + needle_pos + repl_len, needle);
        }
        return haystack;
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
                argv[n_spaces-1] = g_strdup(p);
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

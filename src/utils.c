/* copyright 2013 Sascha Kruse and contributors (see LICENSE for licensing information) */
#define _GNU_SOURCE
#include "utils.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

        if (repl_len <= len) {
                tmp = buf;
        } else {
                tmp = g_malloc(size);
        }

        memcpy(tmp, buf, pos);
        memcpy(tmp + pos, repl, repl_len);
        memmove(tmp + pos + repl_len, buf + pos + len, buf_len - (pos + len) + 1);

        if(tmp != buf) {
                g_free(buf);
        }

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
        g_free(a);

        return new;

}

void string_strip_delimited(char *str, char a, char b)
{
        int iread=-1, iwrite=0, copen=0;
        while (str[++iread] != 0) {
                if (str[iread] == a) {
                        ++copen;
                } else if (str[iread] == b && copen > 0) {
                        --copen;
                } else if (copen == 0) {
                        str[iwrite++] = str[iread];
                }
        }
        str[iwrite] = 0;
}

char* string_to_path(char* string) {

        if (string && 0 == strncmp(string, "~/", 2)) {
                char* home = g_strconcat(getenv("HOME"), "/", NULL);

                string = string_replace("~/", home, string);

                g_free(home);
        }

        return string;
}

void die(char *text, int exit_value)
{
        fputs(text, stderr);
        exit(exit_value);
}

/* vim: set tabstop=8 shiftwidth=8 expandtab textwidth=0: */

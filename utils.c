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
        for(; *s && isspace(*s); s++);
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

        if (strstr(tmp, needle)) {
                return string_replace(needle, replacement, tmp);
        } else {
                return tmp;
        }
}

int digit_count(int i)
{
        int len = 0;
        if ( i == 0) {
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

/* vim: set ts=8 sw=8 tw=0: */

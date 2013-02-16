#include "stdlib.h"
#include "stdio.h"
#include <string.h>

#include "container.h"


str_array *str_array_malloc(void)
{
        str_array *a = malloc(sizeof(str_array));
        a->count = 0;
        a->strs = NULL;
        return a;
}

void str_array_append(str_array *a, char *str)
{
        if (!a)
                return;
        a->count++;
        a->strs = realloc(a->strs, a->count * sizeof(char *));
        (a->strs)[a->count-1] = str;
}

void str_array_dup_append(str_array *a, const char *str)
{
        if (!a)
                return;
        char *dup = strdup(str);
        str_array_append(a, dup);
}

void str_array_deep_free(str_array *a)
{
        if (!a)
                return;
        for (int i = 0; i < a->count; i++) {
                free((a->strs)[i]);
        }
        free(a);
}

/* vim: set ts=8 sw=8 tw=0: */

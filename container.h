#ifndef _LIST_H
#define _LIST_H

#include "dunst.h"

typedef struct _str_array {
        int count;
        char **strs;
} str_array;

str_array *str_array_malloc(void);
void str_array_dup_append(str_array *a, const char *str);
void str_array_append(str_array *a, char *str);
void str_array_deep_free(str_array *a);

#endif
/* vim: set ts=8 sw=8 tw=0: */

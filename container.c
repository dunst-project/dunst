#include "stdlib.h"
#include "stdio.h"
#include <string.h>

#include "container.h"

void n_stack_push(n_stack **s, notification *n)
{
        if (!n || !s)
                return;

        n_stack *new = malloc(sizeof(n_stack));
        new->n = n;
        new->next = *s;
        *s = new;
}

notification *n_stack_pop(n_stack **s)
{
        if (!s || !*s)
                return NULL;

        n_stack *head = *s;
        *s = (*s)->next;

        notification *n = head->n;
        free(head);

        return n;
}

void n_stack_remove(n_stack **s, notification *n)
{
        if (!s || !*s || !n)
                return;

        /* head is n */
        if ((*s)->n == n) {
                n_stack *tmp = *s;
                *s = (*s)->next;
                free(tmp);
                return;
        }

        for (n_stack *iter = *s; iter->next; iter = iter->next) {
                if (iter->next->n == n) {
                        n_stack *tmp = iter->next;
                        iter->next = iter->next->next;
                        free(tmp);
                        return;
                }
        }
}

int n_stack_len(n_stack **s)
{
        if (!s || !*s)
                return 0;

        n_stack *cur = *s;
        int count = 0;

        while (cur) {
                cur = cur->next;
                count++;
        }

        return count;
}

int cmp_notification(notification *a, notification *b)
{
        if (a == NULL && b == NULL)
                return 0;
        else if (a == NULL)
                return -1;
        else if (b == NULL)
                return 1;

        if (a->urgency != b->urgency) {
                return a->urgency - b->urgency;
        } else {
                return b->timestamp - a->timestamp;
        }
}

void n_queue_enqueue(n_queue **q, notification *n)
{
        if (!n || !q)
                return;


        n_queue *new = malloc(sizeof(n_queue));
        new->n = n;
        new->next = NULL;

        if (!(*q)) {
                /* queue was empty */
                *q = new;
                return;
        }


        /* new head */
        if (cmp_notification(new->n, (*q)->n) > 0) {
                new->next = *q;
                *q = new;
                return;
        }

        /* in between */
        n_queue *cur = *q;
        while (cur->next) {
                if (cmp_notification(new->n, cur->next->n) > 0) {
                        new->next = cur->next;
                        cur->next = new;
                        return;
                }

                cur = cur->next;
        }

        /* last */
        cur->next = new;
}

void n_queue_remove(n_queue **q, notification *n)
{
        n_stack_remove(q, n);
}

notification *n_queue_dequeue(n_queue **q)
{
        return n_stack_pop(q);
}

int n_queue_len(n_queue **q)
{
        return n_stack_len(q);
}

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
        a->strs = realloc(a->strs, a->count);
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

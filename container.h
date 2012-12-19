#ifndef _LIST_H
#define _LIST_H

#include "dunst.h"

typedef struct _n_stack {
        notification *n;
        struct _n_stack *next;
} n_stack;

typedef n_stack n_queue;

typedef struct _str_array {
        int count;
        char **strs;
} str_array;

str_array *str_array_malloc(void);
void str_array_dup_append(str_array *a, const char *str);
void str_array_append(str_array *a, char *str);
void str_array_deep_free(str_array *a);

int cmp_notification(notification *a, notification *b);

/************
 * stack
 */

/**
 * push notification onto stack
 * creates a new stack if *s == NULL
 */
void n_stack_push(n_stack **s, notification *n);

/**
 * remove and return notification from stack
 * sets *s to NULL if stack is empty
 */
notification *n_stack_pop(n_stack **s);

/**
 * remove notification from stack
 */

void n_stack_remove(n_stack **q, notification *n);

/**
 * return length of stack
 */

int n_stack_len(n_stack **s);

/***************
 * queue
 */

/**
 * enqueue notification into queue
 * creates a new queue if *q == NULL
 */

void n_queue_enqueue(n_queue **q, notification *n);

/**
 * dequeues the next element from the queue.
 * returns NULL if queue is empty
 */

notification *n_queue_dequeue(n_queue **q);

/**
 * remove notification from queue
 */

void n_queue_remove(n_queue **q, notification *n);

/**
 * return length of queue
 */

int n_queue_len(n_queue **q);

#endif
/* vim: set ts=8 sw=8 tw=0: */

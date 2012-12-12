#ifndef _LIST_H
#define _LIST_H

#include "dunst.h"

typedef struct _l_node {
        struct _l_node *next;
        void *data;
} l_node;

typedef struct _list {
        l_node *head;
} list;

typedef struct _n_stack {
        notification *n;
        struct _n_stack *next;
} n_stack;

/* append to end of list */
int l_push(list * l, void *data);

/* same as append but with a l_node */
int l_node_push(list * l, l_node * n);

/* remove and return last element of list */
void *l_pop(list * l);

/* insert after node. */
int l_insert(l_node * node, void *data);

/*
 * same as insert, but using a node_t
 */
int l_node_insert(l_node * node, l_node * to_be_inserted);

/*
 * remove l_node from list and return a pointer to its data
 */
void *l_remove(list * l, l_node * node);

/*
 * same as l_remove but returns the node instead of the data
 */
l_node *l_node_remove(list * l, l_node * node);

/*
 * initialize a node
 */
l_node *l_init_node(void *data);

/* return the length of the list */
int l_length(list * l);

/* is list empty */
int l_is_empty(list * l);

/* move node from 'from' to 'to' */
int l_move(list * from, list * to, l_node * node);

void l_sort(list * l, int (*f) (void *, void *));

list *l_init(void);


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


#endif
/* vim: set ts=8 sw=8 tw=0: */

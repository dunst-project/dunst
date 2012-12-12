#include "stdlib.h"
#include "stdio.h"

#include "container.h"

int l_push(list * l, void *data)
{
        l_node *n;
        int ret;

        /* invalid input */
        if (!l || !data) {
                return -1;
        }

        n = l_init_node(data);
        if (!n) {
                /* something went wrong */
                return -2;
        }

        ret = l_node_push(l, n);

        return ret;
}

int l_node_push(list * l, l_node * n)
{
        l_node *end;

        /* invalid input */
        if (!l || !n) {
                return -1;
        }

        n->next = NULL;

        /* empty list */
        if (!l->head) {
                l->head = n;

                return 0;
        }

        for (end = l->head; end->next; end = end->next) ;

        if (end != n) {
                end->next = n;
        }

        return 0;
}

void *l_pop(list * l)
{
        l_node *penultimate;
        void *data;

        /* invalid input */
        if (!l) {
                return NULL;
        }

        /* empty list */
        if (!l->head) {
                return NULL;
        }

        /* len(l) == 1 */
        if (!l->head->next) {
                data = l->head->data;
                free(l->head);
                l->head = NULL;

                return data;
        }

        for (penultimate = l->head;
             penultimate->next->next; penultimate = penultimate->next) ;
        data = penultimate->next->data;
        free(penultimate->next);
        penultimate->next = NULL;

        return data;
}

int l_insert(l_node * node, void *data)
{
        int ret;
        l_node *to_be_inserted;

        /* invalid input */
        if (!node || !data) {
                return -1;
        }

        to_be_inserted = l_init_node(data);
        if (!to_be_inserted) {
                return -2;
        }

        ret = l_node_insert(node, to_be_inserted);

        return ret;
}

int l_node_insert(l_node * node, l_node * to_be_inserted)
{
        l_node *tmp;

        /* invalid input */
        if (!node || !to_be_inserted) {
                return -1;
        }

        tmp = node->next;
        node->next = to_be_inserted;
        to_be_inserted->next = tmp;

        return 0;
}

void *l_remove(list * l, l_node * node)
{
        void *data;
        if (l != NULL) {
                l_node_remove(l, node);
        }

        if (node == NULL) {
                return NULL;
        }

        data = node->data;
        free(node);

        return data;
}

l_node *l_node_remove(list * l, l_node * node)
{
        l_node *prev;
        l_node *next;

        /* invalid input */
        if (!l || !node) {
                return NULL;
        }

        /* empty list */
        if (!l->head) {
                return NULL;
        }

        /* node is head */
        if (l->head == node) {
                l->head = node->next;
                node->next = NULL;
                return node;
        }

        /* find the predecessor of node */
        for (prev = l->head;
             prev->next && prev->next != node; prev = prev->next) ;

        /* node not in list */
        if (prev->next != node) {
                return node;
        }

        next = node->next;
        prev->next = next;

        return node;
}

l_node *l_init_node(void *data)
{
        l_node *node;

        node = malloc(sizeof(l_node));
        if (!node) {
                return NULL;
        }

        node->data = data;
        node->next = NULL;

        return node;
}

int l_length(list * l)
{
        l_node *iter;
        int count;

        if (!l || !l->head) {
                return 0;
        }

        count = 0;
        iter = l->head;
        while (iter) {
                count++;
                iter = iter->next;
        }

        return count;
}

int l_is_empty(list * l)
{
        return l->head == NULL;
}

int l_move(list * from, list * to, l_node * node)
{

        if (!from || !to || !node) {
                return -1;
        }
        node = l_node_remove(from, node);
        return l_node_push(to, node);
}

list *l_init(void)
{
        list *l = malloc(sizeof(list));
        l->head = NULL;

        return l;
}

void l_sort(list * l, int (*f) (void *, void *))
{
        list *old_list;

        if (!l || l_length(l) < 2) {
                /* nothing to do */
                return;
        }

        old_list = l_init();

        old_list->head = l->head;
        l->head = NULL;

        while (!l_is_empty(old_list)) {
                l_node *iter;
                l_node *max;

                /* find max in old_list */
                max = old_list->head;
                for (iter = old_list->head; iter; iter = iter->next) {
                        if (f(max->data, iter->data) < 0) {
                                max = iter;
                        }
                }

                l_move(old_list, l, max);
        }

        free(old_list);
}

/* vim: set ts=8 sw=8 tw=0: */

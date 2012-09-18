#ifndef _LLIST_H
#define _LLIST_H

#include "bitmap.h"

typedef struct
{
    int value;
} object;

typedef struct node_struct
{
    layer_t *value;
    struct node_struct *next;
    struct node_struct *prev;
} node;

typedef struct
{
    node *head;
} llist;

llist *new_list();
void add_node(llist *p, layer_t *layer);
void llist_exchange(llist *p, node *n);
void move_down(llist *p, layer_t *layer);
void move_up(llist *p, layer_t *layer);
void print_llist(llist *p);

#endif

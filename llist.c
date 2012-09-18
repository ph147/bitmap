#include <stdio.h>
#include <stdlib.h>
#include "bitmap.h"
#include "llist.h"

llist *new_list()
{
    llist *p = malloc(sizeof(llist));
    p->head = NULL;
    return p;
}

void add_node(llist *p, layer_t *layer)
{
    node *head = malloc(sizeof(node));
    head->value = layer;
    head->next = p->head;
    head->prev = NULL;
    if (p->head != NULL)
        p->head->prev = head;
    p->head = head;
}

void llist_exchange(llist *p, node *n)
{
    node *next_node = n->next;
    node *prev_node = n->prev;

    if (next_node == NULL)
        return;

    n->next = next_node->next;
    n->prev = next_node;

    next_node->next = n;
    next_node->prev = prev_node;

    if (prev_node != NULL)
        prev_node->next = next_node;
    if (p->head == n)
        p->head = next_node;
}

void move_down(llist *p, layer_t *layer)
{
    node *tmp = p->head;

    while (tmp != NULL)
    {
        if (tmp->value == layer)
        {
            if (tmp->next == NULL)
                break;
            llist_exchange(p, tmp);
            break;
        }
        tmp = tmp->next;
    }
}

void move_up(llist *p, layer_t *layer)
{
    node *tmp = p->head;

    while (tmp != NULL)
    {
        if (tmp->value == layer)
        {
            if (tmp->prev == NULL)
                break;
            llist_exchange(p, tmp->prev);
            break;
        }
        tmp = tmp->next;
    }
}

void print_llist(llist *p)
{
    node *tmp = p->head;

    while (tmp != NULL)
    {
        layer_t *layer = (layer_t *) tmp->value;
        //printf("%d\n", layer->value);
        tmp = tmp->next;
    }
}

/*
int main()
{
    llist *l = new_list();

    object obj1 = { 5 };
    object obj2 = { 4 };
    object obj3 = { 3 };
    object obj4 = { 2 };
    object obj5 = { 1 };

    add_node(l, &obj1);
    add_node(l, &obj2);
    add_node(l, &obj3);
    add_node(l, &obj4);

    print_llist(l);
    printf("---\n");
    move_up(l, &obj1);
    print_llist(l);
}
*/

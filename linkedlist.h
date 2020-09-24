#ifndef LINKEDLIST_H
#define LINKEDLIST_H

typedef struct node {
    struct node *next;
    struct node *prev;
    struct packet *packet;
    time_t time_sent;
} node_t;

typedef struct linkedlist {
    int size;
    struct node *start;
    struct node *end;
} linkedlist_t;

void set_between(node_t *left, node_t *middle, node_t *right);
void remove_node(node_t *node);
void push(linkedlist_t *list, struct packet *packet, time_t time_sent);
void pop(linkedlist_t *list);

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "linkedlist.h"
#include "client.h"

/*
    Setter en node mellom to andre noder.
*/
void set_between(node_t *left, node_t *middle, node_t *right) {
    left->next = middle;
    right->prev = middle;
    middle->next = right;
    middle->prev = left;
}

/*
    Fjerner en node fra listen.
    OBS: frier ikke nodene den fjerner.
*/
void remove_node(node_t *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

/*
    Legger til noder i slutten av listen. FIFO.
*/
void push(linkedlist_t *list, packet_t *packet, time_t time) {
    node_t *node = malloc(sizeof(node_t));
    node->packet = packet;
    node->time_sent = time;
    set_between(list->end->prev, node, list->end);
    list->size++;
}

/*
    Fjerner den første noden i listen. FIFO. 
*/
void pop(linkedlist_t *list) {
    node_t *temp = list->start->next;
    packet_t *packet = temp->packet;
    remove_node(list->start->next);
    list->size--;
    free(temp);
}


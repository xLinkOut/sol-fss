// @author Luca Cirillo (545480)

// * Generic Linked List, supporta qualsiasi tipologia di dato all'interno dei nodi,
// *  l'inserimento avviene in coda e la rimozione scorre la lista partendo dalla testa.
// *  Inoltre, le funzioni di push e pop permettono l'utilizzo della lista come una queue con politica FIFO

#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include <stddef.h>

// * Struttura dati di un nodo della lista
typedef struct Node {
    void* data;
    struct Node* next;
} list_node_t;

// * Struttura dati della lista
typedef struct LinkedList {
    list_node_t* first;
    list_node_t* last;
} linked_list_t;

// * Crea un nuovo nodo in memoria che conterrà <data>
// Ritorna un puntatore al nodo in caso di successo, NULL altrimenti
list_node_t* llist_node_create(const void* data, size_t size);

// * Cancella dalla memoria un nodo creato con llist_node_create
void llist_node_destroy(list_node_t* node);

#endif

// @author Luca Cirillo (545480)

// * Generic Linked List, supporta qualsiasi tipologia di dato all'interno dei nodi,
// *  l'inserimento avviene in coda e la rimozione scorre la lista partendo dalla testa.
// *  Inoltre, le funzioni di push e pop permettono l'utilizzo della lista come una queue con politica FIFO

#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include <stddef.h>
#include <stdbool.h>

// * Struttura dati di un nodo della lista
typedef struct Node {
    void* data;
    size_t data_size;
    struct Node* prev;
    struct Node* next;
} list_node_t;

// * Struttura dati della lista
typedef struct LinkedList {
    list_node_t* first;
    list_node_t* last;
} linked_list_t;

// * Crea un nuovo nodo in memoria che conterrà <data>
// Ritorna un puntatore al nodo in caso di successo, NULL altrimenti, setta errno
list_node_t* llist_node_create(const void* data, size_t size);

// * Cancella dalla memoria un nodo creato con llist_node_create
void llist_node_destroy(list_node_t* node);

// * Crea una nuova lista vuota
// Ritorna un puntatore alla lista in caso di successo, NULL altrimenti, setta errno
linked_list_t* llist_create(); // TODO: puntatore a funzione free nodi

// * Cancella dalla memoria una lista creata con llist_create, e tutti i suoi nodi
void llist_destroy(linked_list_t* llist);

// * Crea e aggiunge in testa un nuovo nodo contenente <data>
bool llist_push_first(linked_list_t* llist, const void* data, size_t size);

// * Rimuove la testa della lista e copia il contenuto del nodo in <data>
bool llist_pop_first(linked_list_t* llist, void** data);

#endif

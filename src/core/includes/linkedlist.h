// @author Luca Cirillo (545480)

// * Generic Linked List:
// *  Supporta qualsiasi tipologia di dato all'interno dei nodi, che vengono identificati da una chiave.
// *  Almeno un elemento tra chiave e dato deve essere specificato, meglio se entrambi; nulla toglie
// *   che la chiave possa essere utilizzata per memorizzare dati significativi per l'utente
// *  L'inserimento e la rimozione dei nodi può essere fatto in testa oppure in coda;
// *   in particolare è possibile rimuovere uno specifico nodo tramite la sua chiave.
// *  Per utilizzare la lista come una coda con politica FIFO, si possono sfruttare i metodi
// *   llist_push_last, che aggiunge in coda, e llist_pop_first, che rimuove dalla testa.

#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#include <stdbool.h>
#include <stddef.h>

// * Struttura dati di un nodo della lista
typedef struct Node {
    char* key;          // Chiave identificativa del nodo
    void* data;         // Dati contenuti nel nodo
    size_t data_size;   // Dimensione dei dati contenuti nel nodo
    struct Node* prev;  // Puntatore al nodo precedente
    struct Node* next;  // Puntatore al nodo successivo
} list_node_t;

// * Struttura dati della lista
typedef struct LinkedList {
    list_node_t* first;  // Testa della lista
    list_node_t* last;   // Coda della lista
    size_t length;       // Numero di elementi in lista
} linked_list_t;

// * Crea un nuovo nodo in memoria che conterrà <data>
// Ritorna un puntatore al nodo in caso di successo, NULL altrimenti, setta errno
list_node_t* llist_node_create(const char* key, const void* data, size_t size);

// * Cancella dalla memoria un nodo creato con llist_node_create
void llist_node_destroy(list_node_t* node);

// * Crea una nuova lista vuota
// Ritorna un puntatore alla lista in caso di successo, NULL altrimenti, setta errno
linked_list_t* llist_create();  // TODO: puntatore a funzione free nodi

// * Cancella dalla memoria una lista creata con llist_create, e tutti i suoi nodi
void llist_destroy(linked_list_t* llist);

// * Crea e aggiunge in testa un nuovo nodo contenente <data>
// Ritorna true in caso di successo, false altrimenti, setta errno
bool llist_push_first(linked_list_t* llist, const char* key, const void* data, size_t size);

// * Rimuove la testa della lista e copia il contenuto del nodo in <data>
// Ritorna true in caso di successo, false altrimenti, setta errno
bool llist_pop_first(linked_list_t* llist, char** key, void** data);

// * Crea e aggiunge in coda un nuovo nodo contenente <data>
// Ritorna true in caso di successo, false altrimenti, setta errno
bool llist_push_last(linked_list_t* llist, const char* key, const void* data, size_t size);

// * Rimuove la coda della lista e copia il contenuto del nodo in <data>
// Ritorna true in caso di successo, false altrimenti, setta errno
bool llist_pop_last(linked_list_t* llist, char** key, void** data);

// * Rimuove dalla lista il nodo corrispondente alla chiave <key>
// Ritorna true in caso di successo, false altrimenti, setta errno
bool llist_remove(linked_list_t* llist, const char* key);

// * Cerca nella lista il nodo corrispondente alla chiave <key>
// Ritorna true in caso di successo, false altrimenti, setta errno
bool llist_find(linked_list_t* llist, const char* key);

// * Visualizza a schermo i nodi presenti nella lista
void llist_print(linked_list_t* llist);

#endif

// @author Luca Cirillo (545480)

// * Linked List, con inserimento in coda e rimozione partendo dalla testa

#include <stdbool.h>
#include <stddef.h>

// * Struttura dati di un nodo della lista
typedef struct ListNode {
    int data;
    struct ListNode* next;  // Puntatore al nodo successivo
} list_node_t;

// * Struttura dati della lista
typedef struct LinkedList {
    list_node_t* first;  // Testa della lista
    list_node_t* last;   // Coda della lista
    size_t size;         // Numero di elementi nella lista
} linked_list_t;

// * Crea una nuova lista vuota
// Ritorna un puntatore alla lista in caso di successo, NULL in caso di fallimento, setta errno
linked_list_t* linked_list_create();

// * Cancella una lista creata con linked_list_create
void linked_list_destroy(linked_list_t* llist);

// * Inserisce un nuovo nodo in coda alla lista, che conterrà <data>
// Ritorna true in caso di successo, false in caso di fallimento, setta errno
bool linked_list_insert(linked_list_t* llist, int data);

// * Rimuove il nodo contenente <data> dalla lista, partendo dalla testa
// Ritorna true in caso di successo, false in caso di fallimento, setta errno
bool linked_list_remove(linked_list_t* llist, int data);

// * Cerca il nodo contenente <data> nella lista, partendo dalla testa
// Ritorna true in caso di successo, false in caso di fallimento, setta errno
bool linked_list_find(linked_list_t* llist, int data);

// * Visualizza la lista a schermo
void linked_list_print(linked_list_t* llist);

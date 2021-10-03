// @author Luca Cirillo (545480)

// ! Linked List

#include <errno.h>
#include <stddef.h>
#include <stdbool.h>

// * Specifica se effettuare le operazioni di Push e Pop dalla testa o dalla coda della lista
typedef enum FrontBack { FRONT, BACK } front_back;

// * Struttura dati un generico nodo della lista
typedef struct ListNode {
    int data;
    struct ListNode* prev;  // Puntatore al nodo precedente
    struct ListNode* next;  // Puntatore al nodo successivo
} node_t;

typedef struct LinkedList {
    node_t* first;  // Testa della lista
    node_t* last;   // Coda della lista
    size_t size;    // Numero di elementi nella lista
} linked_list_t;

// * Crea una lista vuota e ritorna un puntatore ad essa
linked_list_t* linked_list_create();

// * Cancella una lista creata con linked_list_create
void linked_list_destroy(linked_list_t* llist);

// * Inserisce un nodo nella lista, in testa o in coda in accordo a where
int linked_list_push(linked_list_t* llist, int data, front_back where);

// * Rimuove un nodo nella lista, dalla testa o dalla coda in accordo a from
int linked_list_pop(linked_list_t* llist, int* data, front_back from);

// * Cerca uno specifico nodo nella lista, ritorna true se lo trova, false altrimenti
bool linked_list_find(linked_list_t* llist, int data);

// * Visualizza la lista a schermo
void linked_list_print(linked_list_t* llist);

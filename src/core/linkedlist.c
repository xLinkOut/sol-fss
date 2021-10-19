// @author Luca Cirillo (545480)

// * Generic Linked List, supporta qualsiasi tipologia di dato all'interno dei nodi,
// *  l'inserimento avviene in coda e la rimozione scorre la lista partendo dalla testa.
// *  Inoltre, le funzioni di push e pop permettono l'utilizzo della lista come una queue con politica FIFO

#include <linkedlist.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

list_node_t* llist_node_create(const void* data, size_t size){
    // Controllo la validità degli argomenti
    // Se sono stati specificati dei dati con dimensione pari a zero,
    //  oppure nessun dato ma dimensione significativa, ritorno errore
    if((data && size == 0) || (!data && size > 0)){
        errno = EINVAL;
        return NULL;
    }

    // Alloco memoria per il nuovo nodo
    list_node_t* node = malloc(sizeof(list_node_t));
    if(!node) return NULL;

    // Se è stato specificato un contenuto da memorizzare
    if(data && size > 0){
        // Alloco la memoria necessaria
        node->data = malloc(size);
        // Copio i dati nel nodo
        memcpy(node->data, data, size);
    }else{
        // Altrimenti, inizializzo il puntatore a data
        node->data = NULL;
    }

    // Inizializzo il puntatore al nodo successivo
    node->next = NULL;

    // Quindi, ritorno un puntatore al nodo
    return node;
}

void llist_node_destroy(list_node_t* node){
    // Controllo la validità degli argomenti
    if(!node) return;
    // Libero la memoria occupata dai dati del nodo
    // TODO: Puntatore a funzione per liberare i dati del nodo
    if(node->data) free(node->data);
    free(node);
}

linked_list_t* llist_create(){
    // Alloco memoria per la lista
    linked_list_t* llist = malloc(sizeof(linked_list_t));
    if(!llist) return NULL;

    // Inizializzo i suoi dati
    llist->first = llist->last = NULL;

    // Ritorno un puntatore alla lista
    return llist;
}

void llist_destroy(linked_list_t* llist){
    // Controllo la validità degli argomenti
    if(!llist) return;
    // Scorro la lista per cancellare tutti i nodi
    list_node_t* current = NULL;
    while(llist->first != NULL){
        current = llist->first;
        llist->first = llist->first->next;
        free(current);
    }
    // Cancello la testa della lista
    if(llist->first) free(llist->first);
    // Cancello la lista
    free(llist);
}
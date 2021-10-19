// @author Luca Cirillo (545480)

// * Generic Linked List, supporta qualsiasi tipologia di dato all'interno dei nodi,
// *  l'inserimento avviene in coda e la rimozione scorre la lista partendo dalla testa.
// *  Inoltre, le funzioni di push e pop permettono l'utilizzo della lista come una queue con politica FIFO

#include <linkedlist.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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
        // Riporto nel nodo la dimensione
        node->data_size = size;
    }else{
        // Altrimenti, inizializzo il puntatore a data
        node->data = NULL;
        node->data_size = 0;
    }

    // Inizializzo i puntatori ai nodi precedente e successivo
    node->prev = NULL;
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
    llist->first = NULL;
    llist->last = NULL;
    llist->length = 0;

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

bool llist_push_first(linked_list_t* llist, const void* data, size_t size){
    // Controllo la validità degli argomenti
    if(!llist){
        errno = EINVAL;
        return false;
    }

    // Creo il nuovo nodo 
    list_node_t* new_node = llist_node_create(data, size);
    if(!new_node) return false;

    // Se la lista è vuota
    if(llist->length == 0){
        // Aggiorno sia il puntatore alla testa che alla coda
        llist->first = new_node;
        llist->last = new_node;
    }else{
        // Altrimenti, aggiunto in testa
        new_node->next = llist->first; // Il successore del nuovo nodo sarà l'attuale nodo in testa
        llist->first->prev = new_node; // Il predecessore dell'attuale nodo di testa sarà il nuovo nodo
        llist->first = new_node; // Il nuovo nodo ora sarà la testa della lista
    }

    // Incremento il contatore degli elementi presenti in lista
    llist->length++;

    return true;
}

bool llist_pop_first(linked_list_t* llist, void** data){
    // Controllo la validità degli argomenti
    if(!llist || !data){
        errno = EINVAL;
        return false;
    }

    // Controllo che la lista non sia vuota
    if(llist->length == 0){
        errno = ENOENT;
        return false;
    }

    // Tengo un riferimento al nodo da rimuovere
    list_node_t* node = llist->first;
    // Se il nodo non è vuoto
    if(node->data && node->data_size > 0){
        // Alloco la memoria per copiare i dati
        *data = malloc(node->data_size);
        // Copio i dati nel puntatore <data>
        memcpy(*data, node->data, node->data_size);
    }else{
        // Altrimenti imposto su NULL
        *data = NULL;
    }

    // Aggiorno la lista, rimuovendo il nodo di testa
    // Se la lista contiene solo il nodo corrente
    if(!node->next){
        llist->first = NULL;
        llist->last = NULL;
    }else{
        // Altrimenti, porto in testa il suo successore
        llist->first = node->next; // La nuova testa sarà il successore del nodo appena rimosso
        llist->first->prev = NULL; // Il predecessore della testa è ovviamente NULL
    }
    // Cancello il nodo
    llist_node_destroy(node);
    // Decremento il contatore degli elementi presenti in lista
    llist->length--;

    return true;
}

bool llist_push_last(linked_list_t* llist, const void* data, size_t size){
    // Controllo la validità degli argomenti
    if(!llist){
        errno = EINVAL;
        return false;
    }

    // Creo il nuovo nodo 
    list_node_t* new_node = llist_node_create(data, size);
    if(!new_node) return false;

    // Se la lista è vuota
    if(llist->length == 0){
        // Aggiorno sia il puntatore alla testa che alla coda
        llist->first = new_node;
        llist->last = new_node;
    }else{
        // Altrimenti, aggiungo in coda
        llist->last->next = new_node; // Il successore dell'attuale nodo in coda sarà il nuovo nodo
        new_node->prev = llist->last; // Il predecessore del nuovo nodo sarà l'attuale nodo in coda 
        llist->last = new_node;  // Il nuovo nodo ora sarà la coda della lista
    }

    // Incremento il contatore degli elementi presenti in lista
    llist->length++;

    return true;
}

bool llist_pop_last(linked_list_t* llist, void** data){
    // Controllo la validità degli argomenti
    if(!llist || !data){
        errno = EINVAL;
        return false;
    }

    // Controllo che la lista non sia vuota
    if(llist->length == 0){
        errno = ENOENT;
        return false;
    }

    // Tengo un riferimento al nodo da rimuovere
    list_node_t* node = llist->last;
    // Se il nodo non è vuoto
    if(node->data && node->data_size > 0){
        // Alloco la memoria per copiare i dati
        *data = malloc(node->data_size);
        // Copio i dati nel puntatore <data>
        memcpy(*data, node->data, node->data_size);
    }else{
        // Altrimenti imposto su NULL
        *data = NULL;
    }

    // Aggiorno la lista, rimuovendo il nodo di testa
    // Se la lista contiene solo il nodo corrente
    if(!node->prev){
        llist->first = NULL;
        llist->last = NULL;
    }else{
        // Altrimenti, aggiorno la coda della lista
        llist->last = node->prev; // La nuova coda sarà il predecessore del nodo appena rimosso
        llist->last->next = NULL; // Il successore della coda è ovviamente NULL
    }
    // Cancello il nodo
    llist_node_destroy(node);
    // Decremento il contatore degli elementi presenti in lista
    llist->length--;

    return true;
}
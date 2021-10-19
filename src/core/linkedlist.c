// @author Luca Cirillo (545480)

// * Generic Linked List, supporta qualsiasi tipologia di dato all'interno dei nodi,
// *  l'inserimento avviene in coda e la rimozione scorre la lista partendo dalla testa.
// *  Inoltre, le funzioni di push e pop permettono l'utilizzo della lista come una queue con politica FIFO

#include <linkedlist.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

list_node_t* llist_node_create(const char* key, const void* data, size_t size){
    // Controllo la validità degli argomenti
    // Se non è stato passato alcun dato, oppure
    //  sono stati specificati dei dati con dimensione pari a zero, oppure
    //  non è stato specificato alcun dato ma una dimensione significativa,
    //  ritorno errore
    if((!key && !data && size == 0) || (data && size == 0) || (!data && size > 0)){
        errno = EINVAL;
        return NULL;
    }

    // Alloco memoria per il nuovo nodo
    list_node_t* node = malloc(sizeof(list_node_t));
    if(!node) return NULL;

    // Se è stata specificata una chiave, la copio nel nodo
    if(key){
        size_t key_size = strlen(key);
        node->key = malloc(key_size + 1);
        memset(node->key, 0, key_size + 1);
        strncpy(node->key, key, key_size);
    }else{
        // Altrimenti, imposto il puntatore node->key su NULL
        node->key = NULL;
    }

    // Se è stato specificato un contenuto da memorizzare
    if(data && size > 0){
        // Alloco la memoria necessaria
        node->data = malloc(size);
        // Copio i dati nel nodo
        memcpy(node->data, data, size);
        // Riporto nel nodo la dimensione
        node->data_size = size;
    }else{
        // Altrimenti, imposto il puntatore node->data su NULL
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
    if(node->key) free(node->key);
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
        free(current); // TODO: list node desroy
    }
    // Cancello la testa della lista
    if(llist->first) free(llist->first);
    // Cancello la lista
    free(llist);
}

bool llist_push_first(linked_list_t* llist, const char* key, const void* data, size_t size){
    // Controllo la validità degli argomenti
    if(!llist){
        errno = EINVAL;
        return false;
    }

    // Creo il nuovo nodo 
    list_node_t* new_node = llist_node_create(key, data, size);
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

bool llist_pop_first(linked_list_t* llist, char** key, void** data){
    // Controllo la validità degli argomenti
    if(!llist){
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
    
    // Se <key> è specificato, copio la chiave del nodo
    if(key){
        *key = malloc(strlen(node->key));
        strncpy(*key, node->key, strlen(node->key)); 
    }

    // Se <data> è specificato, copio il contenuto del nodo
    if(data){
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

bool llist_push_last(linked_list_t* llist, const char* key, const void* data, size_t size){
    // Controllo la validità degli argomenti
    if(!llist){
        errno = EINVAL;
        return false;
    }

    // Creo il nuovo nodo 
    list_node_t* new_node = llist_node_create(key, data, size);
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

bool llist_pop_last(linked_list_t* llist, char** key, void** data){
    // Controllo la validità degli argomenti
    if(!llist){
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

    // Se <key> è specificato, copio la chiave del nodo
    if(key){
        *key = malloc(strlen(node->key));
        strncpy(*key, node->key, strlen(node->key));    
    }
    
    // Se <data> è specificato, copio il contenuto del nodo
    if(data){
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

bool llist_remove(linked_list_t* llist, const char* key){
    // Controllo la validità degli argomenti
    if(!llist || !key){
        errno = EINVAL;
        return false;
    }

    // Controllo che la lista non sia vuota
    if(llist->length == 0){
        errno = ENOENT;
        return false;
    }

    // Parto dalla testa della lista
    list_node_t* current_node = llist->first;

    while(current_node){
        // Se il nodo ha una chiave e questa è uguale a quella specificata
        if(current_node->key && strcmp(current_node->key, key) == 0){
            // Ho trovato il nodo, lo rimuovo dalla lista

            // Se il nodo ha un predecessore
            if(current_node->prev)
                // lo faccio puntare al mio successore
                current_node->prev->next = current_node->next;
            else 
                // altrimenti, vuol dire che è il nodo di testa della lista
                llist->first = current_node->next;
            
            // Se il nodo ha un successore
            if(current_node->next)
                // lo faccio puntare al mio predecessore
                current_node->next->prev = current_node->prev;
            else
                // altrimenti, vuol dire che è il nodo di coda della lista
                llist->last = current_node->prev;
            
            // Cancello il nodo
            llist_node_destroy(current_node);

            // Decremento il contatore degli elementi presenti in lista
            llist->length--;
            
            return true;
        }
        current_node = current_node->next;
    }

    // Non ho trovato il nodo cercato
    return false;
}

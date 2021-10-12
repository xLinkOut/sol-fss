// @author Luca Cirillo (545480)

// * Linked List, con inserimento in coda e rimozione partendo dalla testa

#include <errno.h>
#include <linkedlist.h>
#include <stdio.h>
#include <stdlib.h>

linked_list_t* linked_list_create() {
    linked_list_t* llist = malloc(sizeof(linked_list_t));
    if (!llist) return NULL;
    llist->first = llist->last = NULL;
    llist->size = 0;
    return llist;
}

void linked_list_destroy(linked_list_t* llist) {
    // Controllo la validità degli argomenti
    if (!llist) {
        errno = EINVAL;
        return;
    }
    // Scorro la lista per cancellare tutti i nodi
    while (llist->first != llist->last) {
        list_node_t* node = llist->first;
        llist->first = llist->first->next;
        free(node);
    }
    // Cancello la testa della lista
    if (llist->first) free(llist->first);
    // Cancello la lista
    free(llist);
}

bool linked_list_insert(linked_list_t* llist, int data) {
    // Controllo la validità degli argomenti
    if (!llist) {
        errno = EINVAL;
        return false;
    }
    // Creo un nuovo nodo
    list_node_t* new_node = malloc(sizeof(list_node_t));
    if (!new_node) return false;
    new_node->data = data;
    new_node->next = NULL;

    if (!llist->first) {
        // La lista è vuota, aggiorno testa e coda
        llist->first = new_node;
        llist->last = new_node;
    } else {
        // Inserisco in coda
        (llist->last)->next = new_node;
        llist->last = new_node;
    }

    llist->size++;
    return true;
}

bool linked_list_remove(linked_list_t* llist, int data) {
    // Controllo la validità degli argomenti
    if (!llist || !data) {
        errno = EINVAL;
        return false;
    }

    // Controllo che la lista non sia vuota
    if (!llist->first) {
        errno = ENOENT;
        return -1;
    }

    // Se il nodo cercato è la testa della lista
    if (llist->first->data == data) {
        list_node_t* head = llist->first;
        llist->first = llist->first->next;
        free(head);
        // Se ho rimosso l'unico nodo della lista, aggiorno anche la coda
        if (llist->first == NULL) llist->last = NULL;
        return true;
    }

    // Scorro la lista partendo dalla testa per trovare il nodo che contiene <data>
    list_node_t* last_node = NULL;
    list_node_t* current_node = llist->first->next;

    while (current_node) {
        if (current_node->data == data) {
            // Rimuovo il nodo
            // Aggancio il nodo precedente a quello successivo, rispetto al nodo corrente
            if (last_node) last_node->next = current_node->next;
            if (last_node->next == NULL) llist->last = last_node;
            free(current_node);
            llist->size--;
            return true;
        }
        last_node = current_node;
        current_node = current_node->next;
    }

    // Se nessun nodo corrisponde, alla fine ritorno false
    return false;
}

bool linked_list_find(linked_list_t* llist, int data) {
    // Controllo la validità degli argomenti e che la lista non sia vuota
    if (!llist || !llist->first) return false;
    list_node_t* current_node = llist->first;
    // Scorro tutti gli elementi della lista
    while (current_node) {
        // Appena trovo un nodo con l'elemento da cercare, ritorno true
        if (current_node->data == data)
            return true;
        current_node = current_node->next;
    }
    // Se non lo trovo, alla fine ritorno false
    return false;
}

void linked_list_print(linked_list_t* llist) {
    if (!llist) return;
    if (!llist->first) {
        printf("[-]\n");
        return;
    }
    list_node_t* node = (list_node_t*)llist->first;
    printf("[%d]", node->data);
    while ((node = node->next)) {
        printf("->[%d]", node->data);
    }
    printf("\n");
}

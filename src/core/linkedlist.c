// @author Luca Cirillo (545480)

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
    if (!llist) {
        errno = EINVAL;
        return;
    }
    while (llist->first != llist->last) {
        node_t* node = (node_t*)llist->first;
        llist->first = llist->first->next;
        free((void*)node);
    }
    if (llist->first) free(llist->first);
    free(llist);
}

int linked_list_push(linked_list_t* llist, int data, front_back where) {
    if (!llist) {
        errno = EINVAL;
        return -1;
    }
    node_t* new_node = malloc(sizeof(node_t));
    if (!new_node) return -1;
    new_node->data = data;
    new_node->prev = NULL;
    new_node->next = NULL;

    if (!llist->first) {
        llist->first = new_node;
        llist->last = new_node;
    } else {
        if (where == FRONT) {
            // * Push front
            new_node->next = llist->first;
            (llist->first)->prev = new_node;
            llist->first = new_node;
        } else {
            // * Push back
            new_node->prev = llist->last;
            (llist->last)->next = new_node;
            llist->last = new_node;
        }
    }

    llist->size++;
    return 0;
}

int linked_list_pop(linked_list_t* llist, int* data, front_back from) {
    if (!llist || !data) {
        errno = EINVAL;
        return -1;
    }

    if (llist->size == 0) {
        errno = ENOENT;
        return -1;
    }

    node_t* node;
    if (from == FRONT) {
        // * Pop front
        node = llist->first;
        llist->first = (llist->first)->next;
    } else {
        // * Pop back
        node = llist->last;
        llist->last = (llist->last)->next;
    }
    *data = node->data;
    free((void*)node);

    llist->size--;
    return 0;
}

bool linked_list_find(linked_list_t* llist, int data) {
    // Controllo la validità degli argomenti e che la lista non sia vuota
    if (!llist || !llist->first) return false;
    node_t* current_node = llist->first;
    // Scorro tutti gli elementi della lista
    while (current_node->next) {
        // Appena trovo un nodo con l'elemento da cercare, ritorno true
        if (current_node->data == data)
            return true;
        current_node = current_node->next;
    }
    // Se non lo trovo, alla fine ritorno false
    return false;
}

void linked_list_print(linked_list_t* llist) {
    if (!llist) {
        errno = EINVAL;
        return;
    }
    node_t* node = (node_t*)llist->first;
    printf("[%d]", node->data);
    while ((node = node->next)) {
        printf("->[%d]", node->data);
    }
    printf("\n");
}

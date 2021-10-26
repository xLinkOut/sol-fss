// @author Luca Cirillo (545480)

#include <errno.h>
#include <pthread.h>
#include <queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils.h>

queue_t* queue_init() {
    // Alloco memoria per la coda
    queue_t* queue = malloc(sizeof(queue_t));
    if (!queue) {
        //perror("Error: failed to allocate memory for queue");
        return NULL;
    }

    // Creo il nodo di testa
    if (!(queue->head = malloc(sizeof(node_t)))) {
        free(queue);
        return NULL;
    }

    // Inizializzo il nodo di testa
    queue->head->next = NULL;
    // Imposto il nodo in coda uguale a quello di testa, la coda è vuota
    queue->tail = queue->head;
    // Imposto la lunghezza della coda a zero, attualmente è vuota
    queue->length = 0;

    // Inizializzo l'accesso mutualmente esclusivo
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        //perror("Error: unable to init Queue mutex");
        free(queue->head);
        free(queue);
        return NULL;
    }

    // Inizializzo la variabile di condizione 'Coda vuota'
    if (pthread_cond_init(&queue->empty, NULL) != 0) {
        //perror("Error: unable to init Queue empty condition variable");
        pthread_mutex_destroy(&queue->mutex);
        free(queue->head);
        free(queue);
        return NULL;
    }

    // Finalmente, restituisco la coda pronta all'uso
    return queue;
}

void queue_destroy(queue_t* queue) {
    if (!queue) return;
    // Scorro la coda per cancellare tutti i nodi
    while (queue->head != queue->tail) {
        node_t* node = (node_t*)queue->head;
        queue->head = queue->head->next;
        free(node);
    }
    if (queue->head) free(queue->head);
    // Il compilatore qui consiglia di non controllare mutex ed empty, perché
    // "address of 'queue->mutex' (lo stesso per queue->empty) will always evaluate to 'true'"
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->empty);
    free(queue);
}

int queue_push(queue_t* queue, int fd_ready) {
    // Controllo la validità degli argomenti
    if (!queue) {
        errno = EINVAL;
        return -1;
    }
    // Creo un nuovo nodo
    node_t* new_node = malloc(sizeof(node_t));
    if (!new_node) return -1;
    // Imposto il suo contenuto
    new_node->fd_ready = fd_ready;
    new_node->next = NULL;

    // Accedo alla coda in maniera esclusiva
    LOCK(&queue->mutex);
    // Aggiungo il nuovo nodo in coda
    queue->tail->next = new_node;
    queue->tail = new_node;
    queue->length++;
    // Rilascio l'esclusività sulla coda
    // e risveglio eventuali consumatori in attesa
    SIGNAL(&queue->empty);
    UNLOCK(&queue->mutex);
    return 0;
}

int queue_pop(queue_t* queue) {
    // Controllo la validità degli argomenti
    if (!queue) {
        errno = EINVAL;
        return -2;
    }

    // Accedo alla coda in maniera esclusiva
    LOCK(&queue->mutex);
    // Se la coda è vuota, mi metto in attesa
    while (queue->head == queue->tail) {
        WAIT(&queue->empty, &queue->mutex);
    }

    // * A questo punto ho riacquisito la lock sulla coda
    // Tuttavia, è buona prassi controllare che sia effettivamente
    // stato aggiunto un nuovo nodo, altrimenti ritorno
    if (!queue->head->next) return -2;

    // Preparo l'occorrente per estrarre il nodo dalla coda
    node_t* node = (node_t*)queue->head;
    int fd_ready = (queue->head->next)->fd_ready;
    queue->head = queue->head->next;
    queue->length--;

    UNLOCK(&queue->mutex);
    free(node);

    return fd_ready;
}

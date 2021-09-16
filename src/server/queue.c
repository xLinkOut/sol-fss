// @author Luca Cirillo (545480)

#include <errno.h>
#include <pthread.h>
#include <queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils.h>

Queue_t* queue_init() {
    // Alloco memoria per la coda
    Queue_t* queue = malloc(sizeof(Queue_t));
    if (!queue){
        perror("Error: failed to allocate memory for queue");
        return NULL;
    }

    // Creo il nodo di testa
    if (!(queue->head = malloc(sizeof(Node_t)))) {
        free((void*)queue);
        return NULL;
    }

    // Inizializzo il nodo di testa
    queue->head->content = NULL;
    queue->head->next = NULL;

    // Imposto il nodo in coda uguale a quello di testa, la coda è vuota
    queue->tail = queue->head;

    // Imposto la lunghezza della coda a zero, attualmente è vuota
    queue->length = 0;

    // Inizializzo l'ccesso mutualmente esclusivo
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        perror("Error: unable to init Queue mutex");
        free((void*)queue->head);
        free((void*)queue);
        return NULL;
    }

    // Inizializzo la variabile di condizione 'Coda vuota'
    if (pthread_cond_init(&queue->empty, NULL) != 0) {
        perror("Error: unable to init Queue empty condition variable");
        pthread_mutex_destroy(&queue->mutex);
        free((void*)queue->head);
        free((void*)queue);
        return NULL;
    }

    // Finalmente, restituisco la coda pronta all'uso
    return queue;
}

void queue_destroy(Queue_t* queue) {
    while (queue->head != queue->tail) {
        Node_t* node = (Node_t*)queue->head;
        queue->head = queue->head->next;
        free((void*)node);
    }
    if (queue->head) free((void*)queue->head);
    if (&queue->mutex) pthread_mutex_destroy(&queue->mutex);
    if (&queue->empty) pthread_cond_destroy(&queue->empty);
    free(queue);
}

int queue_push(Queue_t* queue, void* data) {
    // Controllo la validità degli argomenti
    if ((queue == NULL) || (data == NULL)) {
        errno = EINVAL;
        return -1;
    }
    // Creo un nuovo nodo
    Node_t* new_node = malloc(sizeof(Node_t));
    if (!new_node) return -1;
    // Imposto il suo contenuto
    new_node->content = data;
    new_node->next = NULL;

    // Accedo alla coda in maniera esclusiva
    LOCK(&queue->mutex);
    // Aggiungo il nuovo nodo in coda
    queue->tail->next = new_node;
    queue->tail = new_node;
    queue->length += 1;
    // Rilascio l'esclusività sulla coda
    // e risveglio eventuali consumatori in attesa
    SIGNAL(&queue->empty);
    UNLOCK(&queue->mutex);
    return 0;
}

void* queue_pop(Queue_t* queue) {
    // Controllo la validità degli argomenti
    if (queue == NULL) {
        errno = EINVAL;
        return NULL;
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
    // ? Troppo drastico utilizzare assert() ?
    if (!queue->head->next) return NULL;

    // Preparo l'occorrente per estrarre il nodo dalla coda
    Node_t* node = (Node_t*)queue->head;
    void* data = (queue->head->next)->content;
    queue->head = queue->head->next;
    queue->length -= 1;
    // ? Controllare che length sia >= 0 ?
    UNLOCK(&queue->mutex);

    free((void*)node);
    return data;
}

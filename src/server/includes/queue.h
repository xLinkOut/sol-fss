// @author Luca Cirillo (545480)

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>

// * Struttura dati di un elemento della coda
typedef struct Node {
    void* content;
    struct Node* next;
} Node_t;

// * Struttura dati della coda
typedef struct Queue {
    Node_t* head;
    Node_t* tail;
    unsigned long length;
    pthread_mutex_t mutex;
    pthread_cond_t empty;
} Queue_t;

// * Inizializza la coda e ritorna un puntatore ad essa
Queue_t* queue_init();

// * Cancella una coda creata con queue_init
void queue_destroy(Queue_t* queue);

// * Inserisce un elemento nella coda
int queue_push(Queue_t* queue, void* data);

// * Estrae un elemento dalla coda
void* queue_pop(Queue_t* queue);

#endif

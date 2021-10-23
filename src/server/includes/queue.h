// @author Luca Cirillo (545480)

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>

// * Struttura dati di un file descriptor pronto in coda
typedef struct Node {
    int fd_ready; // Indice di un descrittore pronto, oppure segnale di terminazione
    struct Node* next;
} node_t;

// * Struttura dati della coda
typedef struct Queue {
    node_t* head; // Testa
    node_t* tail; // Coda
    unsigned long length; // Tasks in coda
    pthread_mutex_t mutex; // Accesso esclusivo
    pthread_cond_t empty; // Coda vuota
} queue_t;

// * Inizializza la coda e ritorna un puntatore ad essa
queue_t* queue_init();

// * Cancella una coda creata con queue_init
void queue_destroy(queue_t* queue);

// * Inserisce un fd nella coda
int queue_push(queue_t* queue, int data);

// * Estrae un fd dalla coda
// ! Files descriptor validi sono interi non-negativi
// Il valore -1 viene interpretato dal thread worker come segnale di terminazione da parte del dispatcher
// Il valore -2 viene interpretato come errore della funzione queue_pop
// Qualsiasi altro valore negativo è interpretato dal thread worker come invalido
int queue_pop(queue_t* queue);

#endif

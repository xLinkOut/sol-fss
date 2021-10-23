// @author Luca Cirillo (545480)

#ifndef _REQUEST_QUEUE_H_
#define _REQUEST_QUEUE_H_

#include <time.h>

// * Struttura dati di una richiesta in coda
typedef struct Request {
    char command;     // Un comando tra w|W|r|R|l|u|c
    char* arguments;  // Uno o più argomenti tra dirname[,n=0]|file1[,file2]|[n=0]
    char* dirname;    // Parametro opzionale utilizzato congiuntamente a w|W|r|R
    time_t time;      // Tempo di attesa in millisecondi tra una richiesta e l'altra
    struct Request* next;
} request_t;

// * Struttura dati della coda
typedef struct Queue {
    request_t* head;       // Testa
    request_t* tail;       // Coda
    unsigned long length;  // Elementi in coda
} queue_t;

// * Crea una coda vuota e ritorna un puntatore ad essa
queue_t* queue_init();

// * Cancella una coda creata con queue_init
void queue_destroy(queue_t* queue);

// * Inserisce una richiesta in coda
int queue_push(queue_t* queue, request_t* new_request);

// * Estrae una richiesta dalla coda
request_t* queue_pop(queue_t* queue);

// * Inizializza un nuovo nodo e ritorna un puntatore ad esso
request_t* queue_new_request();

// * Cancella un nodo creato con queue_new_node
void queue_destroy_request(request_t* request);

// * Debug: stampa il contenuto della coda
void queue_print(queue_t*);

#endif

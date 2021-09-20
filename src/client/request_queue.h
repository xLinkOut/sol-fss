// @author Luca Cirillo (545480)

#ifndef _REQUEST_QUEUE_H_
#define _REQUEST_QUEUE_H_

// * Struttura dati di una richiesta in coda
typedef struct Request {
    char command;        // Un comando tra w|W|r|R|l|u|c
    char* arguments;     // Uno o più argomenti tra dirname[,n=0]|file1[,file2]|[n=0]
    char* dirname;       // Parametro opzionale utilizzato congiuntamente a w|W|r|R
    unsigned long time;  // Tempo di attesa in millisecondi tra una richiesta e l'altra
    struct Request* next;
} Request_t;

// * Struttura dati della coda
typedef struct Queue {
    Request_t* head;       // Testa
    Request_t* tail;       // Coda
    unsigned long length;  // Elementi in coda
} Queue_t;

// * Crea una coda vuota e ritorna un puntatore ad essa
Queue_t* queue_init();

// * Cancella una coda creata con queue_init
void queue_destroy(Queue_t* queue);

// * Inserisce una richiesta in coda
int queue_push(Queue_t* queue, Request_t* new_request);

// * Estrae una richiesta dalla coda
Request_t* queue_pop(Queue_t* queue);

// * Inizializza un nuovo nodo e ritorna un puntatore ad esso
Request_t* queue_new_request();

// * Cancella un nodo creato con queue_new_node
void queue_destroy_request(Request_t* request);

#endif
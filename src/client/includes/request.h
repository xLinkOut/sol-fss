// @author Luca Cirillo (545480)

// * Request:
// *  Struttura che incapsula tutti i parametri relativi ad una richiesta
// *  espressa da riga di comando.

#ifndef _REQUEST_H_
#define _REQUEST_H_

// * Struttura dati di una richiesta in coda
typedef struct Request {
    char command;        // Un comando tra w|W|r|R|l|u|c
    char* arguments;     // Uno o più argomenti tra dirname[,n=0]|file1[,file2]|[n=0]
    char* dirname;       // Parametro opzionale utilizzato congiuntamente a w|W|r|R
    unsigned long time;  // Tempo di attesa in millisecondi tra una richiesta e l'altra
} request_t;

// * Inizializza un nuovo nodo e ritorna un puntatore ad esso
request_t* request_create();

// * Cancella un nodo creato con queue_new_node
void request_destroy(void* request);

#endif

// @author Luca Cirillo (545480)

#ifndef _API_H_
#define _API_H_

#include <time.h>

// Socket del client
int client_socket;

// * Apre una connessione con il server al socket file sockname
int openConnection(const char* sockname, int msec, const struct timespec abstime);

// * Chiude la connessione associata al socket file sockname
int closeConnection(const char* sockname);

// * Richiede la creazione o l'apertura di un file
int openFile(const char* pathname, int flags);

#endif

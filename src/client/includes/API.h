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

// * Richiede la creazione e/o l'apertura del file <pathname>, in accordo a <flags>
int openFile(const char* pathname, int flags);

// * Scrive il file <pathname> sul server, e salva in <dirname> eventuali file espulsi
int writeFile(const char* pathname, const char* dirname);

// * Richiede la chiusura del file <pathname> precedentemente aperto con openFile
int closeFile(const char* pathname);

#endif

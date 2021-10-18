// @author Luca Cirillo (545480)

#ifndef _API_H_
#define _API_H_

#include <time.h>
#include <stdbool.h>

// Modalità verbose
bool VERBOSE;
// Socket del client
int client_socket;

// * Apre una connessione con il server al socket file sockname
int openConnection(const char* sockname, int msec, const struct timespec abstime);

// * Chiude la connessione associata al socket file sockname
int closeConnection(const char* sockname);

// * Richiede la creazione e/o l'apertura del file <pathname>, in accordo a <flags>
int openFile(const char* pathname, int flags);

// * Legge il contenuto del file <pathname> nel buffer <buf>
int readFile(const char* pathname, void** buf, size_t* size, const char* dirname);

// * Scrive il file <pathname> sul server, e salva in <dirname> eventuali file espulsi
int writeFile(const char* pathname, const char* dirname);

// * Aggiunge <buf> di dimensione <size> al file <pathname>, salva in <dirname> eventuali file espulsi
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

// * Acquisisce il lock in scrittura sul file <pathname>
int lockFile(const char* pathname);

// * Rilascia il lock in scrittura sul file <pathname>
int unlockFile(const char* pathname);

// * Richiede la chiusura del file <pathname> precedentemente aperto con openFile
int closeFile(const char* pathname);

// * Rimuovere il file <pathname> cancellandolo dallo storage
// * Richiede che il file sia stato aperto in scrittura
int removeFile(const char* pathname);

#endif

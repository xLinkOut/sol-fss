// @author Luca Cirillo (545480)

#ifndef _SERVER_CONFIG_H_
#define _SERVER_CONFIG_H_

// Politica di rimpiazzamento
enum POLICY { FIFO, LRU, LFU };

// Percorso del file di configurazione specificato come parametro
char* CONFIG_PATH;
// Percorso di default per il file di configurazione
const char* DEFAULT_CONFIG_PATH = "./config.txt";

// Parametri di configurazione del Server
// Numero di threads worker
size_t THREADS_WORKER;
// Numero massimo di file consentiti
size_t STORAGE_MAX_FILES;
// Dimensione massima dello Storage, in Mb
size_t STORAGE_MAX_CAPACITY;
// Politica di rimpiazzamento
enum POLICY REPLACEMENT_POLICY;
// Path al Socket file
char* SOCKET_PATH;
// Path al Log file
char* LOG_PATH;

#endif

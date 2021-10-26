// @author Luca Cirillo (545480)

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <icl_hash.h>
#include <linkedlist.h>
#include <pthread.h>
#include <rwlock.h>

// * Struttura dati dello storage
typedef struct Storage {
    icl_hash_t* files;                        // Hashmap di StorageFile
    replacement_policy_t replacement_policy;  // Politica di rimpiazzo scelta
    rwlock_t* rwlock;                         // Global Storage read-write-lock

    size_t number_of_files;  // Numero di files attualmente memorizzati, parte da 0 fino a <max_files>
    size_t max_files;        // Numero di files massimo memorizzabile, pari a STORAGE_MAX_FILES
    size_t capacity;         // Spazio attualmente occupato dai files, parte da 0 fino a <max_capacity>
    size_t max_capacity;     // Spazio massimo disponibile, pari a STORAGE_MAX_CAPACITY

    // Statistiche
    time_t start_timestamp;       // Istante di tempo di inizio attività del server
    size_t max_files_reached;     // Numero massimo di file memorizzati nello storage
    size_t max_capacity_reached;  // Capienza massima raggiunta nello storage
    size_t rp_algorithm_counter;  // Numero di esecuzioni dell'algoritmo di rimpiazzo
} storage_t;

// * Struttura dati di un generico file memorizzato nello storage
/*  Su uno stesso file ci possono essere:
        1. più lettori attivi contemporaneamente, ma nessuno scrittore, oppure
        2. un solo scrittore attivo, e nessun lettore.
    
    Il lock in lettura si ottiene con l'apertura del file tramite la chiamata openFile con il flag O_READ,
        oppure come conseguenza della creazione del file con il flag O_CREATE, o ancora implicitamente 
        a seguito dell'apertura in modalità scrittura, specificando il flag O_LOCK;
        si rilascia con la chiusura del file tramite la chiamata closeFile.
    
    Il lock in scrittura si ottiene in due modi diversi:
        1. con la chiamata openFile specificando (eventualmente in OR) il flag O_LOCK, oppure
        2. esplicitamente, tramite la chiamata lockFile;
    e si rilascia in due modi diversi (in maniera duale all'acquisizione):
        1. implicitamente, tramite la chiamata closeFile, che rilascia contemporaneamente lock in lettura e scrittura, oppure
        2. esplicitamente, tramite la chiamata unlockFile.
    
    Gli scrittori hanno accesso prioritario al file, ma devono comunque aspettare che tutti i lettori lo chiudano, prima di operare.
*/
typedef struct StorageFile {
    // File-related
    char* name;      // Nome del file
    void* contents;  // Contenuto del file
    size_t size;     // Dimensione del file

    // Lock-related
    rwlock_t* rwlock;        // Readers/Writers Lock
    linked_list_t* readers;  // Lista di lettori attivi, ovvero di client che hanno aperto il file in lettura
    int writer;              // Client che al momento ha il lock in scrittura sul file

    // Replacement-related
    time_t creation_time;    // Timestamp della creazione del file nello storage (FIFO)
    time_t last_use_time;    // Timestamp dell'ultimo utilizzo del file (LRU)
    unsigned int frequency;  // Numero di accessi al file (LFU)

} storage_file_t;

// * Inizializza uno storage e ritorna un puntatore ad esso
storage_t* storage_create(size_t max_files, size_t max_capacity, replacement_policy_t rp);

// * Cancella uno storage creato con storage_create
void storage_destroy(storage_t* storage);

// * Visualizza i file contenuti nello storage
void storage_print(storage_t* storage);

// * Inizializza uno storage file e ritorna un puntatore ad esso
storage_file_t* storage_file_create(const char* name, const void* contents, size_t size);

// * Cancella uno storage file creato con storage_file_create
void storage_file_destroy(void* file);

// ! APIs
// * Crea e/o apre il file <pathname> in lettura ed eventualmente in scrittura, in accordo a <flags>
int storage_open_file(storage_t* storage, const char* pathname, int flags,
                      int* victims_no, storage_file_t*** victims, int client);

// * Legge il file <pathname> dallo storage, copiando il suo contenuto in <contents>
int storage_read_file(storage_t* storage, const char* pathname, void** contents, size_t* size, int client);

// * Legge dallo storage <n> files e li invia al client
int storage_read_n_files(storage_t* storage, int N, storage_file_t*** files_read, int client);

// * Scrive nello storage il file <pathname> ed il suo contenuto <contents>
int storage_write_file(storage_t* storage, const char* pathname, void* contents, size_t size,
                       int* victims_no, storage_file_t*** victims, size_t* old_size, int client);

// * Aggiunge <contents>, di dimensione <size>, in fondo al file <pathname>
int storage_append_to_file(storage_t* storage, const char* pathname, const void* contents, size_t size,
                           int* victims_no, storage_file_t*** victims, int client);

// * Imposta il lock in scrittura sul file <pathname> per <client>
int storage_lock_file(storage_t* storage, const char* pathname, int client);

// * Rilascia il lock in scrittura del file <pathname> per <client>
int storage_unlock_file(storage_t* storage, const char* pathname, int client);

// * Chiude il file <pathname> aperto in precedenza con storage_open_file da <client>
int storage_close_file(storage_t* storage, const char* pathname, int client);

// * Cancella il file <pathname> dallo storage, se è stato aperto in scrittura da <client>
int storage_remove_file(storage_t* storage, const char* pathname, size_t* size, int client);

#endif

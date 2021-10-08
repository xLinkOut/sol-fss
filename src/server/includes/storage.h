// @author Luca Cirillo (545480)

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <icl_hash.h>
#include <linkedlist.h>
#include <pthread.h>

// * Struttura dati dello storage
typedef struct Storage {
    icl_hash_t* files;  // Hashmap di StorageFile

    size_t number_of_files;  // Numero di files attualmente memorizzati
    size_t max_files;        // Numero di files massimo memorizzabile
    size_t capacity;         // Spazio attualmente occupato dai files
    size_t max_capacity;     // Spazio massimo disponibile

} storage_t;

// * Struttura dati di un generico file memorizzato nello storage
/*  Su uno stesso file ci possono essere:
        1. più lettori attivi contemporaneamente, ma nessuno scrittore, oppure
        2. un solo scrittore attivo, e nessun lettore.
    
    Il lock in lettura si ottiene sempre e solo con l'apertura del file tramite la chiamata openFile senza nessun flag,
        oppure con il solo flag O_CREATE, e si rilascia con la conseguente chiusura del file tramite la chiamata closeFile.
    
    Il lock in scrittura si ottiene in due modi diversi:
        1. implicitamente, con la chiamata openFile specificando (eventualmente in OR) il flag O_LOCK, oppure
        2. esplicitamente, tramite la chiamata lockFile;
    e si rilascia in due modi diversi (in maniera duale all'acquisizione):
        1. implicitamente, tramite la chiamata closeFile, che rilascia contemporaneamente lock in lettura e scrittura, oppure
        2. esplicitamente, tramite la chiamata unlockFile.
    
    Gli scrittori hanno accesso prioritario al file, ma devono comunque aspettare che tutti i lettori lo chiudano, prima di operare.
*/
typedef struct StorageFile {
    char* name;      // Nome del file
    void* contents;  // Contenuto del file
    size_t size;     // Dimensione del file

    // Read/Write lock
    pthread_mutex_t mutex;         // Accesso esclusivo al file (lettura + scrittura)
    pthread_cond_t wait;           // Condizione su cui lettori e scrittori aspettano
    unsigned int pending_readers;  // Lettori in attesa
    unsigned int pending_writers;  // Scrittori in attesa
    linked_list_t* readers;        // Lista di lettori attivi, che hanno aperto il file in lettura
    int writer;                    // Client che al momento ha il lock in scrittura sul file
} storage_file_t;

// * Inizializza uno storage e ritorna un puntatore ad esso
storage_t* storage_create(size_t max_files, size_t max_capacity);

// * Cancella uno storage creato con storage_create
void storage_destroy(storage_t* storage);

// * Inizializza uno storage file e ritorna un puntatore ad esso
storage_file_t* storage_file_create(const char* name, const void* contents, size_t size);

// * Cancella uno storage file creato con storage_file_create
void storage_file_destroy(storage_file_t* file);

// ! APIs
// * Crea e/o apre il file <pathname> in lettura ed eventualmente in scrittura, in accordo a <flags>
int storage_open_file(storage_t* storage, const char* pathname, int flags, int client);

// * Legge il file <pathname> dallo storage
int storage_read_file(storage_t* storage, const char* pathname, void** contents, size_t* size, int client);

// * Scrive nello storage il file <pathname> ed il suo contenuto <contents>
int storage_write_file(storage_t* storage, const char* pathname, const void* contents, size_t size, int client);

// * Aggiunge <contents>, di dimensione <size>, in fondo al file <pathname> 
int storage_append_to_file(storage_t* storage, const char* pathname, const void* contents, size_t size, int client);

// * Chiude il file <pathname> aperto in precedenza con storage_open_file da <client>
int storage_close_file(storage_t* storage, const char* pathname, int client);

// * Espelle dallo storage uno o più files per poterne ospitare uno di dimensione <size>
// * Ritorna il numero di file espulsi, 0 se non è stato necessario espelle file, -1 in caso di fallimento
// * In <victims> verranno salvati i files espulsi dallo storage
// TODO: victims sarà una linked list di <void*>, da castare opportunamente a <storage_file_t*>
int storage_eject_file(storage_t* storage, const char* pathname, size_t size, int client, storage_file_t** victims);

#endif

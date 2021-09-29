// @author Luca Cirillo (545480)

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <icl_hash.h>

// * Struttura dati dello storage
typedef struct Storage {
    icl_hash_t* files;  // Hashmap di StorageFile

    size_t number_of_files;  // Numero di files attualmente memorizzati
    size_t max_files;        // Numero di files massimo memorizzabile

} storage_t;

// * Struttura dati di un qualsiasi file memorizzato nello storage
typedef struct StorageFile {
    char* name;      // Nome del file
    void* contents;  // Contenuto del file
    size_t size;     // Dimensione del file
} storage_file_t;

// * Inizializza uno storage e ritorna un puntatore ad esso
storage_t* storage_create();

// * Cancella uno storage creato con storage_create
void storage_destroy(storage_t* storage);

// * Inizializza uno storage file e ritorna un puntatore ad esso
storage_file_t* storage_file_create(const char* name, const void* contents, size_t size);

// * Cancella uno storage file creato con storage_file_create
void storage_file_destroy(storage_file_t* file);

#endif

// @author Luca Cirillo (545480)

#include <constants.h>
#include <errno.h>
#include <icl_hash.h>
#include <stdbool.h>
#include <stdlib.h>
#include <storage.h>
#include <string.h>

storage_t* storage_create(size_t buckets, size_t max_files) {
    // Controllo la validità degli argomenti
    if (buckets == 0) {
        errno = EINVAL;
        return NULL;
    }

    // Alloco la memoria per lo storage
    storage_t* storage = malloc(sizeof(storage_t));
    if (!storage) return NULL;
    // Creo la hashmap per memorizzare i files
    storage->files = icl_hash_create(buckets, NULL, NULL);
    if (!storage->files) {
        free(storage);
        return NULL;
    }

    // Inizializzo o salvo gli altri parametri
    storage->max_files = max_files;
    storage->number_of_files = 0;

    // Ritorno un puntatore allo storage
    return storage;
}

void storage_destroy(storage_t* storage) {
    // Controllo la validità degli argomenti
    if (!storage) return;
    // Cancello la hashmap
    icl_hash_destroy(storage->files, NULL, NULL);  // TODO: free key-data
    // Libero la memoria dello storage
    free(storage);
}

storage_file_t* storage_file_create(const char* name, const void* contents, size_t size) {
    // Controllo la validità degli argomenti
    if (!name) {
        errno = EINVAL;
        return NULL;
    }
    // Alloco la memoria per un file
    storage_file_t* file = malloc(sizeof(storage_file_t));
    if (!file) return NULL;

    // Salvo il nome del file
    if ((file->name = malloc(strlen(name) + 1)) == NULL) {
        free(file);
        return NULL;
    }
    strncpy(file->name, name, strlen(name) + 1);

    // Salvo il contenuto del file
    if (contents && size != 0) {
        if ((file->contents = malloc(size)) == NULL) {
            free(file->name);
            free(file);
            return NULL;
        }
        memcpy(file->contents, contents, size);
        file->size = size;
    }

    // Ritorno un puntatore al file
    return file;
}

void storage_file_destroy(storage_file_t* file) {
    // Controllo la validità degli argomenti
    if (!file) return;
    // Libero la memoria occupata dal file
    free(file->name);
    free(file->contents);
    free(file);
}

// ! APIs
int storage_open_file(storage_t* storage, const char* pathname, int flags) {
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Controllo se i flags O_CREATE e O_LOCK sono settati
    bool create_flag = IS_O_CREATE(flags);
    bool lock_flag = IS_O_LOCK(flags);
    // Controllo se il file esiste all'interno dello storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // Mantengo separata l'informazione sull'esistenza del file per leggibilità
    bool already_exists = (bool)file;

    // Gestisco prima tutte le possibili situazioni di errore
    // Flag O_CREATE settato e file già esistente
    if (create_flag && already_exists) {
        errno = EEXIST;
        return -1;
    }
    // Flag O_CREATE non settato e file non esistente
    if (!create_flag && !already_exists) {
        errno = ENOENT;
        return -1;
    }

    // I flags sono coerenti con lo stato dello storage, posso procedere
    // Distinguo il caso in cui il file esiste già da quello in cui non esiste ancora
    if (already_exists) {
        // * Il file esiste già nello storage, lo apro per il client in lettura e scrittura
        // TODO: controllo che lo stesso client non voglia aprire più volte lo stesso file
    } else {
        // * Il file non esiste ancora nello storage, lo creo
        // Controllo che nello storage ci sia effettivamente spazio in termini di numero di files
        if (storage->number_of_files == storage->max_files) {
            errno = ENOSPC;
            return -1;
        }
        // Creo il file all'interno dello storage
        file = storage_file_create(pathname, NULL, 0);
        icl_hash_insert(storage->files, pathname, file);
    }

    // Controllo se il file deve essere aperto in modalità locked
    if (lock_flag) {
        // TODO: lock in scrittura
    } else {
        // TODO: lock soltanto in lettura
    }

    return 0;
}

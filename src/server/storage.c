// @author Luca Cirillo (545480)

#include <constants.h>
#include <errno.h>
#include <icl_hash.h>
#include <stdbool.h>
#include <stdlib.h>
#include <storage.h>
#include <string.h>
#include <rwlock.h>

storage_t* storage_create(size_t max_files, size_t max_capacity) {
    // Controllo la validità degli argomenti
    if (max_files == 0 || max_capacity == 0) {
        errno = EINVAL;
        return NULL;
    }

    // Alloco la memoria per lo storage
    storage_t* storage = malloc(sizeof(storage_t));
    if (!storage) return NULL;
    
    // Inizializzo la struttura relativa al lock globale dello storage
    storage->rwlock = rwlock_create();
    if(!storage->rwlock){
        free(storage);
        return NULL;
    }
    
    // Creo la hashmap per memorizzare i files
    storage->files = icl_hash_create(max_files, NULL, NULL);
    if (!storage->files) {
        rwlock_destroy(storage->rwlock);
        free(storage);
        return NULL;
    }

    // Inizializzo o salvo gli altri parametri
    storage->number_of_files = 0;
    storage->max_files = max_files;
    storage->capacity = max_capacity;
    storage->max_capacity = max_capacity;

    // Ritorno un puntatore allo storage
    return storage;
}

void storage_destroy(storage_t* storage) {
    // Controllo la validità degli argomenti
    if (!storage) return;
    // Cancello la hashmap
    icl_hash_destroy(storage->files, NULL, NULL);  // TODO: free key-data
    // Cancello il RWLock
    rwlock_destroy(storage->rwlock);
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
    } else {
        // Se il contenuto non è specificato, inizializzo i campi di contenuto e dimensione
        file->contents = NULL;
        file->size = 0;
    }

    // Inizializzo la struttura relativa al lock del file
    file->rwlock = rwlock_create();
    if(!file->rwlock){
        free(file->contents);
        free(file->name);
        free(file);
        return NULL;
    }

    // Lettori che hanno aperto il file
    file->readers = linked_list_create();
    // Scrittore che ha la lock sul file
    file->writer = 0;

    // Ritorno un puntatore al file
    return file;
}

void storage_file_destroy(storage_file_t* file) {
    // Controllo la validità degli argomenti
    if (!file) return;
    // Libero la memoria occupata dal file
    free(file->name);
    free(file->contents);
    rwlock_destroy(file->rwlock);
    free(file);
}

void storage_file_print(storage_file_t* file) {
    if (!file) return;
    printf("%s (%zd Bytes)\nWriter: [%d], Readers: ", file->name, file->size, file->writer);
    linked_list_print(file->readers);
}

// ! APIs

int storage_open_file(storage_t* storage, const char* pathname, int flags, int client) {
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Controllo se i flags O_CREATE e O_LOCK sono settati
    bool create_flag = IS_O_CREATE(flags);
    bool lock_flag = IS_O_LOCK(flags);

    // TODO: Lock sull'intero storage se O_CREATE è settato

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
        // * Il file esiste già nello storage, faccio gli adeguati controlli per ogni operazione
        // Controllo che lo stesso client non voglia aprire più volte lo stesso file
        if (linked_list_find(file->readers, client)) {
            errno = EEXIST;
            return -1;
        }

        // Controllo che il file non sia lockato da un altro client
        if (file->writer != 0 && file->writer != client) {
            errno = EACCES;
            return -1;
        }
    } else {
        // * Il file non esiste ancora nello storage, lo creo
        // Controllo che nello storage ci sia effettivamente spazio in termini di numero di files
        // ? In questo caso occorre far partire la procedura di replace? Magari per espellere il file più piccolo?
        if (storage->number_of_files == storage->max_files) {
            errno = ENOSPC;
            return -1;
        }
        // Creo il file all'interno dello storage
        file = storage_file_create(pathname, NULL, 0);
        icl_hash_insert(storage->files, pathname, file);
    }

    // Apro il file in lettura
    linked_list_push(file->readers, client, BACK);
    // Controllo se aprire il file anche in scrittura
    if (lock_flag) file->writer = client;

    storage_file_print(file);
    icl_hash_dump(stdout, storage->files);

    // TODO: Rilasciare il lock sull'intero storage

    return 0;
}

int storage_read_file(storage_t* storage, const char* pathname, void** contents, size_t* size, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname || !contents) {
        errno = EINVAL;
        return -1;
    }

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // Controllo se il file esiste all'interno dello storage
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    // Controllo che il client abbia aperto il file in lettura
    if (!linked_list_find(file->readers, client)) {
        errno = EPERM;
        return -1;
    }
    //printf("r: %p\n", file->contents);
    *contents = file->contents;
    *size = file->size;
    return 0;
}

int storage_write_file(storage_t* storage, const char* pathname, const void* contents, size_t size, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname || !contents || size <= 0) {
        errno = EINVAL;
        return -1;
    }

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // Tutti i controlli del caso dovrebbero essere stati eseguiti dalla storage_eject_file
    // Tuttavia, effettuo comunque un controllo
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    // Controllo nuovamente che il file sia stato aperto in scrittura dal client
    if(file->writer != 0 && file->writer != client){
        errno = EPERM;
        return -1;
    }

    // Aggiorno il contenuto del file
    // TODO: se il file non è vuoto, devo cancellare il vecchio contenuto prima di scrivere il nuovo
    free(file->contents); // TODO: devo effettivamente farlo?
    file->contents = contents;
    file->size = size;

    // Aggiorno le informazioni dello storage
    storage->capacity += size;
    
    return 0;
}

int storage_append_to_file(storage_t* storage, const char* pathname, const void* contents, size_t size, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname || !contents || size <= 0) {
        errno = EINVAL;
        return -1;
    }

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // Tutti i controlli del caso dovrebbero essere stati eseguiti dalla storage_eject_file
    // Tuttavia, effettuo comunque un controllo
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    // Controllo nuovamente che il file sia stato aperto in scrittura dal client
    if(file->writer != 0 && file->writer != client){
        errno = EPERM;
        return -1;
    }

    // Amplio la memoria allocata per il file
    void* updated_contents = realloc(file->contents, file->size + size);
    if(!updated_contents) return -1;
    file->contents = updated_contents;
    // Aggiungo <contents> partendo dalla fine di <file->contents>
    memcpy(file->contents + file->size, contents, size);
    // Aggiorno la dimensione del file
    file->size += size;
    // todo update storage size
    return 0;

}

int storage_lock_file(storage_t* storage, const char* pathname, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // Controllo che il file esista
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    // Se il file è gia aperto in scrittura per il client che ha effettuato la richiesta, ritorno immediatamente
    if(file->writer == client) return 0;

    // Se il file non è stato precedentemente aperto, almeno in lettura, dal client, non posso aprirlo in scrittura
    if(!linked_list_find(file->readers, client)) return -1;

    // Se il file non è stato aperto in scrittura da nessun client (= non è bloccato in scrittura)
    if(file->writer == 0){
        // Imposto il lock in scrittura sul file per il client 
        file->writer = client;
    }else{
        // Se il file è lockato da un altro client, aspetto che venga rilasciato
        // e solo successivamente acquisisco la lock
        // ! 
        // TODO: variabile di condizione sul lock su cui aspettare
        file->writer = client;
    }
    
    storage_file_print(file);

    return 0;
}

int storage_unlock_file(storage_t* storage, const char* pathname, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // Controllo che il file esista
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    if(file->writer == 0 || file->writer != client){
        // Il file non è attualmente lockato in scrittura, oppure
        // la lock è detenuta da un client diverso
        errno = ENOLCK;
        return -1;
    }

    // A questo punto, file->writer sarà pari a client, per costruzione, 
    // ovvero client ha in precedenza aperto il file in scrittura
    // Rilascio quindi la lock
    file->writer = 0;
    storage_file_print(file);
    // Il file rimarrà aperto in lettura
    return 0;

}

int storage_close_file(storage_t* storage, const char* pathname, int client) {
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // TODO: Lock in lettura sullo storage
    // Controllo se il file esiste all'interno dello storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // TODO: Unlock storage

    // Se non esiste, ritorno subito errore
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    // Controllo che <client> abbia precedentemente eseguito la openFile
    if (!linked_list_find(file->readers, client)) {
        errno = ENOLCK;
        return -1;
    }

    // TODO: Implementare una linked_list_remove(linked_list_t*, int)
    // Rilascio il lock in lettura sul file
    linked_list_pop(file->readers, &client, FRONT);

    // Rilancio l'eventuale lock in scrittura sul file
    // TODO: chiamare internamente la unlockFile()
    if (file->writer != 0 && file->writer == client) {
        file->writer = 0;
    }

    storage_file_print(file);

    return 0;
}

int storage_remove_file(storage_t* storage, const char* pathname, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // Controllo che il file esista
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    if(file->writer == 0 || file->writer != client){
        // Il file non è attualmente lockato in scrittura, oppure
        // la lock è detenuta da un client diverso
        errno = ENOLCK;
        return -1;
    }

    // A questo punto, file->writer sarà pari a client, per costruzione, 
    // ovvero client ha in precedenza aperto il file in scrittura
    // Cancello quindi il file dallo storage
    if(icl_hash_delete(storage->files, pathname, NULL, &storage_file_destroy) == 0){
        // TODO: funzione di free per file
        icl_hash_dump(stdout, storage->files);
        return 0;
    }

    return -1;
}

int storage_eject_file(storage_t* storage, const char* pathname, size_t size, int client, storage_file_t** victims){
    // Controllo la validità degli argomenti
    if(!storage || !pathname || size <= 0 || !victims){
        errno = EINVAL;
        return -1;
    }

    // TODO: storage lock

    // Controllo che la dimensione del file non sia maggiore dell'intero storage
    if(size > storage->max_capacity){
        errno = ENOSPC;
        return -1;
    }

    // Controllo che il file esista nello storage (e.g. è stato creato con storage_open_file)
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    if(!file){
        errno = ENOENT;
        return -1;
    }

    // Controllo che il file sia stato aperto in scrittura dal client
    if(file->writer != 0 && file->writer != client){
        errno = EPERM;
        return -1;
    }


    // Controllo che ci sia spazio libero a sufficienza per salvare il file
    if(size > storage->capacity){
        // Se non c'è spazio, decido qui quali file espellere dallo storage
        
        int victims_no = 0; // Numero dei file espulsi dallo storage
        
        // TODO: replacement mechanism
        
        return victims_no;
    }

    return 0;
}

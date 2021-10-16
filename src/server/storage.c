// @author Luca Cirillo (545480)

#include <constants.h>
#include <errno.h>
#include <icl_hash.h>
#include <stdbool.h>
#include <stdlib.h>
#include <storage.h>
#include <string.h>
#include <rwlock.h>
#include <time.h>
#include <utils.h>

storage_t* storage_create(size_t max_files, size_t max_capacity, replacement_policy_t rp) {
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
    storage->replacement_policy = rp;
    storage->number_of_files = 0;
    storage->max_files = max_files;
    storage->capacity = 0;
    storage->max_capacity = max_capacity;

    // Inizializzo le statistiche
    storage->start_timestamp = time(NULL);
    storage->max_files_reached = 0;
    storage->max_capacity_reached = 0;
    storage->rp_algorithm_counter = 0;

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
    memset(file->name, 0, sizeof(file->name));
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

    // Dati utili alla politica di rimpiazzo scelta
    file->creation_time = time(NULL); // Timestamp corrente
    file->last_use_time = file->creation_time; // Inizialmente, coincide con il tempo di creazione
    file->frequency = 0; // Si suppone che la creazione del file non conti come utilizzo dello stesso

    // Nota: non ha senso impostare inizialmente <last_use_time> a 0 in quanto un file appena creato è vuoto,
    // e da li a poco seguirà, tipicamente, una writeFile. Dal punto di vista della politica di rimpiazzo,
    // non conviene eliminare un file vuoto (con dimensione pari a 0), nonostante questo risulti come mai utilizzato.

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
    printf("Tempo di creazione: %ld\n", file->creation_time);
    printf("Ultimo utilizzo: %ld\n", file->last_use_time);
    printf("Numero di accessi: %u\n", file->frequency);
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

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);

    // Controllo se il file esiste all'interno dello storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    // Mantengo separata l'informazione sull'esistenza del file per leggibilità
    bool already_exists = (bool)file;

    // Gestisco prima tutte le possibili situazioni di errore
    // Flag O_CREATE settato e file già esistente
    if (create_flag && already_exists) {
        rwlock_done_read(storage->rwlock);
        errno = EEXIST;
        return -1;
    }
    // Flag O_CREATE non settato e file non esistente
    if (!create_flag && !already_exists) {
        rwlock_done_read(storage->rwlock);
        errno = ENOENT;
        return -1;
    }

    // I flags sono coerenti con lo stato dello storage, posso procedere
    // Distinguo il caso in cui il file esiste già da quello in cui non esiste ancora
    if (already_exists) { // && !O_CREATE
        // * Il file esiste già nello storage

        // Acquisisco l'accesso in lettura sul file
        rwlock_start_read(file->rwlock);

        // Controllo che il file non sia aperto in scrittura (locked) da un altro client
        if (file->writer != 0 && file->writer != client) {
            rwlock_done_read(file->rwlock);
            rwlock_done_read(storage->rwlock);
            errno = EACCES;
            return -1;
        }

        // Controllo che il client non abbia già aperto il file (almeno in lettura)
        // Qualora fosse già stato aperto in lettura, l'accesso in scrittura deve essere 
        // richiesto dal client tramite la API lockFile
        if (linked_list_find(file->readers, client)) {
            rwlock_done_read(file->rwlock);
            rwlock_done_read(storage->rwlock);
            errno = EEXIST;
            return -1;
        }

        // Rilascio l'accesso in lettura sul file
        rwlock_done_read(file->rwlock);
        // Acquisisco l'accesso in scrittura sul file
        rwlock_start_write(file->rwlock);

        // Apro il file in lettura per il client
        if(!linked_list_insert(file->readers, client)){
            // Errore di inserimento in lista
            rwlock_done_write(file->rwlock);
            rwlock_done_read(storage->rwlock);
            // Errno è settato da linked_list_insert
            return -1;
        }

        // Se il flag O_LOCK è stato settato, apro il file anche in scrittura per il client
        if(lock_flag) file->writer = client;

        // Aggioro le statistiche del file
        file->last_use_time = time(NULL);
        file->frequency++;

        // ! DEBUG
        storage_file_print(file); // Stampo alcune informazioni sul file

        // Ho terminato, rilascio le lock acquisite
        rwlock_done_write(file->rwlock);
        rwlock_done_read(storage->rwlock);

    } else { // && O_CREATE
        // * Il file non esiste ancora nello storage, lo creo
        
        // Controllo che nello storage ci sia effettivamente spazio in termini di numero di files
        // ? In questo caso occorre far partire la procedura di replace? Magari per espellere il file più piccolo?
        /* if (storage->number_of_files == storage->max_files) {
            errno = ENOSPC;
            return -1;
        } */

        // Rilascio l'accesso in lettura sullo storage
        rwlock_done_read(storage->rwlock);
        // TODO: metodo per switchare read<->write lock atomicamente
        // Acquisisco l'accesso in scrittura sullo storage
        rwlock_start_write(storage->rwlock);

        // Creo un nuovo file vuoto
        file = storage_file_create(pathname, NULL, 0);

        // * Non è necessario richiedere l'accesso in scrittura sul file perché non può essere ancora utilizzato da altri client

        // Lo apro in lettura per il client
        if(!linked_list_insert(file->readers, client)){
            // Errore di inserimento in lista
            storage_file_destroy(file);
            rwlock_done_write(storage->rwlock);
            // Errno viene settato da linked_list_insert
            return -1;
        }

        // Se il flag O_LOCK è stato settato, apro il file anche in scrittura per il client
        if(lock_flag) file->writer = client;

        // Inserisco il file nello storage
        if(!icl_hash_insert(storage->files, file->name, file)){
            // Se l'inserimento nello storage fallisce, libero la memoria e ritorno errore
            storage_file_destroy(file);
            rwlock_done_write(storage->rwlock);
            // Errno viene settato da icl_hash_insert
            return -1;
        }

        // Aggiorno le informazioni dello storage
        storage->number_of_files++; // Incremento il numero di file presenti nello storage
        storage->max_files_reached = MAX(storage->max_files_reached, storage->number_of_files);
        //printf("Lo storage contiene %d files\n", storage->number_of_files);

        // ! DEBUG
        //storage_file_print(file); // Stampo alcune informazioni sul file
        //icl_hash_dump(stdout, storage->files); // Stampo il contenuto dello storage

        // Rilascio l'accesso in scrittura sullo storage
        rwlock_done_write(storage->rwlock);
    }

    return 0;
}

int storage_read_file(storage_t* storage, const char* pathname, void** contents, size_t* size, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname || !contents || !size) {
        errno = EINVAL;
        return -1;
    }

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);

    // Controllo se il file esiste all'interno dello storage
    if (!file) {
        rwlock_done_read(storage->rwlock);
        errno = ENOENT;
        return -1;
    }

    // Acquisisco l'accesso in lettura sul file
    rwlock_start_read(file->rwlock);

    // Controllo che il client abbia aperto il file in lettura
    if (!linked_list_find(file->readers, client)) {
        rwlock_done_read(file->rwlock);
        rwlock_done_read(storage->rwlock);
        errno = EPERM;
        return -1;
    }

    // Rilascio l'accesso in lettura sul file
    rwlock_done_read(file->rwlock);
    // Acquisisco l'accesso in scrittura sul file
    rwlock_start_write(file->rwlock);

    // Copio il contenuto e la dimensione del file
    *size = file->size;
    *contents = malloc(file->size);
    memcpy(*contents, file->contents, file->size);

    // Aggioro le statistiche del file
    file->last_use_time = time(NULL);
    file->frequency++;

    // Rilascio l'accesso in scrittura sul file
    rwlock_done_write(file->rwlock);
    // Rilascio l'accesso in lettura sullo storage
    rwlock_done_read(storage->rwlock);
    
    return 0;
}

int storage_write_file(storage_t* storage, const char* pathname, const void* contents, size_t size, int* victims_no, storage_file_t*** victims, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname || !contents || size <= 0) {
        errno = EINVAL;
        return -1;
    }

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);

    // Prima di fare qualsiasi cosa, controllo che la dimensione del file che si vuole scrivere 
    //  non sia maggiore della capienza massima dello storage
    if(size > storage->max_capacity){
        rwlock_done_read(storage->rwlock);
        errno = ENOSPC;
        return -1;
    }

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);

    // Controllo che il file che si vuole scrivere esista nello storage
    if (!file) {
        rwlock_done_read(storage->rwlock);
        errno = ENOENT;
        return -1;
    }

    // Acquisisco l'accesso in lettura sul file
    rwlock_start_read(file->rwlock);

    // Controllo nuovamente che il file sia stato aperto in scrittura dal client
    if(file->writer != client){
        rwlock_done_read(file->rwlock);
        rwlock_done_read(storage->rwlock);
        errno = EPERM;
        return -1;
    }

    // Algoritmo di rimpiazzo
    *victims_no = 0;
    *victims = malloc(sizeof(storage_file_t*) * storage->number_of_files);  // Al più, rimuovo tutti i file presenti
    
    // Finché non c'è spazio sufficiente a contenere il nuovo file, seleziono file da rimuovere
    while ((storage->capacity - file->size) + size > storage->max_capacity) {
        // Seleziono il file da espellere
        storage_file_t* victim = (storage_file_t*)icl_hash_get_victim(storage->files, storage->replacement_policy, pathname);

        if (!victim) {
            // Non è stato possibile espelle alcun file, scrittura annullata
            rwlock_done_read(file->rwlock);
            rwlock_done_read(storage->rwlock);
            errno = ECANCELED;
            return -1;
        }

        // Effettuo una copia del file per poterlo inviare al client
        *victims[*victims_no] = storage_file_create(victim->name, victim->contents, victim->size);

        // Elimino il file dallo storage
        if(icl_hash_delete(storage->files, victim->name, NULL, &storage_file_destroy) == -1){
            // Errore nella cancellazione del file
            printf("Errore nella cancellazione del file\n");
        }

        // Aggiorno le informazioni dello storage
        storage->number_of_files--;                       // Decremento il numero di file nello storage
        storage->capacity -= (*victims)[*victims_no]->size;  // Libero lo spazio occupato dal file rimosso
        
        // Incremento il numero dei file espulsi
        (*victims_no)++;
    }

    printf("Victims number: %d\n", *victims_no);
    if(*victims && *victims_no > 0)
        for(int i=0;i < *victims_no;i++)
            printf("Victim n.%d: %s %d %p\n", i, (*victims)[i]->name, (*victims)[i]->size, (*victims)[i]->contents);

    if(*victims_no == 0) free(*victims);

    // Rilascio l'accesso in lettura sul file
    rwlock_done_read(file->rwlock);
    // Acquisisco l'accesso in scrittura sul file
    rwlock_start_write(file->rwlock);

    // Rimuovo le tracce di un eventuale file precedentemente scritto
    if(file->contents) free(file->contents);
    size_t old_size = file->size; // Usata in fondo per aggiornare le informazioni dello storage
    
    // Aggiorno il contenuto del file
    file->contents = contents;
    file->size = size;

    // Aggioro le statistiche del file
    file->last_use_time = time(NULL);
    file->frequency++;

    // Rilascio l'accesso in lettura sullo storage
    rwlock_done_read(storage->rwlock);
    // Acquisisco l'accesso in scrittura sullo storage
    rwlock_start_write(storage->rwlock);
    // Rilascio l'accesso in scrittura sul file
    rwlock_done_write(file->rwlock);

    // Aggiorno le informazioni dello storage
    // Sottraggo alla capacità dello storage quella occupata dal file che eventualmente ho sovrascritto,
    // quindi sommo la dimensione del nuovo file caricato
    storage->capacity = (storage->capacity - old_size) + size;
    storage->max_capacity_reached = MAX(storage->max_capacity_reached, storage->capacity);
    if(*victims_no > 0) storage->rp_algorithm_counter++;
    //printf("Lo storage occupa %d bytes\n", storage->capacity);

    // Rilascio l'accesso in scrittura sullo storage
    rwlock_done_write(storage->rwlock);
    
    return 0;
}

int storage_append_to_file(storage_t* storage, const char* pathname, const void* contents, size_t size, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname || !contents || size <= 0) {
        errno = EINVAL;
        return -1;
    }

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    
    // Rilascio l'accesso in lettura sullo storage
    rwlock_done_read(storage->rwlock);

    // Tutti i controlli del caso dovrebbero essere stati eseguiti dalla storage_eject_file
    // Tuttavia, effettuo comunque un controllo
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    // Acquisisco l'accesso in lettura sul file
    rwlock_start_read(file->rwlock);
    
    // Controllo nuovamente che il file sia stato aperto in scrittura dal client
    if(file->writer != 0 && file->writer != client){
        rwlock_done_read(file->rwlock);
        errno = EPERM;
        return -1;
    }

    // Rilascio l'accesso in lettura sul file
    rwlock_done_read(file->rwlock);
    // Acquisisco l'accesso in scrittura sul file
    rwlock_start_write(file->rwlock);

    // Amplio la memoria allocata per il file
    void* updated_contents = realloc(file->contents, file->size + size);
    if(!updated_contents) return -1;
    file->contents = updated_contents;
    // Aggiungo <contents> partendo dalla fine di <file->contents>
    memcpy(file->contents + file->size, contents, size);

    // Aggioro le statistiche del file
    file->last_use_time = time(NULL);
    file->frequency++;

    // Rilascio l'accesso in scrittura sul file
    rwlock_done_write(file->rwlock);

    // Acquisisco l'accesso in scrittura sullo storage
    rwlock_start_write(storage->rwlock);
 
    // Aggiorno la dimensione del file
    file->size += size;

    // Aggiorno le informazioni dello storage
    storage->capacity += size; // Sommo la dimensione del contenuto aggiunto
    storage->max_capacity_reached = MAX(storage->max_capacity_reached, storage->capacity);
    //printf("Lo storage occupa %d bytes\n", storage->capacity);

    // Rilascio l'accesso in scrittura sullo storage
    rwlock_done_write(storage->rwlock);

    return 0;
}

int storage_lock_file(storage_t* storage, const char* pathname, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    
    // Rilascio l'accesso in lettura sullo storage
    rwlock_done_read(storage->rwlock);

    // Controllo che il file esista
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    // Acquisisco l'accesso in lettura sul file
    rwlock_start_read(file->rwlock);

    // Se il file è gia aperto in scrittura per il client che ha effettuato la richiesta, ritorno immediatamente
    if(file->writer == client) return 0;

    // Se il file non è stato precedentemente aperto, almeno in lettura, dal client, non posso aprirlo in scrittura
    if(!linked_list_find(file->readers, client)) return -1;

    // Se il file non è stato aperto in scrittura da nessun client (= non è bloccato in scrittura)
    if(file->writer == 0){
        // Rilascio l'accesso in lettura sul file
        rwlock_done_read(file->rwlock);
        // Acquisisco l'accesso in scrittura sul file
        rwlock_start_write(file->rwlock);
        // Imposto il lock in scrittura sul file per il client 
        file->writer = client;
    }else{
        // Se il file è lockato da un altro client, aspetto che venga rilasciato
        // e solo successivamente acquisisco la lock
        // ! 
        // Rilascio l'accesso in lettura sul file
        rwlock_done_read(file->rwlock);
        // Acquisisco l'accesso in scrittura sul file
        rwlock_start_write(file->rwlock);
        // TODO: variabile di condizione sul lock su cui aspettare
        file->writer = client;
    }

    // Aggioro le statistiche del file
    file->last_use_time = time(NULL);
    file->frequency++;

    // Rilascio l'accesso in scrittura sul file
    rwlock_done_write(file->rwlock);

    return 0;
}

int storage_unlock_file(storage_t* storage, const char* pathname, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    
    // Rilascio l'accesso in lettura sullo storage
    rwlock_done_read(storage->rwlock);
    
    // Controllo che il file esista
    if (!file) {
        errno = ENOENT;
        return -1;
    }

    // Acquisisco l'accesso in lettura sul file
    rwlock_start_read(file->rwlock);

    if(file->writer == 0 || file->writer != client){
        // Il file non è attualmente lockato in scrittura, oppure
        // la lock è detenuta da un client diverso
        rwlock_done_read(file->rwlock);
        errno = ENOLCK;
        return -1;
    }

    // Rilascio l'accesso in lettura sul file
    rwlock_done_read(file->rwlock);
    // Acquisisco l'accesso in scrittura sul file
    rwlock_start_write(file->rwlock);

    // A questo punto, file->writer sarà pari a client, per costruzione, 
    // ovvero client ha in precedenza aperto il file in scrittura
    // Rilascio quindi la lock
    file->writer = 0;
    // Il file rimarrà aperto in lettura

    // Aggioro le statistiche del file
    file->last_use_time = time(NULL);
    file->frequency++;

    // Rilascio l'accesso in scrittura sul file
    rwlock_done_write(file->rwlock);

    return 0;
}

int storage_close_file(storage_t* storage, const char* pathname, int client) {
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);

    // Controllo se il file esiste all'interno dello storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);

    // Se non esiste, ritorno subito errore
    if (!file) {
        rwlock_done_read(storage->rwlock);
        errno = ENOENT;
        return -1;
    }

    // Acquisisco l'accesso in lettura sul file
    rwlock_start_read(file->rwlock);

    // Controllo che <client> abbia precedentemente eseguito la openFile
    if (!linked_list_find(file->readers, client)) {
        rwlock_done_read(file->rwlock);
        rwlock_done_read(storage->rwlock);
        errno = ENOLCK;
        return -1;
    }

    // Rilascio l'accesso in lettura sullo storage
    rwlock_done_read(file->rwlock);
    // Acquisisco l'accesso in scrittura sullo storage
    rwlock_start_write(file->rwlock);

    // Chiudo il file in scrittura per il client, se era stato aperto con questa modalità
    // TODO: chiamare internamente la unlockFile ?
    if (file->writer != 0 && file->writer == client) file->writer = 0;

    // Chiudo il file in lettura per il client
    if(!linked_list_remove(file->readers, client)){
        rwlock_done_write(file->rwlock);
        rwlock_done_read(storage->rwlock);
        // Errno è settato da linked_list_remove
        return -1;
    }

    // Aggioro le statistiche del file
    file->last_use_time = time(NULL);
    file->frequency++;
    
    // ! Debug
    storage_file_print(file);

    // Rilascio l'accesso in scrittura sul file
    rwlock_done_write(file->rwlock);
    // Rilascio l'accesso in lettura sullo storage
    // TODO: è possibile rilasciarlo anche prima ?
    rwlock_done_read(storage->rwlock);

    return 0;
}

int storage_remove_file(storage_t* storage, const char* pathname, int client){
    // Controllo la validità degli argomenti
    if (!storage || !pathname) {
        errno = EINVAL;
        return -1;
    }

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);

    // Recupero il file dallo storage
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    
    // Controllo che il file esista
    if (!file) {
        rwlock_done_read(storage->rwlock);
        errno = ENOENT;
        return -1;
    }
    
    // Acquisisco l'accesso in lettura sul file
    rwlock_start_read(file->rwlock);

    if(file->writer == 0 || file->writer != client){
        // Il file non è attualmente lockato in scrittura, oppure
        // la lock è detenuta da un client diverso
        rwlock_done_read(file->rwlock);
        rwlock_done_read(storage->rwlock);
        errno = ENOLCK;
        return -1;
    }
    
    // Usata in fondo per aggiornare le informazioni dello storage
    size_t old_size = file->size;

    // Rilascio l'accesso in lettura sullo storage
    rwlock_done_read(storage->rwlock);
    // Acquisisco l'accesso in scrittura sullo storage
    rwlock_start_write(storage->rwlock);
    // Rilascio l'accesso in lettura sul file
    rwlock_done_read(file->rwlock);
    
    // A questo punto, file->writer sarà pari a client, per costruzione, 
    // ovvero client ha in precedenza aperto il file in scrittura
    // Cancello quindi il file dallo storage
    if(icl_hash_delete(storage->files, pathname, NULL, &storage_file_destroy) == -1){
        // TODO: funzione di free per file
        rwlock_done_write(storage->rwlock);    
        return -1;
    }
    
    //icl_hash_dump(stdout, storage->files);

    // Aggiorno le informazioni dello storage
    storage->number_of_files--; // Decremento il numero di file nello storage
    storage->capacity -= old_size; // Libero lo spazio occupato dal file rimosso
    //printf("Lo storage contiene %d files\n", storage->number_of_files);
    //printf("Lo storage occupa %d bytes\n", storage->capacity);

    // Rilascio l'accesso in scrittura sul file
    rwlock_done_write(storage->rwlock);

    return 0;
}

int storage_eject_file(storage_t* storage, const char* pathname, size_t size, int client, storage_file_t** victims){
    // Controllo la validità degli argomenti
    if(!storage || !pathname || size <= 0 || !victims){
        errno = EINVAL;
        return -1;
    }

    // Acquisisco l'accesso in lettura sullo storage
    rwlock_start_read(storage->rwlock);
    
    // Controllo che la dimensione del file non sia maggiore dell'intero storage
    if(size > storage->max_capacity){
        rwlock_done_read(storage->rwlock);
        errno = ENOSPC;
        return -1;
    }

    // Controllo che il file esista nello storage (e.g. è stato creato con storage_open_file)
    storage_file_t* file = icl_hash_find(storage->files, pathname);
    if(!file){
        rwlock_done_read(storage->rwlock);
        errno = ENOENT;
        return -1;
    }

    // Acquisisco l'accesso in lettura sul file
    rwlock_start_read(file->rwlock);

    // Controllo che il file sia stato aperto in scrittura dal client
    if(file->writer != 0 && file->writer != client){
        rwlock_done_read(file->rwlock);
        rwlock_done_read(storage->rwlock);
        errno = EPERM;
        return -1;
    }

    // Rilascio l'accesso in lettura sullo storage
    rwlock_done_read(storage->rwlock);
    // Acquisisco l'accesso in scrittura sullo storage
    rwlock_start_write(storage->rwlock);

    // Controllo che ci sia spazio libero a sufficienza per salvare il file
    if(size > storage->capacity){
        // Se non c'è spazio, decido qui quali file espellere dallo storage
        
        int victims_no = 0; // Numero dei file espulsi dallo storage
        
        // TODO: replacement mechanism
        
        return victims_no;
    }

    // Rilascio l'accesso in lettura sul file
    rwlock_done_read(file->rwlock);

    return 0;
}

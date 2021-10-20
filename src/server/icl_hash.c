/**
 * @file icl_hash.c
 *
 * Dependency free hash table implementation.
 *
 * This simple hash table implementation should be easy to drop into
 * any other peice of code, it does not depend on anything else :-)
 * 
 * @author Jakub Kurzak
 */
/* $Id: icl_hash.c 2838 2011-11-22 04:25:02Z mfaverge $ */
/* $UTK_Copyright: $ */

#include "icl_hash.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils.h>

#define BITS_IN_int (sizeof(int) * CHAR_BIT)
#define THREE_QUARTERS ((int)((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH ((int)(BITS_IN_int / 8))
#define HIGH_BITS (~((unsigned int)(~0) >> ONE_EIGHTH))

/**
 * A simple string hash.
 *
 * An adaptation of Peter Weinberger's (PJW) generic hashing
 * algorithm based on Allen Holub's version. Accepts a pointer
 * to a datum to be hashed and returns an unsigned integer.
 * From: Keith Seymour's proxy library code
 *
 * @param[in] key -- the string to be hashed
 *
 * @returns the hash index
 */
unsigned int
hash_pjw(void *key) {
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if (!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

int string_compare(void *a, void *b) {
    return (strcmp((char *)a, (char *)b) == 0);
}

/**
 * Create a new hash table.
 *
 * @param[in] nbuckets -- number of buckets to create
 * @param[in] hash_function -- pointer to the hashing function to be used
 * @param[in] hash_key_compare -- pointer to the hash key comparison function to be used
 *
 * @returns pointer to new hash table.
 */

icl_hash_t *
icl_hash_create(int nbuckets, unsigned int (*hash_function)(void *), int (*hash_key_compare)(void *, void *)) {
    icl_hash_t *ht;
    int i;

    ht = (icl_hash_t *)malloc(sizeof(icl_hash_t));
    if (!ht) return NULL;

    ht->nentries = 0;
    ht->buckets = (icl_entry_t **)malloc(nbuckets * sizeof(icl_entry_t *));
    if (!ht->buckets) return NULL;

    ht->nbuckets = nbuckets;
    for (i = 0; i < ht->nbuckets; i++)
        ht->buckets[i] = NULL;

    ht->hash_function = hash_function ? hash_function : hash_pjw;
    ht->hash_key_compare = hash_key_compare ? hash_key_compare : string_compare;

    return ht;
}

/**
 * Search for an entry in a hash table.
 *
 * @param ht -- the hash table to be searched
 * @param key -- the key of the item to search for
 *
 * @returns pointer to the data corresponding to the key.
 *   If the key was not found, returns NULL.
 */

void *
icl_hash_find(icl_hash_t *ht, void *key) {
    icl_entry_t *curr;
    unsigned int hash_val;

    if (!ht || !key) return NULL;

    hash_val = (*ht->hash_function)(key) % ht->nbuckets;

    for (curr = ht->buckets[hash_val]; curr != NULL; curr = curr->next)
        if (ht->hash_key_compare(curr->key, key))
            return (curr->data);

    return NULL;
}

/**
 * Insert an item into the hash table.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 *
 * @returns pointer to the new item.  Returns NULL on error.
 */

icl_entry_t *
icl_hash_insert(icl_hash_t *ht, void *key, void *data) {
    icl_entry_t *curr;
    unsigned int hash_val;

    if (!ht || !key) return NULL;

    hash_val = (*ht->hash_function)(key) % ht->nbuckets;

    for (curr = ht->buckets[hash_val]; curr != NULL; curr = curr->next)
        if (ht->hash_key_compare(curr->key, key))
            return (NULL); /* key already exists */

    /* if key was not found */
    curr = (icl_entry_t *)malloc(sizeof(icl_entry_t));
    if (!curr) return NULL;

    curr->key = key;
    curr->data = data;
    curr->next = ht->buckets[hash_val]; /* add at start */

    ht->buckets[hash_val] = curr;
    ht->nentries++;

    return curr;
}

/**
 * Replace entry in hash table with the given entry.
 *
 * @param ht -- the hash table
 * @param key -- the key of the new item
 * @param data -- pointer to the new item's data
 * @param olddata -- pointer to the old item's data (set upon return)
 *
 * @returns pointer to the new item.  Returns NULL on error.
 */

icl_entry_t *
icl_hash_update_insert(icl_hash_t *ht, void *key, void *data, void **olddata) {
    icl_entry_t *curr, *prev;
    unsigned int hash_val;

    if (!ht || !key) return NULL;

    hash_val = (*ht->hash_function)(key) % ht->nbuckets;

    /* Scan bucket[hash_val] for key */
    for (prev = NULL, curr = ht->buckets[hash_val]; curr != NULL; prev = curr, curr = curr->next)
        /* If key found, remove node from list, free old key, and setup olddata for the return */
        if (ht->hash_key_compare(curr->key, key)) {
            if (olddata != NULL) {
                *olddata = curr->data;
                free(curr->key);
            }

            if (prev == NULL)
                ht->buckets[hash_val] = curr->next;
            else
                prev->next = curr->next;
        }

    /* Since key was either not found, or found-and-removed, create and prepend new node */
    curr = (icl_entry_t *)malloc(sizeof(icl_entry_t));
    if (curr == NULL) return NULL; /* out of memory */

    curr->key = key;
    curr->data = data;
    curr->next = ht->buckets[hash_val]; /* add at start */

    ht->buckets[hash_val] = curr;
    ht->nentries++;

    if (olddata != NULL && *olddata != NULL)
        *olddata = NULL;

    return curr;
}

/**
 * Free one hash table entry located by key (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param key -- the key of the new item
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int icl_hash_delete(icl_hash_t *ht, void *key, void (*free_key)(void *), void (*free_data)(void *)) {
    icl_entry_t *curr, *prev;
    unsigned int hash_val;

    if (!ht || !key) return -1;
    hash_val = (*ht->hash_function)(key) % ht->nbuckets;

    prev = NULL;
    for (curr = ht->buckets[hash_val]; curr != NULL;) {
        if (ht->hash_key_compare(curr->key, key)) {
            if (prev == NULL) {
                ht->buckets[hash_val] = curr->next;
            } else {
                prev->next = curr->next;
            }
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data) (*free_data)(curr->data);
            ht->nentries++;
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

/**
 * Free hash table structures (key and data are freed using functions).
 *
 * @param ht -- the hash table to be freed
 * @param free_key -- pointer to function that frees the key
 * @param free_data -- pointer to function that frees the data
 *
 * @returns 0 on success, -1 on failure.
 */
int icl_hash_destroy(icl_hash_t *ht, void (*free_key)(void *), void (*free_data)(void *)) {
    icl_entry_t *bucket, *curr, *next;
    int i;

    if (!ht) return -1;

    for (i = 0; i < ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for (curr = bucket; curr != NULL;) {
            next = curr->next;
            if (*free_key && curr->key) (*free_key)(curr->key);
            if (*free_data && curr->data) (*free_data)(curr->data);
            free(curr);
            curr = next;
        }
    }

    if (ht->buckets) free(ht->buckets);
    if (ht) free(ht);

    return 0;
}

/**
 * Dump the hash table's contents to the given file pointer.
 *
 * @param stream -- the file to which the hash table should be dumped
 * @param ht -- the hash table to be dumped
 *
 * @returns 0 on success, -1 on failure.
 */

int icl_hash_dump(FILE *stream, icl_hash_t *ht) {
    icl_entry_t *bucket, *curr;
    int i;

    if (!ht) return -1;

    for (i = 0; i < ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for (curr = bucket; curr != NULL;) {
            if (curr->key)
                fprintf(stream, "icl_hash_dump: %s: %p\n", (char *)curr->key, curr->data);
            curr = curr->next;
        }
    }

    return 0;
}

#include <time.h>
#include <limits.h>
#include <storage.h>

void* icl_hash_get_victim(icl_hash_t* ht, replacement_policy_t rp, const char* pathname){ 
    if (!ht) return NULL; // todo errno
    
    icl_entry_t *bucket, *curr;

    storage_file_t* victim_name = NULL;
    time_t victim_creation_time = LONG_MAX;
    time_t victim_last_use_time = LONG_MAX;
    unsigned int victim_frequency = UINT_MAX;

    for (int i = 0; i < ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for (curr = bucket; curr != NULL;) {
            if (curr->key){
                // Se incontro proprio il file che sto tentando di scrivere, lo salto
                if(strcmp(((storage_file_t*) (curr->data))->name, pathname) == 0){curr = curr->next; continue;}

                // Logica dell'algoritmo di rimpiazzo, in base alla politica scelta in fase di configurazione
                storage_file_t* current_file = (storage_file_t*) curr->data;
                
                // TODO: controllare che i file non siano aperti in lettura/scrittura da altri client
                
                switch(rp){
                    case FIFO:
                        // Viene selezionato il file presente nello storage da più tempo,
                        //  ovvero quello con creation time più basso
                        if(current_file->creation_time < victim_creation_time){
                            victim_creation_time = current_file->creation_time;
                            victim_name = current_file;
                        }
                        break;
                    case LRU:
                        // Viene selezionato il file non utilizzato da più tempo,
                        //  ovvero quello con last use time più basso
                        if(current_file->last_use_time < victim_last_use_time){
                            victim_last_use_time = current_file->last_use_time;
                            victim_name = current_file;
                        }
                        break;
                    case LFU:
                        // Viene selezionato il file utilizzato meno frequentemente,
                        //  ovvero quello con frequency più basso
                        if(current_file->frequency < victim_frequency){
                            victim_frequency = current_file->frequency;
                            victim_name = current_file;
                        }
                        break;
                }
            }
            curr = curr->next;
        }
    }

    return victim_name;
}

int icl_hash_get_n_files(icl_hash_t* ht, int N, void*** files){ 
    if (!ht) return -1; // todo errno
    
    storage_file_t*** read_files = (storage_file_t***) files;
    icl_entry_t *bucket, *curr;
    int index = 0;
    storage_file_t* current_file = NULL;
    for (int i = 0; i < ht->nbuckets && index < N; i++) {
        bucket = ht->buckets[i];
        for (curr = bucket; curr != NULL && index < N; curr = curr->next) {
            if (curr->key){
                // Prendo il file corrente
                current_file = (storage_file_t*) curr->data;
                //printf("%s %p %zu\n", current_file->name, current_file->contents, current_file->size);
                // Salto i file vuoti
                if(current_file->size == 0) continue;
                storage_file_t* new_file = storage_file_create(current_file->name, current_file->contents, current_file->size);
                (*read_files)[index] = new_file;
                index++;
            }
        }
    }

    return index;
}

void icl_hash_print(icl_hash_t* ht){
    if (!ht) return;
    
    int i;
    int counter = 1;
    icl_entry_t *bucket, *curr;
    storage_file_t* file = NULL;

    for (i = 0; i < ht->nbuckets; i++) {
        bucket = ht->buckets[i];
        for (curr = bucket; curr != NULL; curr = curr->next) {
            if (curr->key){
                file = (storage_file_t*) curr->data;
                char* human_readable_size = calculate_size(file->size);
                fprintf(stdout, "[%d] (%s) %s\n", counter++, human_readable_size , file->name);
                free(human_readable_size);
            }
        }
    }
}
// @author Luca Cirillo (545480)

#include <icl_hash.h>
#include <stdlib.h>
#include <storage.h>
#include <string.h>
#include <errno.h>

storage_t* storage_create(size_t buckets) {
    // Controllo la validità degli argomenti
    if (buckets == 0) {
        errno = EINVAL;
        return NULL;
    }

    storage_t* storage = malloc(sizeof(storage_t));
    if (!storage) return NULL;
    storage->files = icl_hash_create(buckets, NULL, NULL);
    if (!storage->files) {
        free(storage);
        return NULL;
    }
    return storage;
}

void storage_destroy(storage_t* storage){
    if(!storage) return;
    icl_hash_destroy(storage->files, NULL, NULL); // TODO: free key-data
    free(storage);
}

storage_file_t* storage_file_create(const char* name, const void* contents, size_t size){
    if(!name){
        errno = EINVAL;
        return NULL;
    }
    storage_file_t* file = malloc(sizeof(storage_file_t));
    if(!file) return NULL;

    if((file->name = malloc(strlen(name) + 1)) == NULL){
        free(file);
        return NULL;
    }
    strncpy(file->name, name, strlen(name) + 1);
    
    if(contents && size != 0){
        if((file->contents = malloc(size))== NULL){
            free(file->name);
            free(file);
            return NULL;
        }
        memcpy(file->contents, contents, size);
        file->size = size;
    }
    return file;
}

void storage_file_destroy(storage_file_t* file){
    if(!file) return;
    free(file->name);
    free(file->contents);
    free(file);
}
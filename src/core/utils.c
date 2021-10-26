// @author Luca Cirillo (545480)

#include <constants.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils.h>

int readn(long fd, void* buf, size_t size) {
    size_t left = size;
    int r;
    char* bufptr = (char*)buf;
    while (left > 0) {
        if ((r = read((int)fd, bufptr, left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;  // EOF
        left -= r;
        bufptr += r;
    }
    return size;
}

int writen(long fd, void* buf, size_t size) {
    size_t left = size;
    int r;
    char* bufptr = (char*)buf;
    while (left > 0) {
        if ((r = write((int)fd, bufptr, left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;
        left -= r;
        bufptr += r;
    }
    return 1;
}

// * Converte una stringa in un numero
int is_number(const char* arg, long* num) {
    char* string = NULL;
    long value = strtol(arg, &string, 10);

    if (errno == ERANGE) {
        perror("Error: an overflow occurred using is_number");
        exit(errno);
    }

    if (string != NULL && *string == (char)0) {
        *num = value;
        return 1;
    }
    return 0;
}

// * Crea ricorsivamente la struttura di cartella definita in <path>
int mkdir_p(const char* path) {
    char abs_path[PATH_MAX];
    const size_t length = strlen(path);
    char* p = NULL;

    // Copio la stringa per poterla modificare
    if (length > sizeof(abs_path) - 1) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(abs_path, path);

    // Itero sulla stringa per identificare tutte le componenti
    for (p = abs_path + 1; *p; p++) {
        // Quando incontro uno slash
        if (*p == '/') {
            // "Termino" temporaneamente la stringa
            *p = '\0';
            // Creo la cartella sul disco
            if (mkdir(abs_path, S_IRWXU) != 0)
                // E' possibile che la cartella esista già, solo in questo caso ignoro l'errore
                if (errno != EEXIST)
                    return -1;
            // Ripristino lo slash precedentemente modificato
            *p = '/';
        }
    }

    return 0;
}

static const char* sizes[] = {"EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B"};
static const uint64_t exbibytes = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

char* calculate_size(uint64_t size) {
    int i;
    char* result = malloc(20);
    uint64_t multiplier = exbibytes;

    for (i = 0; i < DIM(sizes); i++, multiplier /= 1024) {
        if (size < multiplier)
            continue;
        if (size % multiplier == 0)
            sprintf(result, "%" PRIu64 " %s", size / multiplier, sizes[i]);
        else
            sprintf(result, "%.1f %s", (float)size / multiplier, sizes[i]);
        return result;
    }
    strcpy(result, "0");
    return result;
}

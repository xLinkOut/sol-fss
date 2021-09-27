// @author Luca Cirillo (545480)

#include <utils.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

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
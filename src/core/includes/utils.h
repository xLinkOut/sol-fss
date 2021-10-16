// @author Luca Cirillo (545480)

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define LOCK(lock)                               \
    if (pthread_mutex_lock(lock) != 0) {         \
        fprintf(stderr, "Error: LOCK failed\n"); \
        pthread_exit((void*)EXIT_FAILURE);       \
    }

#define UNLOCK(lock)                               \
    if (pthread_mutex_unlock(lock) != 0) {         \
        fprintf(stderr, "Error: UNLOCK failed\n"); \
        pthread_exit((void*)EXIT_FAILURE);         \
    }

#define WAIT(cond, lock)                         \
    if (pthread_cond_wait(cond, lock) != 0) {    \
        fprintf(stderr, "Error: WAIT failed\n"); \
        pthread_exit((void*)EXIT_FAILURE);       \
    }

#define SIGNAL(cond)                               \
    if (pthread_cond_signal(cond) != 0) {          \
        fprintf(stderr, "Error: SIGNAL failed\n"); \
        pthread_exit((void*)EXIT_FAILURE);         \
    }

#define BROADCAST(cond)                               \
    if (pthread_cond_broadcast(cond) != 0) {          \
        fprintf(stderr, "Error: BROADCAST failed\n"); \
        pthread_exit((void*)EXIT_FAILURE);            \
    }

int readn(long fd, void* buf, size_t size);
int writen(long fd, void* buf, size_t size);
int is_number(const char* arg, long* num);
int mkdir_p(const char* path);

#endif

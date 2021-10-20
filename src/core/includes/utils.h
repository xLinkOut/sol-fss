// @author Luca Cirillo (545480)

#ifndef _UTILS_H_
#define _UTILS_H_

#include <inttypes.h>
#include <stdlib.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define DIM(x) (sizeof(x) / sizeof(*(x)))

static const char* sizes[] = {"EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B"};
static const uint64_t exbibytes = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

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

int mkdir_p(const char* path);
int is_number(const char* arg, long* num);
int readn(long fd, void* buf, size_t size);
int writen(long fd, void* buf, size_t size);
char* calculate_size(uint64_t size);

#endif

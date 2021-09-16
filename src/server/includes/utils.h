// @author Luca Cirillo (545480)

#ifndef _UTIL_H
#define _UTIL_H

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

#endif

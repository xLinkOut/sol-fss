// @author Luca Cirillo (545480)

// * Readers/Writers Lock, con politica "writers preferred"

#include <pthread.h>
#include <rwlock.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// * Questa implementazione di Readers/Writers Lock adotta una politica "writers preferred", ovvero
//  un lettore deve aspettare se ci sono scrittori attivi o in attesa, mentre
//  uno scrittore deve aspettare se ci sono lettori attivi o scrittori attivi.
// Questa politica previene il caso in cui un flusso continuo di lettori in arrivo "taglia fuori"
//  tutte le richieste provenienti da scrittori, che rimarrebbero in attesa per un tempo indefinito

struct RWLock {
    // Variabili di sincronizzazione
    pthread_mutex_t mutex;    // Accesso mutualmente esclusivo
    pthread_cond_t read_go;   // Attesa per start_read()
    pthread_cond_t write_go;  // Attesa per start_write()

    // Variabili di stato
    // Il comportamento del RWLock è completamente determinato
    //  dal numero di threads che leggono o scrivono, e
    //  dal numero di threads che vogliono leggere o scrivere
    int active_readers;
    int active_writers;
    int waiting_readers;
    int waiting_writers;
};

rwlock_t* rwlock_create() {
    rwlock_t* rwlock = malloc(sizeof(rwlock_t));
    if (!rwlock) return NULL;

    // Azzero la memoria allocata
    memset(rwlock, 0, sizeof(rwlock_t));

    // Inizializzo i parametri relativi a lettori e scrittori
    rwlock->active_readers = 0;
    rwlock->active_writers = 0;
    rwlock->waiting_readers = 0;
    rwlock->waiting_readers = 0;

    // Inizializzo l'accesso esclusivo
    if (pthread_mutex_init(&rwlock->mutex, NULL) != 0) {
        //perror("Error: unable to init lock mutex");
        free(rwlock);
        return NULL;
    }

    // Inizializzo la condizione di attesa per i lettori
    if (pthread_cond_init(&rwlock->read_go, NULL) != 0) {
        //perror("Error: unable to init lock read_go condition variable");
        pthread_mutex_destroy(&rwlock->mutex);
        free(rwlock);
        return NULL;
    }

    // Inizializzo la condizione di attesa per gli scrittori
    if (pthread_cond_init(&rwlock->write_go, NULL) != 0) {
        //perror("Error: unable to init lock write_go condition variable");
        pthread_mutex_destroy(&rwlock->mutex);
        pthread_cond_destroy(&rwlock->read_go);
        free(rwlock);
        return NULL;
    }

    // Ritorno il RWLock
    return rwlock;
}

void rwlock_destroy(rwlock_t* rwlock) {
    if (!rwlock) return;
    pthread_mutex_destroy(&rwlock->mutex);
    pthread_cond_destroy(&rwlock->read_go);
    pthread_cond_destroy(&rwlock->write_go);
    free(rwlock);
}

bool read_should_wait(rwlock_t* rwlock) {
    if (!rwlock) return false;
    // * Un lettore deve aspettare se sono presenti scrittori attivi oppure in attesa
    return (rwlock->active_writers > 0 || rwlock->waiting_writers > 0);
}

bool write_should_wait(rwlock_t* rwlock) {
    if (!rwlock) return false;
    // * Uno scrittore deve aspettare se sono presenti scrittori attivi oppure lettori attivi
    return (rwlock->active_writers > 0 || rwlock->active_readers > 0);
}

bool rwlock_start_read(rwlock_t* rwlock) {
    if (!rwlock) return false;
    if (pthread_mutex_lock(&rwlock->mutex) != 0)  // lock.acquire()
        return false;

    // Incremento il numero di lettori in attesa
    rwlock->waiting_readers++;

    // Potrebbe dover aspettare prima di continuare
    while (read_should_wait(rwlock)) {
        if (pthread_cond_wait(&rwlock->read_go, &rwlock->mutex) != 0)
            return false;
    }

    // Superato il controllo, il lettore passa da 'in attesa' a 'attivo
    rwlock->waiting_readers--;
    rwlock->active_readers++;

    if (pthread_mutex_unlock(&rwlock->mutex) != 0)  // lock.release()
        return false;

    return true;
}

bool rwlock_done_read(rwlock_t* rwlock) {
    if (!rwlock) return false;
    if (pthread_mutex_lock(&rwlock->mutex) != 0)  // lock.acquire()
        return false;

    // Il lettore ha terminato, non è più attivo
    rwlock->active_readers--;

    // A questo punto, si possono verificare due scenari:
    //  (a) non ci sono scrittori in attesa, quindi non faccio nulla in quanto
    //      la presenza di un lettore attivo non toglie la possibilità ad altri lettori di lavorare, oppure
    //  (b) c'è (almeno) uno scrittore in attesa e io sono l'ultimo lettore attivo,
    //      quindi posso risvegliare uno degli scrittori in modo che possa procedere
    if (rwlock->active_readers == 0 && rwlock->waiting_writers > 0) {
        if (pthread_cond_signal(&rwlock->write_go) != 0)
            return false;
    }

    if (pthread_mutex_unlock(&rwlock->mutex) != 0)  // lock.release()
        return false;

    return true;
}

bool rwlock_start_write(rwlock_t* rwlock) {
    if (!rwlock) return false;
    if (pthread_mutex_lock(&rwlock->mutex) != 0)  // lock.acquire()
        return false;

    // Incremento il numero di scrittori in attesa
    rwlock->waiting_writers++;

    // Potrebbe dover aspettare prima di continuare
    while (write_should_wait(rwlock)) {
        if (pthread_cond_wait(&rwlock->write_go, &rwlock->mutex) != 0)
            return false;
    }

    // Superato il controllo, lo scrittore passa da 'in attesa' a 'attivo
    rwlock->waiting_writers--;
    rwlock->active_writers++;

    if (pthread_mutex_unlock(&rwlock->mutex) != 0)  // lock.release()
        return false;

    return true;
}

bool rwlock_done_write(rwlock_t* rwlock) {
    if (!rwlock) return false;
    if (pthread_mutex_lock(&rwlock->mutex) != 0)  // lock.acquire()
        return false;

    // Lo scrittore ha terminato, non è più attivo
    rwlock->active_writers--;

    // A questo punto, si possono verificare due scenari:
    // (a) c'è (almeno) uno scrittore in attesa, quindi lo risveglio così che possa procedere
    // (b) non c'è nessuno scrittore in attesa, quindi risveglio tutti i lettori in attesa
    if (rwlock->waiting_writers == 0) {
        if (pthread_cond_signal(&rwlock->write_go) != 0)
            return false;
    } else {
        if (pthread_cond_broadcast(&rwlock->read_go) != 0)
            return false;
    }

    if (pthread_mutex_unlock(&rwlock->mutex) != 0)  // lock.release()
        return false;

    return true;
}

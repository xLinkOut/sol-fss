// @author Luca Cirillo (545480)

// * Readers/Writers Lock

#include <pthread.h>
#include <stdbool.h>

// Per evitare l'accesso alla struttura dati che controlla il lock,
//  la definizione della stessa è stata omessa dall'header di RWLock.
//  E' riportata solo la definizione del tipo, così che possa essere utilizzata all'esterno
typedef struct RWLock rwlock_t;

// * Crea un nuovo RWLock e restituisce un puntatore ad esso
rwlock_t* rwlock_create();
// * Elimina un RWLock esistente, creato con rwlock_create
void rwlock_destroy(rwlock_t* rwlock);
// * Acquisisce il lock in lettura
bool rwlock_start_read(rwlock_t* rwlock);
// * Rilascia il lock in lettura
bool rwlock_done_read(rwlock_t* rwlock);
// * Acquisisce il lock in scrittura
bool rwlock_start_write(rwlock_t* rwlock);
// * Rilascia il lock in scrittura
bool rwlock_done_write(rwlock_t* rwlock);

// I metodi read_should_wait e write_should_wait vengono utilizzati solo internamente,
//  quindi sono stati omessi dall'header di RWLock
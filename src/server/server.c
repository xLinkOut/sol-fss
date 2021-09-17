// @author Luca Cirillo (545480)

//#define _POSIX_C_SOURCE 199309L // Sigaction

#include <errno.h>
#include <pthread.h>
#include <queue.h>
#include <server_config.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

// TODO: liberare la memoria prima di uscire in caso di errore

// TODO: spostare in un file di utilities
#define BUFFER_SIZE 512

#define CONCURRENT_CONNECTIONS 8

// Struttura dati per passare più argomenti ai threads worker
typedef struct worker_args {
    int pipe_output;
    Queue_t* task_queue;
} worker_args_t;

// TODO: spostare nel file di gestione dei segnali
sig_atomic_t stop = 0;        // SIGHUP
sig_atomic_t force_stop = 0;  // SIGINT e SIGQUIT
static void* signals_handler(void* sigset) {
    int error;
    int signal;
    // Aspetto l'arrivo di un segnale tra SIGINT, SIGQUIT e SIGHUP
    if ((error = sigwait((sigset_t*)sigset, &signal)) != 0) {
        fprintf(stderr, "Error: something wrong happened while waiting for signals!\n");
        exit(error);
    }

    switch (signal) {
        /* 
            ! SIGINT, SIGQUIT: il server termina il prima possibile:
                non accetta nuove richieste da parte dei client connessi
                né da nuovi client, chiude quindi tutte le connessioni attive
                ma stampa comunque il sunto delle statistiche.
            * SIGINT e SIGQUIT vengono di fatto gestiti nello stesso modo.
        */
        case SIGINT:
        case SIGQUIT:
            force_stop = 1;
            printf("Info: SIGINT or SIGQUIT received\n");
            break;

        /*
            ! SIGHUP: il server completa le richieste, quindi termina:
                non accetta nuove richieste da parte di nuovi client
                ma vengono comunque servite tutte le richieste dei client connessi 
                al momento della ricezione del segnale.
                Il server terminerà solo quando tutti i client connessi chiuderanno la connessione.
        */
        case SIGHUP:
            stop = 1;
            printf("Info: SIGHUP received\n");
            break;

        default:
            break;
    }

    return NULL;
}

// TODO: spostare in un file per i workers
static void* worker(void* args) {
    worker_args_t* worker_args = (worker_args_t*)args;

    int fd;

    // ! MAIN WORKER LOOP
    // TODO: condizione while, eseguo finché !force_stop, ma nel caso di stop? Come faccio a eseguire finché i client non finiscono?
    while(1){
        fd = (int)queue_pop(worker_args->task_queue);
        printf("Worker on %d\n", fd);
    }

    return NULL;
}

// TODO: spostare in un file di utilities
// Converte una stringa in un numero
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

// File di log
FILE* log_file = NULL;

// Logga su file un particolare evento, con un certo livello di importanza
// 'level' si suppone sia uno tra INFO, DEBUG, WARN, ERROR
// TODO: spostare in un altro file
void log_event(const char* level, const char* message) {
    time_t timer = time(NULL);
    struct tm* tm_info = localtime(&timer);
    char date_time[20];
    strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log_file, "%s | %s\t| %s\n", date_time, level, message);
}

// TODO: routine di cleanup per la chiusura su errore del server
int main(int argc, char* argv[]) {
    // ! Parametri in ingresso:
    //  * 1. <config_path>: path al file di configurazione
    //    Se non viene specificato, si assume che il file di configurazione
    //    sia presente nella directory corrente con il nome 'config.txt'

    if (argc == 1) {
        // * <config_path> non specificato
        if ((CONFIG_PATH = (char*)malloc(sizeof(char) * (strlen(DEFAULT_CONFIG_PATH) + 1))) == NULL) {
            perror("Error: unable to allocate memory using malloc");
            return errno;
        }
        strncpy(CONFIG_PATH, DEFAULT_CONFIG_PATH, strlen(DEFAULT_CONFIG_PATH) + 1);
        printf("Warning: missing <config_path>, assuming '%s' is in the current directory.\n", CONFIG_PATH);
    } else if (argc == 2) {
        // * <config_path> presente
        if ((CONFIG_PATH = (char*)malloc(sizeof(char) * (strlen(argv[1]) + 1))) == NULL) {
            perror("Error: unable to allocate memory using malloc");
            return errno;
        }
        strncpy(CONFIG_PATH, argv[1], strlen(argv[1]) + 1);
        printf("Info: Using '%s' as configuration file.\n", CONFIG_PATH);
    } else {
        // Troppi parametri
        fprintf(stderr, "Error: too many parameters!\n");
        fprintf(stderr, "Usage: %s <config_path>\n", argv[0]);
        return errno;
    }

    // ! CONFIGURAZIONE
    // Parso il file di configurazione e carico i parametri in memoria
    printf("Info: loading configuration...\n");

    FILE* config_file = NULL;
    if ((config_file = fopen(CONFIG_PATH, "r")) == NULL) {
        perror("Error: failed to load configuration");
        return errno;
    }

    // Parso il file di configurazione, una riga per volta
    char* key;
    char* value;
    int value_length;
    char* strtok_status;
    long numeric_value = -1;
    char buffer[BUFFER_SIZE];
    // Leggo una riga dal file
    while (fgets(buffer, BUFFER_SIZE, config_file)) {
        // Salto righe vuote e righe commentate, che iniziano con #
        if (buffer[0] && (buffer[0] == '\n' || buffer[0] == '#')) continue;

        // Parso la coppia key -> value, con il delimitatore '='
        key = strtok_r(buffer, "=", &strtok_status);
        value = NULL;

        if (key) {
            // Parso la coppia key -> value, con il delimitatore '='
            value = strtok_r(NULL, "=", &strtok_status);
            value_length = strlen(value);
            // Aggiungo il terminatore alla fine della stringa,
            // considerando anche il caso in cui non sia presente
            // una riga vuota alla fine del file di configurazione
            if (value[value_length - 1] == '\n') value[value_length - 1] = '\0';

            if (strcmp(key, "THREADS_WORKER") == 0) {
                // * THREADS_WORKER
                if (is_number(value, &numeric_value) == 0 || numeric_value <= 0) {
                    fprintf(stderr, "Error: %s has an invalid value", key);
                    return EINVAL;
                }
                THREADS_WORKER = (size_t)numeric_value;

            } else if (strcmp(key, "STORAGE_MAX_CAPACITY") == 0) {
                // * STORAGE_MAX_CAPACITY
                if (is_number(value, &numeric_value) == 0 || numeric_value <= 0) {
                    fprintf(stderr, "Error: %s has an invalid value (%s)", key, value);
                    return EINVAL;
                }
                STORAGE_MAX_CAPACITY = (size_t)numeric_value;

            } else if (strcmp(key, "STORAGE_MAX_FILES") == 0 || numeric_value <= 0) {
                // * STORAGE_MAX_FILES
                if (is_number(value, &numeric_value) == 0) {
                    fprintf(stderr, "Error: %s has an invalid value", key);
                    return EINVAL;
                }
                STORAGE_MAX_FILES = (size_t)numeric_value;

            } else if (strcmp(key, "REPLACEMENT_POLICY") == 0) {
                // * REPLACEMENT_POLICY
                if (strcmp(value, "fifo") == 0)
                    REPLACEMENT_POLICY = FIFO;
                else if (strcmp(value, "lru") == 0)
                    REPLACEMENT_POLICY = LRU;
                else if (strcmp(value, "lfu") == 0)
                    REPLACEMENT_POLICY = LFU;
                else {
                    fprintf(stderr, "Error: %s has an invalid value", key);
                    return EINVAL;
                }

            } else if (strcmp(key, "SOCKET_PATH") == 0) {
                // * SOCKET_PATH
                if ((SOCKET_PATH = (char*)malloc(sizeof(char) * value_length + 1)) == NULL) {
                    perror("Error: unable to allocate memory using malloc for SOCKET_PATH");
                    return errno;
                }
                strncpy(SOCKET_PATH, value, value_length + 1);

            } else if (strcmp(key, "LOG_PATH") == 0) {
                // * LOG_PATH
                if ((LOG_PATH = (char*)malloc(sizeof(char) * value_length + 1)) == NULL) {
                    perror("Error: unable to allocate memory using malloc for LOG_PATH");
                    return errno;
                }
                strncpy(LOG_PATH, value, value_length + 1);
            }
        }
    }

    // Chiudo il file di configurazione
    if (fclose(config_file) != 0) {
        perror("Error: failed to close configuration file");
        return errno;
    }

    printf("Info: configuration loaded successfully!\n");

#ifdef DEBUG
    // Sommario di debug
    printf(
        "Debug: configuration summary:\n"
        "\t+ STORAGE_MAX_CAPACITY:\t%ld\n"
        "\t+ STORAGE_MAX_FILES:\t%ld\n"
        "\t+ REPLACEMENT_POLICY:\t%d\n"
        "\t+ THREADS_WORKER:\t%ld\n"
        "\t+ SOCKET_PATH:\t%s\n"
        "\t+ LOG_PATH:\t%s\n",
        STORAGE_MAX_CAPACITY, STORAGE_MAX_FILES, REPLACEMENT_POLICY,
        THREADS_WORKER, SOCKET_PATH, LOG_PATH);
#endif

    // ! LOG FILE
    // TODO: Sovrascrivere un log esistente o appenderci il contenuto alla fine?
    if ((log_file = fopen(LOG_PATH, "w")) == NULL) {
        perror("Error: failed to open log file");
        return errno;
    }

    // Log di avvio del
    log_event("INFO", " == Server bootstrap == ");

    // ! SEGNALI
    // Segnali da mascherare durante l'esecuzione dell'handler
    sigset_t sigset;
    // Inizializza la maschera; operazione richiesta per usare sigaddset
    sigemptyset(&sigset);
    // Aggiungo SIGINT
    sigaddset(&sigset, SIGINT);
    // Aggiungo SIGQUIT
    sigaddset(&sigset, SIGQUIT);
    // Aggiungo SIGHUP
    sigaddset(&sigset, SIGHUP);

    // Preparo la struttura da passare alla system call sigaction
    struct sigaction sig_action;
    // Inizializzo la struttura
    memset(&sig_action, 0, sizeof(sig_action));
    // Imposto l'handler su SIG_IGN, per ignorare l'arrivo dei segnali
    sig_action.sa_handler = SIG_IGN;
    // Imposto la maschera dei segnali creata precedentemente, per mascherare l'arrivo
    // di SIGINT, SIGQUIT e SIGHUP durante la gestione di altri segnali
    sig_action.sa_mask = sigset;
    // Ignoro il segnale SIGPIPE, per prevenire crash del server
    // in caso di disconnessione da parte dei clients sul socket
    if (sigaction(SIGPIPE, &sig_action, NULL) != 0) {
        perror("Error: failed to install signal handler for SIGPIPE with sigaction");
        return errno;
    }

    // Per la gestione di tutti gli altri segnali indicati in sigset, utilizzo un thread specializzato
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL) != 0) {  // ? Oppure SIG_SETMASK?
        perror("Error: failed to set signal mask for signals handler thread");
        return errno;
    }
    // Eseguo il therad specializzato nella gestione dei segnali
    pthread_t thread_signal_handler;
    if (pthread_create(&thread_signal_handler, NULL, &signals_handler, (void*)&sigset) != 0) {
        perror("Error: failed to launch signals handler thread");
        return errno;
    }

    printf("Info: started signals handler thread\n");

    // ! CONNESSIONE
    struct sockaddr_un socket_address;
    // Inizializzo la struttura
    memset(&socket_address, '0', sizeof(socket_address));
    socket_address.sun_family = AF_UNIX;
    // Imposto come path quello del socket preso dal file di configurazione
    // ? Come validare SOCKET_PATH?
    // TODO: controllo sull'esistenza del file?
    // TODO: cambiare in strncpy
    strcpy(socket_address.sun_path, SOCKET_PATH);

    // Creo il socket lato server, che accetterà nuove connessioni
    int server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error: failed to create server socket");
        return errno;
    }

    // Eseguo la bind sul socket appena creato
    if (bind(server_socket, (struct sockaddr*)&socket_address, sizeof(socket_address)) == -1) {
        perror("Error: failed to bind server socket");
        return errno;
    }

    // Quindi imposto la modalità passiva, ponendolo in ascolto
    if (listen(server_socket, CONCURRENT_CONNECTIONS) == -1) {
        perror("Error: failed to put server socket in listen mode");
        return errno;
    }

    // ! TASKS QUEUE
    Queue_t* task_queue = queue_init();
    if (!task_queue) {
        fprintf(stderr, "Error: failed to create a task queue");
        return EXIT_FAILURE;
    }

    // ! PIPE
    int pipe_workers[2];  // [0]: lettura, [1]: scrittura
    if (pipe(pipe_workers) == -1) {
        perror("Error: failed to create pipe");
        return errno;
    }

    // ! THREAD POOL
    // Creo la thread pool
    pthread_t* thread_pool;
    if ((thread_pool = (pthread_t*)malloc(sizeof(pthread_t) * THREADS_WORKER)) == NULL) {
        perror("Error: failed to allocate memory for thread pool");
        return errno;
    }

    // Parametri dei threads worker
    worker_args_t* worker_args = (worker_args_t*)malloc(sizeof(worker_args_t));
    worker_args->task_queue = task_queue;
    worker_args->pipe_output = pipe_workers[1];

    // Inizializzo e lancio i threads worker
    for (int i = 0; i < THREADS_WORKER; i++) {
        if (pthread_create(&thread_pool[i], NULL, &worker, (void*)worker_args) != 0) {
            fprintf(stderr, "Error: failed to start worker thread (%d)\n", i);
            return EXIT_FAILURE;
        }
        printf("Info: worker thread (%d) started\n", i);
    }

    printf("Info: thread pool initialized\n");

    // ! PREPARAZIONE SELECT
    // Insieme dei file descriptor attivi
    fd_set set;
    // Insieme dei file descriptor pronti in lettura
    fd_set ready_set;
    // Mantengo il massimo indice di descrittore attivo,
    // inizialmente pari allo stesso server socket
    int fd_num = server_socket;
    // Inizializzo la maschera
    FD_ZERO(&set);
    // Aggiungo il descrittore del socket server
    FD_SET(server_socket, &set);

    // Descrittore del socket client
    int client_socket;

    // Conteggio dei clients attivi, per gestire la terminazione "soft" del segnale SIGHUP
    int active_clients = 0;

    // Timeout di 100ms, dopo il quale uscire dalla select
    struct timeval timeout = {0, 100000};
    // Copia del timeout, che viene modificato dalla select ad ogni iterazione
    struct timeval current_timeout;

    // ! MAIN LOOP (DISPATCHER)
    // Rimango attivo finchè arriva un segnale di SIGINT|SIGQUIT (force_stop)
    // oppure arriva un segnale di SIGHUP e non ci sono più client connessi
    // * Termino quando: force_stop OR (stop AND active_clients == 0)
    while (!(force_stop || (stop && (active_clients == 0)))) {
        // Reimposto l'insieme dei descrittori su quello iniziale,
        // in quanto la select lo modifica ad ogni iterazione
        ready_set = set;
        // Faccio lo stesso con il timeout
        current_timeout = timeout;

        // ! SELECT
        if (select(fd_num + 1, &ready_set, NULL, NULL, &current_timeout) == -1) {
            // TODO: gestione errori
            continue;
        }

        // Itero sui selettori per processare tutti quelli pronti
        // Il massimo numero di descrittori è indicato da fd_num
        for (int fd = 0; fd < fd_num + 1; fd++) {
            // fd è pronto?
            if (FD_ISSET(fd, &ready_set)) {
                // Il server socket è pronto && non è arrivato SIGHUP
                if (fd == server_socket && !stop) {
                    // * Nuovo connessione in entrata
                    // Accetto la connessione in entrata e controllo eventuali errori
                    if ((client_socket = accept(server_socket, NULL, 0)) == -1) {
                        // TODO: cleanup prima di uscire
                        perror("Error: failed to accept an incoming connection");
                        return errno;
                    }
                    // Aggiungo il nuovo descrittore nella maschera di partenza
                    FD_SET(client_socket, &set);
                    // Aggiorno il contatore del massimo indice
                    if (client_socket > fd_num) fd_num = client_socket;
                    // Aggiorno il contatore dei clients attivi
                    active_clients++;
                    log_event("INFO", "Accepted incoming connection");
                    printf("Info: accepted incoming connection on %d\n", client_socket);
                } else {
                    // * Nuovo task da parte di un client connesso
                    // Inserisco il descrittore nella coda dei tasks
                    queue_push(task_queue, (void*)fd);
                    // Rimuovo il descrittore dal ready set
                    FD_CLR(fd, &set);
                    //if(fd == fd_num) fd_num--;
                    log_event("INFO", "Nuovo task da x aggiunto in coda");
                    printf("Aggiunto %d nella queue\n", fd);
                }
            }
        }
    }

    // ! EXIT
    // Mi assicuro che tutti i threads spawnati siano terminati
    //pthread_join(thread_signal_handler, NULL);
    free(thread_pool);

    // Libero la memoria prima di uscire
    free(CONFIG_PATH);
    free(SOCKET_PATH);
    free(LOG_PATH);

    // Elimino il socket file
    unlink(SOCKET_PATH);

    // Chiudo i socket
    close(server_socket);
    close(client_socket);

    // Log di chiusura
    log_event("INFO", " == Server shutdown == ");

    // Chiudo il file di log
    if (fclose(log_file) != 0) {
        perror("Error: failed to close log file");
        return errno;
    }

    return EXIT_SUCCESS;
}

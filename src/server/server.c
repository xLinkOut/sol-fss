// @author Luca Cirillo (545480)

#include <config.h>
#include <constants.h>
#include <errno.h>
#include <pthread.h>
#include <queue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <storage.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <utils.h>

// TODO: liberare la memoria prima di uscire in caso di errore
// TODO: routine di cleanup per la chiusura su errore del server
// TODO: ResponseCode appropriati ad ogni API call

// File di log
FILE* log_file = NULL;
// Mutex file di log
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Logga su file un particolare evento, con un certo livello di importanza
// 'level' si suppone essere uno tra INFO, DEBUG, WARN, ERROR
void log_event(const char* level, const char* message) {
    time_t timer = time(NULL);
    struct tm* tm_info = localtime(&timer);
    char date_time[20];
    strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", tm_info);
    if(pthread_mutex_lock(&log_mutex) != 0){
        perror("Error: failed to lock log file");
        // TODO: exit ?
    }
    fprintf(log_file, "%s | %s\t| %s\n", date_time, level, message);
    if(pthread_mutex_unlock(&log_mutex) != 0){
        perror("Error: failed to unlock log file");
        // TODO: exit ?
    }
}

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

// Struttura dati per passare più argomenti ai threads worker
typedef struct worker_args {
    storage_t* storage;   // Riferimento allo storage in uso
    Queue_t* task_queue;  // Coda dei task che arrivano e vengono smistati dal dispatcher
    int pipe_output;      // Pipe di comunicazione worker(s) <-> dispatcher
} worker_args_t;

static void* worker(void* args) {
    // Argomenti passati al thread worker
    worker_args_t* worker_args = (worker_args_t*)args;

    int fd_ready;                   // fd del client servito al momento
    int api_exit_code = 0;          // Codice di uscita di una API call
    char* strtok_status;            // Stato per le chiamate alla syscall strtok_r
    char request[MESSAGE_LENGTH];   // Messaggio richiesta del client
    char response[MESSAGE_LENGTH];  // Messaggio risposta del server
    char pathname[MESSAGE_LENGTH];  // Quasi ogni API call prevede un pathname
    
    // ! MAIN WORKER LOOP
    // TODO: condizione while, eseguo finché !force_stop, ma nel caso di stop? Come faccio a eseguire finché i client non finiscono?
    while (1) {
        // Recupero un file descriptor pronto dalla queue
        fd_ready = queue_pop(worker_args->task_queue);

        // Controllo che la pop non abbia ritornato un codice di errore (-2)
        if (fd_ready == -2) {
            perror("Error: failed to pop an element from task queue");
            break;
        }

        // Controllo che non sia un segnale di terminazine dal dispatcher (-1)
        if (fd_ready == -1) break;

        // Controllo che in coda non sia finito un qualsiasi altro valore negativo
        if (fd_ready < 0) {
            fprintf(stderr, "Error: negative file descriptor in task queue\n");
            return NULL;
        }

        // A questo punto sono sicuro di avere un fd valido
        //printf("Worker on %d\n", fd_ready);

        // Pulisco tracce di eventuali richieste precedenti
        memset(request, 0, MESSAGE_LENGTH);
        // Leggo il contenuto della richiesta del client
        if (readn((long)fd_ready, (void*)request, MESSAGE_LENGTH) == -1) {
            fprintf(stderr, "Error: read on client failed\n");
            return NULL;
        }

        // * Faccio il parsing della richiesta
        // Il formato atteso è: <int:codice_richiesta> <string:parametri>[,<string:parametri>]
        //printf("Richiesta: %s\n", request);
        // Uso lo spazio come delimitatore
        char* token = strtok_r(request, " ", &strtok_status);
        // Se scopro essere una stringa vuota, non vado oltre
        if (!token) continue;
        // Recupero dalla richiesta il comando che deve essere eseguito
        int command;
        if (sscanf(token, "%d", &command) != 1) {
            fprintf(stderr, "Error: invalid command in request: %s\n", request);
            continue;
        }

        // * Eseguo le operazioni relative al comando ricevuto
        switch (command) {
            case OPEN:  // * openFile: OPEN <str:pathname> <int:flags>
                // Parso il pathname dalla richiesta
                token = strtok_r(NULL, " ", &strtok_status);
                memset(pathname, 0, MESSAGE_LENGTH);
                if (!token || sscanf(token, "%s", pathname) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }
                // Parso i flags dalla richiesta
                int flags = 0;
                token = strtok_r(NULL, " ", &strtok_status);
                if (!token || sscanf(token, "%d", &flags) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }

                printf("OPEN: %s %d\n", pathname, flags);

                // Eseguo la API call
                api_exit_code = storage_open_file(worker_args->storage, pathname, flags, fd_ready);
                // Preparo il buffer per la risposta
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", api_exit_code);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }

                log_event("INFO", "Storage open file with code ");
                break;

            case READ: // ! readFile: READ <str:pathname>
                // Parso il pathname dalla richiesta
                token = strtok_r(NULL, " ", &strtok_status);
                memset(pathname, 0, MESSAGE_LENGTH);
                if (!token || sscanf(token, "%s", pathname) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }

                printf("READ: %s\n", pathname);
                
                // Eseguo la API call
                void* f_contents = NULL;
                size_t size = 0;
                api_exit_code = storage_read_file(worker_args->storage, pathname, &f_contents, &size, fd_ready);
                
                int code = 1;
                if(api_exit_code == -1){
                    if(errno == ENOENT) code = 0;
                    else if (errno == EPERM) code = -1;
                }
                //printf("readfile exit code %d\n", api_exit_code);

                // Invio al client il codice di ritorno e, eventualmente, la dimensione del file
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d %zd", code, code == 1 ? size : 0);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }
                
                if(api_exit_code == -1) break;
                if (writen((long)fd_ready, f_contents, size) == -1) {
                    perror("Error: writen failed");
                    break;
                }

                // Libero la memoria occupata per leggere il file
                free(f_contents);

                // TODO: fix this, usare direttamente code come codice risposta
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", 0);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }

                log_event("INFO", "Storage read file with code ");
                break;
                
            case WRITE: // ! writeFile: WRITE <str:pathname> <int:file_size>
                // Parso il pathname del file
                token = strtok_r(NULL, " ", &strtok_status);
                memset(pathname, 0, MESSAGE_LENGTH);
                if (!token || sscanf(token, "%s", pathname) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }

                // Parso la dimensione del file
                size_t file_size = 0;
                token = strtok_r(NULL, " ", &strtok_status);
                if (!token || sscanf(token, "%zd", &file_size) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }

                printf("WRITE: %s %zd\n", pathname, file_size);

                // Conosco la dimensione del file, posso allocare lo spazio necessario
                void* contents = malloc(file_size);
                memset(contents, 0, file_size);
                // Ricevo dal client il contenuto del file
                if(readn((long)fd_ready, (void*)contents, file_size) == -1){
                    perror("Error: readn failed");
                    free(contents);
                    break;
                }
                
                // Scrivo il contenuto del file all'intero dello storage
                int victims_no = 0;
                storage_file_t** victims = NULL;
                api_exit_code = storage_write_file(worker_args->storage, pathname, contents, file_size, &victims_no, &victims, fd_ready);

                // Invio al client eventuali file espulsi
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", victims_no);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failedaaa");
                    break;
                }

                if(victims_no > 0){
                    // Invio al client i file espulsi
                    for(int i=0;i<victims_no;i++){
                        printf("Sending n.%d: %s %zd\n", i, victims[i]->name, victims[i]->size);
                        // Invio al client il nome e la dimensione del file
                        memset(response, 0, MESSAGE_LENGTH);
                        snprintf(response, MESSAGE_LENGTH, "%s %d", victims[i]->name, victims[i]->size);
                        if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                            perror("Error: writen failed");
                            break;
                        }

                        // Invio al client il contenuto del file
                        if (writen((long)fd_ready, (void*)victims[i]->contents, victims[i]->size) == -1) {
                            perror("Error: writen failed");
                            break;
                        }
                    }
                }
                
                // Preparo il buffer per la risposta
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", api_exit_code);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }

                log_event("INFO", "Storage write file with code ");
                break;

            case APPEND: // ! appendToFile: APPEND <str:pathname> <int:size>
                // Parso il pathname del file
                token = strtok_r(NULL, " ", &strtok_status);
                memset(pathname, 0, MESSAGE_LENGTH);
                if (!token || sscanf(token, "%s", pathname) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }

                // Parso la dimensione del file
                file_size = 0;
                token = strtok_r(NULL, " ", &strtok_status);
                if (!token || sscanf(token, "%zd", &file_size) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }

                printf("APPEND: %s %zd\n", pathname, file_size);

                // Conosco la dimensione del file, posso allocare lo spazio necessario
                contents = malloc(file_size);
                memset(contents, 0, file_size);
                // Ricevo dal client il contenuto del file
                if(readn((long)fd_ready, (void*)contents, file_size) == -1){
                    perror("Error: readn failed");
                    free(contents);
                    break;
                }


                // Scrivo il contenuto del file all'intero dello storage
                victims_no = 0;
                victims = NULL;
                api_exit_code = storage_append_to_file(worker_args->storage, pathname, contents, file_size, &victims_no, &victims, fd_ready);

                // Invio al client eventuali file espulsi
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", victims_no);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failedaaa");
                    break;
                }

                if(victims_no > 0){
                    // Invio al client i file espulsi
                    for(int i=0;i<victims_no;i++){
                        printf("Sending n.%d: %s %zd\n", i, victims[i]->name, victims[i]->size);
                        // Invio al client il nome e la dimensione del file
                        memset(response, 0, MESSAGE_LENGTH);
                        snprintf(response, MESSAGE_LENGTH, "%s %d", victims[i]->name, victims[i]->size);
                        if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                            perror("Error: writen failed");
                            break;
                        }

                        // Invio al client il contenuto del file
                        if (writen((long)fd_ready, (void*)victims[i]->contents, victims[i]->size) == -1) {
                            perror("Error: writen failed");
                            break;
                        }
                    }
                }

                // Preparo il buffer per la risposta
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", api_exit_code);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }
                
                log_event("INFO", "Storage append file with code ");
                break;

            case LOCK: // ! lockFile: LOCK <str:pathname>
                // Parso il pathname del file
                token = strtok_r(NULL, " ", &strtok_status);
                memset(pathname, 0, MESSAGE_LENGTH);
                if (!token || sscanf(token, "%s", pathname) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }
                
                printf("LOCK %s\n", pathname);

                // Eseguo la API call
                api_exit_code = storage_lock_file(worker_args->storage, pathname, fd_ready);

                // Preparo il buffer per la risposta
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", api_exit_code);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }

                log_event("INFO", "Storage lock file with code ");
                break;

            case UNLOCK: // ! unlockFile: UNLOCK <str:pathname>
                // Parso il pathname del file
                token = strtok_r(NULL, " ", &strtok_status);
                memset(pathname, 0, MESSAGE_LENGTH);
                if (!token || sscanf(token, "%s", pathname) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }
                
                printf("UNLOCK %s\n", pathname);

                // Eseguo la API call
                api_exit_code = storage_unlock_file(worker_args->storage, pathname, fd_ready);

                // Preparo il buffer per la risposta
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", api_exit_code);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }

                log_event("INFO", "Storage unlock file with code ");
                break;

            case CLOSE:  // * closeFile: CLOSE <str:pathname>
                // Parso il pathname dalla richiesta
                token = strtok_r(NULL, " ", &strtok_status);
                memset(pathname, 0, MESSAGE_LENGTH);
                if (!token || sscanf(token, "%s", pathname) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }

                printf("CLOSE: %s\n", pathname);

                // Eseguo la API call
                api_exit_code = storage_close_file(worker_args->storage, pathname, fd_ready);
                // Preparo il buffer per la risposta
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", api_exit_code);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }

                log_event("INFO", "Storage close file with code ");
                break;

            case REMOVE: // ! removeFile: REMOVE <str:pathname>
                // Parso il pathname dalla richiesta
                token = strtok_r(NULL, " ", &strtok_status);
                memset(pathname, 0, MESSAGE_LENGTH);
                if (!token || sscanf(token, "%s", pathname) != 1) {
                    fprintf(stderr, "Error: bad request\n");
                    break;
                }

                printf("REMOVE: %s\n", pathname);

                // Eseguo la API call
                api_exit_code = storage_remove_file(worker_args->storage, pathname, fd_ready);
                // Preparo il buffer per la risposta
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", api_exit_code);
                if (writen((long)fd_ready, (void*)response, MESSAGE_LENGTH) == -1) {
                    perror("Error: writen failed");
                    break;
                }

                log_event("INFO", "Storage remove file with code ");
                break;
                
            case DISCONNECT:  // * closeConnection
                // Un client ha richiesto la chiusura della connessione
                // Lo comunico al thread dispatcher tramite la pipe
                memset(response, 0, MESSAGE_LENGTH);
                snprintf(response, MESSAGE_LENGTH, "%d", CLIENT_LEFT);
                if (writen((long)worker_args->pipe_output, (void*)response, PIPE_LEN) == -1) {
                    perror("Error: writen failed");
                    break;
                }
                log_event("INFO", "Client left");
                break;

            default:
                fprintf(stderr, "Error: unknown command %d\n", command);
                break;
        }

        // Informo il dispatcher che il worker ha terminato con la richiesta di fd_ready
        memset(response, 0, MESSAGE_LENGTH);
        snprintf(response, MESSAGE_LENGTH, "%d", fd_ready);
        if (writen((long)worker_args->pipe_output, (void*)response, PIPE_LEN) == -1) {
            perror("Error: writen failed");
            continue;
        }
    }

    return NULL;
}

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
    // Controllo se il file di configurazione esiste
    // e se dispongo dei diritti necessari per poterlo leggere
    if (access(CONFIG_PATH, R_OK) == -1) {
        // File non esistente oppure permessi non sufficienti
        fprintf(stderr, "Error: %s does not exist or permissions are insufficient to read the file\n", CONFIG_PATH);
        return EINVAL;
    }

    // Parso il file di configurazione e carico i parametri in memoria
    printf("Info: loading configuration...\n");

    FILE* config_file = NULL;
    if ((config_file = fopen(CONFIG_PATH, "r")) == NULL) {
        perror("Error: failed to load configuration file");
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
            value_length = strlen(value);  // Include il terminatore nel conteggio
            // Aggiungo il terminatore alla fine della stringa,
            // considerando anche il caso in cui non sia presente
            // una riga vuota alla fine del file di configurazione
            if (value[value_length - 1] == '\n') value[value_length - 1] = '\0';

            if (strcmp(key, "THREADS_WORKER") == 0) {
                // * THREADS_WORKER
                if (is_number(value, &numeric_value) == 0 || numeric_value <= 0) {
                    fprintf(stderr, "Error: %s has an invalid value\n", key);
                    return EINVAL;
                }
                THREADS_WORKER = (size_t)numeric_value;

            } else if (strcmp(key, "STORAGE_MAX_CAPACITY") == 0) {
                // * STORAGE_MAX_CAPACITY
                if (is_number(value, &numeric_value) == 0 || numeric_value <= 0) {
                    fprintf(stderr, "Error: %s has an invalid value (%s)\n", key, value);
                    return EINVAL;
                }
                STORAGE_MAX_CAPACITY = (size_t)numeric_value;

            } else if (strcmp(key, "STORAGE_MAX_FILES") == 0 || numeric_value <= 0) {
                // * STORAGE_MAX_FILES
                if (is_number(value, &numeric_value) == 0) {
                    fprintf(stderr, "Error: %s has an invalid value\n", key);
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
                    fprintf(stderr, "Error: %s has an invalid value\n", key);
                    return EINVAL;
                }

            } else if (strcmp(key, "SOCKET_PATH") == 0) {
                // * SOCKET_PATH
                if ((SOCKET_PATH = (char*)malloc(sizeof(char) * value_length)) == NULL) {
                    perror("Error: unable to allocate memory using malloc for SOCKET_PATH");
                    return errno;
                }
                strncpy(SOCKET_PATH, value, value_length);

            } else if (strcmp(key, "LOG_PATH") == 0) {
                // * LOG_PATH
                if ((LOG_PATH = (char*)malloc(sizeof(char) * value_length)) == NULL) {
                    perror("Error: unable to allocate memory using malloc for LOG_PATH");
                    return errno;
                }
                strncpy(LOG_PATH, value, value_length);
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
    // * Si è scelto di non sovrascrivere un eventuale file di log già esistente
    // Ogni sessione del server è racchiusa tra due righe di bootstrap/shutdown
    if ((log_file = fopen(LOG_PATH, "w")) == NULL) {
        perror("Error: failed to open log file");
        return errno;
    }

    // Avvio del server
    log_event("INFO", " == Server bootstrap == ");

    // ! STORAGE
    storage_t* storage = storage_create(STORAGE_MAX_FILES, STORAGE_MAX_CAPACITY, REPLACEMENT_POLICY);
    if (!storage) {
        perror("Error: storage creation failed");
        return errno;
    }

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
    // in caso di scrittura su pipe che sono state chiuse
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
    strcpy(socket_address.sun_path, SOCKET_PATH);  // TODO: strncpy

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

    // Buffer per lavorare con i dati trasmessi sulla pipe dai workers
    char pipe_buffer[PIPE_LEN];
    int pipe_message;

    // ! THREAD POOL
    // Creo la thread pool
    pthread_t* thread_pool;
    if ((thread_pool = (pthread_t*)malloc(sizeof(pthread_t) * THREADS_WORKER)) == NULL) {
        perror("Error: failed to allocate memory for thread pool");
        return errno;
    }

    // Parametri dei threads worker
    worker_args_t* worker_args = (worker_args_t*)malloc(sizeof(worker_args_t));
    worker_args->storage = storage;
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
    // Aggiungo anche la pipe tra dispatcher e workers
    FD_SET(pipe_workers[0], &set);

    // Descrittore del socket client
    int client_socket;

    // Conteggio dei clients attivi, per gestire la terminazione "soft" del segnale SIGHUP
    int active_clients = 0;

    // Timeout di 100ms, dopo il quale uscire dalla select
    struct timeval timeout = {0, 100000};
    // Copia del timeout, che viene modificato dalla select ad ogni iterazione
    struct timeval current_timeout;

    // ! MAIN LOOP (DISPATCHER)
    // Rimango attivo finché arriva un segnale di SIGINT|SIGQUIT (force_stop)
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
                    //printf("Active clients: %d\n", active_clients);
                    log_event("INFO", "Accepted incoming connection");
                    //printf("Info: accepted incoming connection on %d\n", client_socket);

                } else if (fd == pipe_workers[0]) {
                    // * Nuova comunicazione da un thread worker
                    memset(pipe_buffer, 0, PIPE_LEN);
                    if (readn((long)fd, (void*)pipe_buffer, PIPE_LEN) == -1) {
                        perror("Error: readn failed on pipe");
                        continue;
                    }
                    if (sscanf(pipe_buffer, "%d", &pipe_message) != 1) {
                        perror("Error: sscanf failed");
                        continue;
                    }
                    if (pipe_message == CLIENT_LEFT) {
                        active_clients--;
                    } else {
                        FD_SET(pipe_message, &set);
                        if (pipe_message > fd_num) fd_num = pipe_message;
                    }
                    //printf("Active clients: %d\n", active_clients);

                } else {
                    // * Nuovo task da parte di un client connesso
                    // Inserisco il descrittore nella coda dei tasks
                    queue_push(task_queue, fd);
                    // Rimuovo il descrittore dal ready set
                    FD_CLR(fd, &set);
                    //if(fd == fd_num) fd_num--;
                    log_event("INFO", "Nuovo task da x aggiunto in coda");
                    //printf("Aggiunto %d nella queue\n", fd);
                }
            }
        }
    }

    // ! EXIT
    // Stampo un sommario delle operazioni effettuate
    printf(
        "Some storage statistics:\n"
        "+ Server start @ %ld, shutdown @ %ld\n"
        "+ Max files stored: %zd\n"
        "+ Max space used: %zd\n"
        "+ Replacement algorithm executed %zd times\n\n"
        "+ At shutdown, these files are inside the storage:\n",
        storage->start_timestamp, time(NULL),
        storage->max_files_reached, storage->max_capacity_reached,
        storage->rp_algorithm_counter
    );

    // Visualizzo i file presenti nello storage al momento dell'arresto
    storage_print(storage);
    
    // Mi assicuro che tutti i threads spawnati siano terminati
    // Prima il signal handler thread, che è il primo a terminare
    pthread_join(thread_signal_handler, NULL);
    // Poi inserisco un valore di "chiusura" per tutti i thread workers
    for (int i = 0; i < THREADS_WORKER; i++) queue_push(task_queue, -1);
    // Quindi aspetto la loro imminente chiusura
    for (int i = 0; i < THREADS_WORKER; i++) pthread_join(thread_pool[i], NULL);
    // E libero la memoria per la pool
    free(thread_pool);

    // Chiudo i socket
    close(server_socket);
    close(client_socket);

    // Chiudo la pipe dispatcher <-> workers
    close(pipe_workers[0]);
    close(pipe_workers[1]);

    // Cancello lo storage
    storage_destroy(storage);

    // Elimino il socket file
    unlink(SOCKET_PATH);
    
    // Log di chiusura
    log_event("INFO", " == Server shutdown == ");

    // Chiudo il file di log
    if (fclose(log_file) != 0) {
        perror("Error: failed to close log file");
        return errno;
    }

    // Libero la memoria prima di uscire
    free(CONFIG_PATH);
    free(SOCKET_PATH);
    free(LOG_PATH);

    return EXIT_SUCCESS;
}

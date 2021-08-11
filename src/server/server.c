// @author Luca Cirillo (545480)

#include <errno.h>
#include <pthread.h>
#include <server_config.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

// TODO: spostare in un file di utilities
#define BUFFER_SIZE 512

// TODO: routine di cleanup per la chiusura su errore del server

// TODO: spostare nel file di gestione dei segnali
static void* signals_handler(void* sigset) {
    int error;
    int signal;
    // Aspetto l'arrivo di un segnale tra SIGINT, SIGQUIT e SIGHUP
    if ((error = sigwait((sigset_t*)sigset, &signal)) != 0) {
        fprintf(stderr, "Error: something wrong append while waiting for signals!\n");
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
            // TODO: gestione
            printf("SIGINT | SIGQUIT\n");
            break;

        /*
            ! SIGHUP: il server completa le richieste, quindi termina:
                non accetta nuove richieste da parte di nuovi client
                ma vengono comunque servite tutte le richieste dei client connessi 
                al momento della ricezione del segnale.
                Il server terminerà solo quando tutti i client connessi  chiuderanno la connessione.
        */
        case SIGHUP:
            // TODO: gestione
            printf("SIGHUP\n");
            break;

        default:
            break;
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

int main(int argc, char* argv[]) {
    // ! Parametri in ingresso:
    //  * 1. <config_path>: path al file di configurazione 'config.txt'
    //    Se non viene specificato, si assume che il file di configurazione
    //    sia presente nella directory corrente

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

            // Aggiungo il terminatore alla fine della stringa
            value[strlen(value) - 1] = '\0';

            // * THREADS_WORKER
            if (strcmp(key, "THREADS_WORKER") == 0) {
                if (is_number(value, &numeric_value) == 0 || numeric_value <= 0) {
                    fprintf(stderr, "Error: %s has an invalid value", key);
                    return EINVAL;
                }
                THREADS_WORKER = (size_t)numeric_value;

            // * STORAGE_MAX_CAPACITY
            } else if (strcmp(key, "STORAGE_MAX_CAPACITY") == 0) {
                if (is_number(value, &numeric_value) == 0 || numeric_value < 0) {
                    fprintf(stderr, "Error: %s has an invalid value (%s)", key, value);
                    return EINVAL;
                }
                STORAGE_MAX_CAPACITY = (size_t)numeric_value;

            // * STORAGE_MAX_FILES
            } else if (strcmp(key, "STORAGE_MAX_FILES") == 0 || numeric_value < 0) {
                if (is_number(value, &numeric_value) == 0) {
                    fprintf(stderr, "Error: %s has an invalid value", key);
                    return EINVAL;
                }
                STORAGE_MAX_FILES = (size_t)numeric_value;

            // * REPLACEMENT_POLICY
            } else if (strcmp(key, "REPLACEMENT_POLICY") == 0) {
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

            // * SOCKET_PATH
            } else if (strcmp(key, "SOCKET_PATH") == 0) {
                if ((SOCKET_PATH = (char*)malloc(sizeof(char) * strlen(value) + 1)) == NULL) {
                    perror("Error: unable to allocate memory using malloc for SOCKET_PATH");
                    return errno;
                }
                strncpy(SOCKET_PATH, value, strlen(value) + 1);

            // * LOG_PATH
            } else if (strcmp(key, "LOG_PATH") == 0) {
                if ((LOG_PATH = (char*)malloc(sizeof(char) * strlen(value) + 1)) == NULL) {
                    perror("Error: unable to allocate memory using malloc for LOG_PATH");
                    return errno;
                }
                strncpy(LOG_PATH, value, strlen(value) + 1);
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
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL) != 0) {
        perror("Error: failed to set signal mask for signals handler thread");
        return errno;
    }
    // Eseguo il therad specializzato nella gestione dei segnali
    pthread_t thread_signal_handler;
    if (pthread_create(&thread_signal_handler, NULL, &signals_handler, (void*)&sigset) != 0) {
        perror("Error: failed to launch signals handler thread");
        return errno;
    }

    // ! EXIT
    // Mi assicuro che tutti i threads spawnati siano terminati
    //pthread_join(thread_signal_handler, NULL);

    // Libero la memoria prima di uscire
    free(CONFIG_PATH);
    free(SOCKET_PATH);
    free(LOG_PATH);

    return EXIT_SUCCESS;
}

// @author Luca Cirillo (545480)

#include <API.h>
#include <constants.h>
#include <errno.h>
#include <getopt.h>
#include <request_queue.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utils.h>

// TODO: Liberare eventuale memoria prima di uscire, anche nel caso di -h

void print_help() {
    printf(
        "-f socketname\t \n"
        "-w dirname[,n=0]\t \n"
        "-W file1[,file2]\t \n"
        "-D dirname\t \n"
        "-r file1[,file2]\t \n"
        "-R [n=0]\t \n"
        "-d dirname\t \n"
        "-t time\t \n"
        "-l file1[,file2]\t \n"
        "-p\t Enable verbose mode\n"
        "-h\t Print this message and exit\n");
}

// ! MAIN
int main(int argc, char* argv[]) {
    // Se non viene specificato alcun parametro, stampo l'usage ed esco
    if (argc == 1) {
        printf("Usage: %s [OPTIONS]...\n\nSee '%s -h' for more information\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    // ! PARSING
    // Altrimenti, parso i parametri passati da riga di comando

    // Nella optstring, il primo ':' serve a distinguere il caso di carattere sconosciuto, che ritorna '?',
    // dal caso di mancato argomento, che invece ritorna proprio ':'.
    // Il parametro -R, che prevede un argomento opzionale, verrà gestito nel caso ':'

    int EXIT_CODE = EXIT_SUCCESS;           // Codice di uscita in caso di errore
    char* SOCKET_PATH = NULL;               // Percorso al socket file del server
    Request_t* request = NULL;              // Descrizione di una singola richiesta
    Queue_t* request_queue = queue_init();  // Coda delle richiesta
    if (!request_queue) {
        fprintf(stderr, "Error: failed to create request queue\n");
        return errno;
    }

    int option;  // Carattere del parametro appena letto da getopt
    while ((option = getopt(argc, argv, ":hpf:w:W:D:r:R:d:t:l:u:c:")) != -1) {
        switch (option) {
            // * Path del socket
            case 'f':
                // Controllo che non sia già stato specificato
                if (SOCKET_PATH) {
                    fprintf(stderr, "Error: -f parameter can only be specified once\n");
                    EXIT_CODE = EINVAL;
                    goto free_and_exit;
                }

                // Salvo il path del socket
                if (!(SOCKET_PATH = malloc(strlen(optarg) + 1))) {
                    perror("Error: failed to allocate memory for SOCKET_PATH");
                    EXIT_CODE = errno;
                    goto free_and_exit;
                }
                strcpy(SOCKET_PATH, optarg);

                break;

            // * Comandi da eseguire
            // Scrittura, lettura, lock/unlock, cancellazione
            case 'w':
            case 'W':
            case 'r':
            case 'R':
            case 'l':
            case 'u':
            case 'c':
                // Controllo se è presente una richiesta in attesa di essere messa in coda
                if (request) queue_push(request_queue, request);
                // Creo una nuova richiesta
                if (!(request = queue_new_request())) {
                    fprintf(stderr, "Error: failed to allocate memory for a new request");
                    EXIT_CODE = errno;
                    goto free_and_exit;
                }
                // Salvo il comando
                request->command = option;
                // Salvo la lista di argomenti
                if (!(request->arguments = malloc(strlen(optarg) + 1))) {
                    perror("Error: failed to allocate memory for request arguments");
                    EXIT_CODE = errno;
                    goto free_and_exit;
                }
                strcpy(request->arguments, optarg);
                break;

            // * Eventuali argomenti
            case 'D':  // Relativa ai comandi 'w' e 'W'
                // Controllo che non sia stato specificato congiuntamente ad una richiesta
                if (!request) {
                    fprintf(stderr, "Error: no request to set this parameter on\n");
                    EXIT_CODE = EINVAL;
                    goto free_and_exit;
                }
                // Controllo che non sia stato specificato con comandi che non siano 'w' oppure 'W'
                if (request->command != 'w' && request->command != 'W') {
                    fprintf(stderr, "Error: -D must be used in conjunction with -w or -W\n");
                    EXIT_CODE = EINVAL;
                    goto free_and_exit;
                }
                // Quindi, alloco lo spazio per salvare l'argomento
                if (!(request->dirname = malloc(strlen(optarg) + 1))) {
                    perror("Error: failed to allocate memory for request dirname");
                    EXIT_CODE = errno;
                    goto free_and_exit;
                }
                strcpy(request->dirname, optarg);
                break;

            case 'd':  // Relativa ai comandi 'r' e 'R'
                // Controllo che non sia stato specificato congiuntamente ad una richiesta
                if (!request) {
                    fprintf(stderr, "Error: no request to set this parameter on\n");
                    EXIT_CODE = EINVAL;
                    goto free_and_exit;
                }
                // Controllo che non sia stato specificato con comandi che non siano 'r' oppure 'R'
                if (request->command != 'r' && request->command != 'R') {
                    fprintf(stderr, "Error: -d must be used in conjunction with -r or -R\n");
                    EXIT_CODE = EINVAL;
                    goto free_and_exit;
                }
                // Quindi, alloco lo spazio per salvare l'argomento
                if (!(request->dirname = malloc(strlen(optarg) + 1))) {
                    perror("Error: failed to allocate memory for arguments");
                    EXIT_CODE = errno;
                    goto free_and_exit;
                }
                strcpy(request->dirname, optarg);
                break;

            // * Time
            case 't':
                // Controllo che non sia stato specificato congiuntamente ad una richiesta
                if (!request) {
                    fprintf(stderr, "Error: no request to set this parameter on\n");
                    EXIT_CODE = EINVAL;
                    goto free_and_exit;
                }
                // Controllo che sia effettivamente un numero valido
                // TODO: controllo che sia un numero >= 0
                if (!is_number(optarg, &request->time)) {
                    fprintf(stderr, "Error: time is invalid\n");
                    EXIT_CODE = EINVAL;
                    goto free_and_exit;
                }
                break;

            // * Verbose
            case 'p':
                // Controllo che non sia già stato specificato
                if (VERBOSE) {
                    fprintf(stderr, "Error: -p parameter can only be specified once\n");
                    EXIT_CODE = EINVAL;
                    goto free_and_exit;
                }
                VERBOSE = true;
                break;

            // * Messaggio di help
            case 'h':
                // Stampo l'help ed esco
                print_help();
                EXIT_CODE = EXIT_SUCCESS;
                goto free_and_exit;

            // * Argomento non specificato
            case ':':
                switch (optopt) {  // Controllo che non sia R, che ha un argomento facoltativo
                    case 'R':      // E' proprio R, devo impostare n = 0
                        // Eseguo le operazioni relative al comando R, ma imposto come argomento di default 'n=0'
                        // Controllo se è presente una richiesta in attesa di essere messa in coda
                        if (request) queue_push(request_queue, request);
                        // Creo una nuova richiesta
                        if (!(request = queue_new_request())) {
                            fprintf(stderr, "Error: failed to allocate memory for a new request");
                            EXIT_CODE = errno;
                            goto free_and_exit;
                        }
                        // Salvo il comando
                        request->command = optopt;  // Non posso usare option perché è uguale a ':', uso optopt
                        // Salvo la lista di argomenti
                        char* default_n = "n=0";
                        if (!(request->arguments = malloc(strlen(default_n) + 1))) {
                            perror("Error: failed to allocate memory for request arguments");
                            EXIT_CODE = errno;
                            goto free_and_exit;
                        }
                        strcpy(request->arguments, default_n);
                        break;
                    default:
                        fprintf(stderr, "Parameter -%c takes an argument\n", optopt);
                        EXIT_CODE = EINVAL;
                        goto free_and_exit;
                }
                break;

            // * Parametro sconosciuto
            case '?':
                printf("Unknown parameter '%c'\n", optopt);
                EXIT_CODE = EINVAL;
                goto free_and_exit;
        }
    }

    // Metto in coda l'eventuale ultima (o unica) richiesta
    if (request) queue_push(request_queue, request);

    queue_print(request_queue);  // ! Debug

    // * Apertura della connessione con il server
    // Tempo di attesa tra un tentativo di connessione e l'altro (1 secondo)
    int msec = 1000;
    // Effettua nuovi tentativi di connessione fino al tempo assoluto abstime.tv_sec (10 secondi)
    struct timespec abstime = {.tv_sec = time(NULL) + 10, .tv_nsec = 0};
    // Instauro una connessione con il server
    if (openConnection(SOCKET_PATH, msec, abstime) == -1) {
        perror("Error: failed to initiate socket connection");
        EXIT_CODE = errno;
        goto free_and_exit;
    }
    if (VERBOSE) printf("Connection established correctly to '%s'\n", SOCKET_PATH);

    // * Esecuzione delle richieste, in ordine FIFO
    char* filename = NULL;
    char* strtok_status = NULL;
    // readFile
    void* buffer = NULL;
    size_t size = 0;
    // readNFiles
    long N = 0;
    char* token = NULL;

    while ((request = queue_pop(request_queue))) {
        printf("%c %s (%s, %lu)\n", request->command, request->arguments, request->dirname, request->time);
        switch (request->command) {
            case 'W':  // Invio al server un(a lista di) file
                // Possono essere specificati più file separati da virgola
                filename = strtok_r(request->arguments, ",", &strtok_status);
                while (filename) {
                    if (openFile(filename, O_CREATE | O_LOCK) == -1) {
                        perror("Error: can't open the file, skip it");
                        // Non avendo aperto correttamente il file, evito di proseguire
                        filename = strtok_r(NULL, ",", &strtok_status);
                        continue;
                    }

                    if (writeFile(filename, request->dirname) == -1) {
                        perror("Error: cannot write the contents of the file");
                    }

                    if (closeFile(filename) == -1) {
                        perror("Error: something went wrong while closing the file");
                    }

                    filename = strtok_r(NULL, ",", &strtok_status);
                }
                break;
            
            case 'r': // Leggo dal server un(a lista di) file
                // Possono essere specificati più file separati da virgola
                filename = strtok_r(request->arguments, ",", &strtok_status);
                while (filename) {
                    if (openFile(filename, 0) == -1) { // TODO: O_READ
                        perror("Error: can't open the file, skip it");
                        // Non avendo aperto correttamente il file, evito di proseguire
                        filename = strtok_r(NULL, ",", &strtok_status);
                        continue;
                    }

                    if (readFile(filename, &buffer, &size, request->dirname) == -1) {
                        perror("Error: cannot read the file");
                    }

                    if (closeFile(filename) == -1) {
                        perror("Error: something went wrong while closing the file");
                    }

                    filename = strtok_r(NULL, ",", &strtok_status);
                }
                break;

            case 'R': // Leggo dal server un certo numero di files qualsiasi
                // Parso il numero di file da leggere
                if(!(token = strtok_r(request->arguments, "=", &strtok_status))){
                    perror("Error: bad request");
                    break;
                }

                if(!(token = strtok_r(NULL, "=", &strtok_status))){
                    perror("Error: bad request");
                    break;
                }

                if(!is_number(token, &N)){
                    perror("Error: bad request");
                    break;
                }
                
                if(readNFiles(N, request->dirname) == -1){
                    perror("Error: failed to read N files");
                    break;
                }

                break;

            case 'l': // Acquisisco la mutua esclusione su un(a lista di) file
                // Possono essere specificati più file separati da virgola
                filename = strtok_r(request->arguments, ",", &strtok_status);
                while (filename) {
                    // Si suppone che il file sia già stato aperto in lettura dal client
                    openFile(filename, 0);
                    if (lockFile(filename) == -1) {
                        perror("Error: cannot lock file");
                    }

                    filename = strtok_r(NULL, ",", &strtok_status);
                }
                break;

            case 'u': // Rilascio la mutua esclusione su un(a lista di) file
                // Possono essere specificati più file separati da virgola
                filename = strtok_r(request->arguments, ",", &strtok_status);
                while (filename) {
                    // Si suppone che il file sia già stato aperto in scrittura dal client
                    if (unlockFile(filename) == -1) {
                        perror("Error: cannot unlock file");
                    }
                    filename = strtok_r(NULL, ",", &strtok_status);
                }
                break;

            case 'c': // Rimuovo dallo storage un(a lista di) file
                // Possono essere specificati più file separati da virgola
                filename = strtok_r(request->arguments, ",", &strtok_status);
                while (filename) {
                    if (openFile(filename, O_LOCK) == -1) {
                        perror("Error: can't open the file, skip it");
                        // Non avendo aperto correttamente il file, evito di proseguire
                        filename = strtok_r(NULL, ",", &strtok_status);
                        continue;
                    }

                    if (removeFile(filename) == -1) {
                        perror("Error: cannot delete file from storage");
                    }

                    filename = strtok_r(NULL, ",", &strtok_status);
                }
                break;
        }

        // Se -t è stato specificato, mi fermo per il tempo specificato tra una richiesta e la successiva
        if (request->time > 0 && request_queue->length != 0) { // Tuttavia, non aspetto se non ci sono ci sono più richieste da elaborare
            // Struttura dati per la nanosleep
            struct timespec sleep_time = {
                .tv_sec = request->time >= 1000 ? request->time / 1000 : 0,    // Se l'attesa è maggiore di un secondo, uso proprio i secondi come misura
                .tv_nsec = request->time < 1000 ? request->time * 1000000 : 0  // Altrimenti, uso i nanosecondi
            };
            nanosleep(&sleep_time, NULL);  // Aspetto per request->time millisecondi
        }
        // Libero la memoria
        queue_destroy_request(request);
    }

    // * Chiusura della connessione con il server
    if (closeConnection(SOCKET_PATH) == -1) {
        perror("Error: failed to close socket connection");
        EXIT_CODE = errno;
        goto free_and_exit;
    }
    if (VERBOSE) printf("Connection on '%s' closed\n", SOCKET_PATH);

free_and_exit:  // Salto qui solo in caso di errore, per liberare la memoria prima di uscire

    // Libero la memoria ed esco
    if (SOCKET_PATH) free(SOCKET_PATH);
    if (request) queue_destroy_request(request);
    if (request_queue) queue_destroy(request_queue);

    return EXIT_SUCCESS;
}

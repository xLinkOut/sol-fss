// @author Luca Cirillo (545480)

#include <API.h>
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

    bool VERBOSE = false;                   // Flag (p) modalità verbose
    char* SOCKET_PATH = NULL;               // Percorso al socket file del server
    Queue_t* request_queue = queue_init();  // Coda delle richiesta
    if (!request_queue) {
        fprintf(stderr, "Error: failed to create request queue\n");
        return errno;
    }
    Request_t* request = NULL;  // Descrizione di una singola richiesta

    int option;  // Carattere del parametro appena letto da getopt
    while ((option = getopt(argc, argv, ":hpf:w:W:D:r:R:d:t:l:")) != -1) {
        switch (option) {
            // * Path del socket
            case 'f':
                // Controllo che non sia già stato specificato
                if (SOCKET_PATH) {
                    fprintf(stderr, "Error: -f parameter can only be specified once\n");
                    return EINVAL;
                }

                // Salvo il path del socket
                //SOCKET_PATH = strdup(optarg);
                if (!(SOCKET_PATH = malloc(sizeof(char) * (strlen(optarg) + 1)))) {
                    perror("Error: failed to allocate memory for SOCKET_PATH");
                    return errno;
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
                    return errno;
                }
                // Salvo il comando
                request->command = option;
                // Salvo la lista di argomenti
                if (!(request->arguments = malloc(sizeof(char) * strlen(optarg)))) {
                    perror("Error: failed to allocate memory for request arguments");
                    return errno;
                }
                strcpy(request->arguments, optarg);
                break;

            // * Eventuali argomenti
            case 'D':  // Relativa ai comandi 'w' e 'W'
                // Controllo che non sia stato specificato congiuntamente ad una richiesta
                if (!request) {
                    fprintf(stderr, "Error: no request to set this parameter on\n");
                    return EINVAL;
                }
                // Controllo che non sia stato specificato con comandi che non siano 'w' oppure 'W'
                if (request->command != 'w' && request->command != 'W') {
                    fprintf(stderr, "Error: -D must be used in conjunction with -w or -W\n");
                    return EINVAL;
                }
                // Quindi, alloco lo spazio per salvare l'argomento
                if (!(request->dirname = malloc(sizeof(char) * strlen(optarg)))) {
                    perror("Error: failed to allocate memory for request dirname");
                    return errno;
                }
                strcpy(request->dirname, optarg);
                break;
            case 'd':  // Relativa ai comandi 'r' e 'R'
                // Controllo che non sia stato specificato congiuntamente ad una richiesta
                if (!request) {
                    fprintf(stderr, "Error: no request to set this parameter on\n");
                    return EINVAL;
                }
                // Controllo che non sia stato specificato con comandi che non siano 'r' oppure 'R'
                if (request->command != 'r' && request->command != 'R') {
                    fprintf(stderr, "Error: -d must be used in conjunction with -r or -R\n");
                    return EINVAL;
                }
                // Quindi, alloco lo spazio per salvare l'argomento
                if (!(request->dirname = malloc(sizeof(char) * strlen(optarg)))) {
                    perror("Error: failed to allocate memory for arguments");
                    return errno;
                }
                strcpy(request->dirname, optarg);
                break;

            // * Time
            case 't':
                // Controllo che non sia stato specificato congiuntamente ad una richiesta
                if (!request) {
                    fprintf(stderr, "Error: no request to set this parameter on\n");
                    return EINVAL;
                }
                // Controllo che sia effettivamente un numero valido
                // TODO: controllo che sia un numero >= 0
                if (!is_number(optarg, &request->time)) {
                    fprintf(stderr, "Error: time is invalid\n");
                    return EINVAL;
                }
                break;

            // * Verbose
            case 'p':
                // Controllo che non sia già stato specificato
                if (VERBOSE) {
                    fprintf(stderr, "Error: -p parameter can only be specified once\n");
                    return EINVAL;
                }
                VERBOSE = true;
                break;

            // * Messaggio di help
            case 'h':
                // Stampo l'help ed esco
                print_help();
                return EXIT_SUCCESS;
                break;

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
                            return errno;
                        }
                        // Salvo il comando
                        request->command = optopt;  // Non posso usare option perché è uguale a ':', uso optopt
                        // Salvo la lista di argomenti
                        if (!(request->arguments = malloc(sizeof(char) * 4))) {
                            perror("Error: failed to allocate memory for request arguments");
                            return errno;
                        }
                        strcpy(request->arguments, "n=0");
                        break;
                    default:
                        fprintf(stderr, "Parameter -%c takes an argument\n", optopt);
                        return EINVAL;
                }
                break;

            // * Parametro sconosciuto
            case '?':
                printf("Unknown parameter '%c'\n", optopt);
                return EINVAL;
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
        return errno;
    }
    if(VERBOSE) printf("Connection established correctly to '%s'\n", SOCKET_PATH);

    // * Esecuzione delle richieste, in ordine FIFO
    /*
    while((request = queue_pop(request_queue))){
        printf("%c\n", request->command);
    }
    */

    // * Chiusura della connessione con il server
    if(closeConnection(SOCKET_PATH) == -1){
        perror("Error: failed to close socket connection");
        return errno;
    }
    if(VERBOSE) printf("Connection on '%s' closed\n", SOCKET_PATH);

    return EXIT_SUCCESS;
}

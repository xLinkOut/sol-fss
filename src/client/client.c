// @author Luca Cirillo (545480)

#include <errno.h>
#include <getopt.h>
#include <request_queue.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Liberare eventuale memoria prima di uscire
//  TODO: anche nel caso di -h

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

int main(int argc, char* argv[]) {
    // Se non viene specificato alcun parametro, stampo l'usage ed esco
    if (argc == 1) {
        printf("Usage: %s [OPTIONS]...\n\nSee '%s -h' for more information\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    // Altrimenti, parso i parametri passati da riga di comando

    // Nella optstring, il primo ':' serve a distinguere il caso di carattere sconosciuto, che ritorna '?',
    // dal caso di mancato argomento, che invece ritorna proprio ':'.
    // Il parametro -R, che prevede un argomento opzionale, verrà gestito nel caso ':'

    bool VERBOSE = false;                   // Flag (p) modalità verbose
    char* SOCKET_PATH = NULL;               // Percorso al socket file del server
    Queue_t* request_queue = queue_init();  // Coda delle richiesta
    if (!request_queue) {
        fprintf(stderr, "Error: failed to initialize request queue\n");
        return EXIT_FAILURE;
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
                    return EXIT_FAILURE;
                }
                // Salvo il path del socket
                if (!(SOCKET_PATH = malloc(sizeof(char) * strlen(optarg)))) {
                    perror("Error: failed to allocate memory for SOCKET_PATH");
                    return EXIT_FAILURE;
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
                    return EXIT_FAILURE;
                }
                // Salvo il comando
                request->command = option;
                // Salvo la lista di argomenti
                if (!(request->arguments = malloc(sizeof(char) * strlen(optarg)))) {
                    perror("Error: failed to allocate memory for request arguments");
                    return EXIT_FAILURE;
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
                    return EXIT_FAILURE;
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
                    return EXIT_FAILURE;
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
                    return EXIT_FAILURE;
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
                        // TODO:
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

    return EXIT_SUCCESS;
}

// @author Luca Cirillo (545480)

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Liberare eventuale memoria prima di uscire
//  TODO: anche nel caso di -h

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

    char* SOCKET_PATH = NULL;  // Percorso al socket file del server
    bool VERBOSE = false;      // Flag (p) modalità verbose

    int option;  // Carattere del parametro appena letto da getopt
    while ((option = getopt(argc, argv, ":hpf:w:W:D:r:R:d:t:l:")) != -1) {
        switch (option) {
            case 'f':  // * -f socketname
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

            case 'p':  // * -p
                // Controllo che non sia già stato specificato
                if (VERBOSE) {
                    fprintf(stderr, "Error: -p parameter can only be specified once\n");
                    return EXIT_FAILURE;
                }
                VERBOSE = true;
                break;

            case 'h':  // * -h
                // Stampo l'help ed esco
                print_help();
                return EXIT_SUCCESS;
                break;

            case ':':              // * Argomento non specificato
                switch (optopt) {  // Controllo che non sia R, che ha un argomento facoltativo
                    case 'R':      // E' proprio R
                        // TODO: Imposto n = 0
                        break;
                    default:
                        fprintf(stderr, "Parameter -%c takes an argument\n", optopt);
                        return EINVAL;
                }
                break;

            case '?':  // * Parametro sconosciuto
                printf("Unknown parameter '%c'\n", optopt);
                return EINVAL;
        }
    }

    return EXIT_SUCCESS;
}
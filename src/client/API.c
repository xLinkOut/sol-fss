// @author Luca Cirillo (545480)

#include <API.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <utils.h>
#include <stdio.h>
#include <unistd.h>
#include <constants.h>
#include <sys/stat.h>

// Imposto il socket con un valore negativo, e.g. 'non connesso'
int client_socket = -1;
// Buffer per memorizzare i dati da inviare al server
char message_buffer[REQUEST_LENGTH];

int openConnection(const char* sockname, int msec, const struct timespec abstime) {
    // Controllo la validità degli argomenti
    if (!sockname || msec < 0 || abstime.tv_sec < 0 || abstime.tv_nsec < 0) {
        errno = EINVAL;
        return -1;
    }

    // Controllo che il client non abbia già instaurato una connessione al server
    if (client_socket > -1) {
        errno = EISCONN;
        return -1;
    }

    struct sockaddr_un socket_address;
    // Inizializzo la struttura
    memset(&socket_address, '0', sizeof(socket_address));
    socket_address.sun_family = AF_UNIX;
    // Imposto come path quello del socket passato come argomento
    // TODO: controlli su SOCKET_PATH; strncpy
    strcpy(socket_address.sun_path, sockname);

    // Creo il socket lato client, che si connetterà al server
    client_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    // Controllo la buona riuscita dell'operazione
    if (client_socket == -1) return -1;

    // Struttura dati per la nanosleep
    struct timespec sleep_time = {
        .tv_sec = msec >= 1000 ? msec / 1000 : 0,    // Se l'attesa è maggiore di un secondo, uso proprio i secondi come misura
        .tv_nsec = msec < 1000 ? msec * 1000000 : 0  // Altrimenti, uso i nanosecondi
    };

    int connect_status = -1;
    // 'Aspetto e riprovo' finché la connessione non va a buon fine e non è ancora scaduto il timeout
    while ((connect_status = connect(client_socket, (struct sockaddr*)&socket_address, sizeof(socket_address))) == -1 && time(NULL) < abstime.tv_sec) {
        //printf("Connection failed, I'll try again ...\n");
        nanosleep(&sleep_time, NULL);  // Aspetto per msec
    }

    if (connect_status == -1) errno = ETIMEDOUT;
    return connect_status;
}

int closeConnection(const char* sockname){
    // Controllo la validità degli argomenti
    if(!sockname){
        errno = EINVAL;
        return -1;
    }

    // Mando al server un messaggio di uscita
    // Svuoto il buffer di comunicazione
    memset(message_buffer, 0, REQUEST_LENGTH);
    // Scrivo nel buffer il comando per chiudere la connessione
    snprintf(message_buffer, REQUEST_LENGTH, "%d", DISCONNECT);
    // Invio il messaggio al server
    if(writen((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    // Non aspetto una risposta dal server, chiuso il socket
    if(close(client_socket) == -1){
        client_socket = -1;
        return -1;
    }

    // Resetto il socket
    client_socket = -1;

    return 0;
}

int openFile(const char* pathname, int flags){
    // Controllo la validità degli argomenti
    if(!pathname){
        errno = EINVAL;
        return -1;
    }

    // Controllo che sia stata instaurata una connessione con il server
    if(client_socket == -1){
        errno = ENOTCONN;
        return -1;
    }

    // Preparo la richiesta da inviare
    memset(message_buffer, 0, REQUEST_LENGTH);
    snprintf(message_buffer, REQUEST_LENGTH, "%d %s %d", OPEN, pathname, flags);
    if(writen((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    printf("openFile sent\n");

    // Leggo la risposta
    memset(message_buffer, 0, REQUEST_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    // TODO: if(VERBOSE)
    switch(status){
        case SUCCESS:
            printf("openFile success\n");
            return 0;
            break;
        default:
            fprintf(stderr, "Something went wrong\n");
    }

    return -1;
}

int readFile(const char* pathname, void** buf, size_t* size){
    // Controllo la validità degli argomenti
    if(!pathname || !buf || !size){
        errno = EINVAL;
        return -1;
    }

    // Controllo che sia stata instaurata una connessione con il server
    if(client_socket == -1){
        errno = ENOTCONN;
        return -1;
    }

    // Invio al server la richiesta di READ
    memset(message_buffer, 0, REQUEST_LENGTH);
    snprintf(message_buffer, REQUEST_LENGTH, "%d %s", READ, pathname);
    if(writen((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    printf("readFile sent\n");

    // Ricevo dal server un messaggio di conferma e, se il file è stato trovato, la sua dimensione
    memset(message_buffer, 0, REQUEST_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }
    char* strtok_status = NULL;
    char* token = strtok_r(message_buffer, " ", &strtok_status);
    if (!token) return -1;
    int result = 0;
    if (sscanf(token, "%d", &result) != 1) {
        return -1;
    }
    if(result == 0){
        errno = ENOENT;
        return -1;
    }else if(result == -1){
        errno = EPERM;
        return -1;
    }

    // result = 1 => file trovata e permessi ok
    token = strtok_r(NULL, " ", &strtok_status);
    size_t size_from_server = 0;
    if (sscanf(token, "%zd", &size_from_server) != 1) {
        return -1;
    }
    *size = size_from_server;
    //printf("%d %zd\n", result, size_from_server);

    // Ricevo il file dal server
    *buf = malloc(*size);
    memset(*buf, 0, *size);
    if(readn((long)client_socket, *buf, *size) == -1){
        return -1;
    }
    //printf("\t%s\n", (char*)*buf);
     // Leggo la risposta
    memset(message_buffer, 0, REQUEST_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    // TODO: if(VERBOSE)
    switch(status){
        case SUCCESS:
            printf("readFile success\n");
            return 0;
            break;
        default:
            fprintf(stderr, "Something went wrong\n");
    }

    return -1;
}

int writeFile(const char* pathname, const char* dirname){
    // Controllo la validità degli argomenti
    if(!pathname){
        errno = EINVAL;
        return -1;
    }

    // Controllo che sia stata instaurata una connessione con il server
    if(client_socket == -1){
        errno = ENOTCONN;
        return -1;
    }
    
    // Controllo che il file esista e che dispongo dei permessi necessari per poterlo leggere
    if (access(pathname, R_OK) == -1) {
        errno = EPERM;
        return -1;
    }
    
    /* Schema:
        Client: invia pathname e dimensione del file da salvare nello storage
        Server: effettua i controlli necessari sui parametri:
            File <pathname> presente nello storage √
            Client ha i diritti di scrittura sul file √
            Dimensione non superiore alla massima dello storage √
            Lo storage ha spazio a sufficienza per contenere il file √
                No? Faccio partire il procedimento di replace
        Server: determina se è necessario espellere uno o più file per liberare spazio
        Server: eventualmente, invia al client i file espulsi da salvare
        Client: se dirname è stato specificato, salva li i file espulsi dallo storage
        Client: al termine, invia il file da salvare
    */
    
    // Apro il file
    FILE* file = fopen(pathname, "r");
    if(!file) return -1; 

    // Determino la dimensione del file e la invio al server
    struct stat file_stat; // * file_stat.st_size è la dimensione del file
    if(stat(pathname, &file_stat) == -1){
        fclose(file);
        return -1;
    }

    // Invio al server la richiesta di WRITE, il pathname e la dimensione del file
    memset(message_buffer, 0, REQUEST_LENGTH);
    snprintf(message_buffer, REQUEST_LENGTH, "%d %s %lld", WRITE, pathname, file_stat.st_size);
    if(writen((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    printf("writeFile sent\n");

    // Attendo di ricevere il numero di file espulsi
    int victims_no = 0;
    memset(message_buffer, 0, REQUEST_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }
    if(sscanf(message_buffer, "%d", &victims_no) != 1){
        errno = EBADMSG;
        return -1;
    }
    
    // Ciclo fino a ricevere tutti i file espulsi
    char victim_pathname[MESSAGE_LENGTH]; 
    size_t size = 0;
    char* token = NULL;
    char* strtok_status = NULL;

    while(victims_no > 0){
        // Leggo il pathname e la dimensione del file espulso
        memset(message_buffer, 0, REQUEST_LENGTH);
        memset(victim_pathname, 0, MESSAGE_LENGTH);
        if(readn((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
            return -1;
        }
        // Pathname
        token = strtok_r(message_buffer, " ", &strtok_status);
        if (!token || sscanf(token, "%s", victim_pathname) != 1) {
            errno = EBADMSG;
            return -1;
        }
        // Size
        token = strtok_r(NULL, " ", &strtok_status);
        if (!token || sscanf(token, "%zd", &size) != 1) {
            errno = EBADMSG;
            return -1;
        }
        
        // Alloco spazio per il file
        void* contents = malloc(size);
        if(!contents){
            perror("malloc");
            return -1;
        }
        memset(contents, 0, size);
        if(readn((long)client_socket, (void*)contents, size) == -1){
            return -1;
        }

        // Se il client ha specificato una cartella in cui salvare i file espulsi
        if(dirname){
            FILE* output_file = NULL;
            char* abs_path[4096]; // TODO: define
            snprintf(abs_path, 4096, strrchr(dirname, '/') ? "%s%s" : "%s/%s", dirname, victim_pathname);
            output_file = fopen(abs_path, "w");
            if(!output_file) return -1;
            if(fputs(contents, output_file) == EOF){
                fclose(output_file);
                return -1;
            }
            if(fclose(output_file) == -1) return -1;
        }

        free(contents);
        victims_no--;
    }

    // Alloco la memoria necessaria per leggere il file
    void* contents = malloc(file_stat.st_size);
    // Leggo il contenuto del file come un unico blocco di dimensione st_size
    fread(contents, file_stat.st_size, 1, file);

    // Invio il contenuto del file che voglio scrivere
    if (writen((long)client_socket, (void*)contents, file_stat.st_size) == -1) {
        perror("Error: writen failed");
        return -1;
    }

    // Leggo la risposta
    memset(message_buffer, 0, REQUEST_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    // TODO: if(VERBOSE)
    switch(status){
        case SUCCESS:
            printf("writeFile success\n");
            return 0;
            break;
        default:
            fprintf(stderr, "Something went wrong\n");
    }

    return -1;
}

int closeFile(const char* pathname){
    // Controllo la validità degli argomenti
    if(!pathname){
        errno = EINVAL;
        return -1;
    }

    // Controllo che sia stata instaurata una connessione con il server
    if(client_socket == -1){
        errno = ENOTCONN;
        return -1;
    }

    // Preparo la richiesta da inviare
    memset(message_buffer, 0, REQUEST_LENGTH);
    snprintf(message_buffer, REQUEST_LENGTH, "%d %s", CLOSE, pathname);
    if(writen((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    printf("closeFile sent\n");

    // Leggo la risposta
    memset(message_buffer, 0, REQUEST_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, REQUEST_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    // TODO: if(VERBOSE)
    switch(status){
        case SUCCESS:
            printf("closeFile success\n");
            return 0;
            break;
        default:
            fprintf(stderr, "Something went wrong\n");
    }

    return -1;
}

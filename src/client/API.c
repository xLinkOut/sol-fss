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
#include <stdbool.h>

// Imposto il socket con un valore negativo, e.g. 'non connesso'
int client_socket = -1;
// Buffer per memorizzare i dati da inviare al server
char message_buffer[MESSAGE_LENGTH];
// Modalità verbose
bool VERBOSE = false;

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

    // Controllo che il client sia connesso al server
    if (client_socket == -1) {
        errno = ENOTCONN;
        return -1;
    }

    // Mando al server un messaggio di uscita
    // Svuoto il buffer di comunicazione
    memset(message_buffer, 0, MESSAGE_LENGTH);
    // Scrivo nel buffer il comando per chiudere la connessione
    snprintf(message_buffer, MESSAGE_LENGTH, "%d", DISCONNECT);
    // Invio il messaggio al server
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
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
    if(!pathname || flags < 0){
        errno = EINVAL;
        return -1;
    }

    // Controllo che sia stata instaurata una connessione con il server
    if(client_socket == -1){
        errno = ENOTCONN;
        return -1;
    }

    // Preparo la richiesta da inviare
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %s %d", OPEN, pathname, flags);
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    if(VERBOSE) printf("Request to open '%s' file... \n", pathname);

    // Leggo la risposta
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    switch(status){
        case SUCCESS:
            if (VERBOSE) printf("File opened successfully!\n");
            return 0;
        
        default:
            if (VERBOSE) printf("Something went wrong!\n");
    }

    return -1;
}

int readFile(const char* pathname, void** buf, size_t* size, const char* dirname){
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
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %s", READ, pathname);
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    if (VERBOSE) printf("Request to read '%s' file...\n", pathname);

    // Ricevo dal server un messaggio di conferma e, se il file è stato trovato, la sua dimensione
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
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
    *buf = malloc(*size); // Chiamare la free di questa memoria è compito del client
    memset(*buf, 0, *size);
    if(readn((long)client_socket, *buf, *size) == -1){
        return -1;
    }

    // Se <dirname> è stata specificata, salvo il file sul disco
    if(dirname){
        // Creo il path completo per il salvataggio del file
        // Calcolo la lunghezza del path indicato da dirname
        size_t dirname_length = strlen(dirname);
        char abs_path[PATH_MAX]; // => dirname/pathname, // TODO: MAX_PATH in linux/limits.h
        memset(abs_path, 0, PATH_MAX);
        
        // Gestisco il caso in cui dirname termina con '/' e victim_pathname inizia con '/'
        //if(dirname[dirname_length-1] == '/' && victim_pathname[0] == '/') dirname[dirname_length-1] = '\0';
        // Controllo se dirname termina con '/' oppure victim_pathname inizia con '/'
        int slash = dirname[dirname_length-1] == '/' || pathname[0] == '/';
        // Se dirname non termina con '/', e victim_name non inizia con '/', lo aggiungo tra i due
        snprintf(abs_path, PATH_MAX, slash ? "%s%s" : "%s/%s", dirname, pathname);

        // Per mantenere l'integrità del path assoluto del file che ho ricevuto dal server
        //  ho eventualmente bisogno di creare all'interno di dirname una struttura di cartelle
        //  per poter contenere il file, in maniera ricorsiva. Un comportamento simile al comando 'mkdir -p <path>'
        mkdir_p(abs_path);

        // Salvo il contenuto del file sul disco
        FILE* output_file = fopen(abs_path, "w");
        if(!output_file) return -1;
        if(fputs(*buf, output_file) == EOF){
            fclose(output_file);
            return -1;
        }
        if(fclose(output_file) == -1) return -1;
        if(VERBOSE) printf("%zd bytes saved to '%s'!\n", *size, abs_path);
    }

     // Leggo la risposta
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    switch(status){
        case SUCCESS:
            if (VERBOSE) printf("Successfully read %zd bytes!\n", *size);
            return 0;
        
        default:
            if (VERBOSE) printf("Something went wrong!\n");
    }

    return -1;
}

int readNFiles(int N, const char* dirname) {
    // Controllo che sia stata instaurata una connessione con il server
    if (client_socket == -1) {
        errno = ENOTCONN;
        return -1;
    }

    // Invio al server la richiesta di READN
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %d", READN, N);
    if (writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1) {
        return -1;
    }

    if (VERBOSE) printf("Request to read %d file(s)...\n", N);

    // Ricevo dal server il numero di file effettivamente letti
    int files_no = 0;
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if (readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1) {
        return -1;
    }
    if (sscanf(message_buffer, "%d", &files_no) != 1) {
        errno = EBADMSG;
        return -1;
    }

    // Se il numero di file è negativo, qualcosa è andato storto
    if (files_no < 0) {
        if (VERBOSE) printf("Something went wrong!\n");
        return -1;
    }

    // Se il numero di file è zero, nessun file è disponibile per la lettura
    if (files_no == 0) {
        if (VERBOSE) printf("There are no files available for reading\n");
        return 0;
    }

    // files_no > 0
    if (VERBOSE) printf("%d file(s) have been read from the server\n", files_no);
    
    int i = 0;
    char* token = NULL;                  // Appoggio per strtok_r
    size_t file_size = 0;                // Dimensione del file da leggere
    void* file_contents = NULL;          // Contenuto del file da leggere
    char* strtok_status = NULL;          // Stato per strtok_r
    char file_pathname[MESSAGE_LENGTH];  // Pathname del file

    for (; i < files_no; i++) {
        // Ricevo dal server il nome e la dimensione del file
        memset(message_buffer, 0, MESSAGE_LENGTH);
        memset(file_pathname, 0, MESSAGE_LENGTH);
        if (readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1) {
            return -1;
        }

        // Pathname
        token = strtok_r(message_buffer, " ", &strtok_status);
        if (!token || sscanf(token, "%s", file_pathname) != 1) {
            errno = EBADMSG;
            return -1;
        }
        // Size
        token = strtok_r(NULL, " ", &strtok_status);
        if (!token || sscanf(token, "%zu", &file_size) != 1) {
            errno = EBADMSG;
            return -1;
        }

        if (VERBOSE) printf("Receiving file n.%d (%zd bytes): '%s' \n", i + 1, file_size, file_pathname);

        // Alloco spazio per il file
        file_contents = malloc(file_size + 1);
        if (!file_contents) return -1;
        memset(file_contents, 0, file_size + 1);

        // Ricevo il contenuto del file
        if (readn((long)client_socket, file_contents, file_size) == -1) {
            free(file_contents);
            return -1;
        }

        // Se il client ha specificato una cartella in cui salvare i file letti,
        //  procedo a salvare i file ricreando l'albero delle directories specificato nel pathname
        if (dirname) {
            // Creo il path completo per il salvataggio del file
            // Calcolo la lunghezza del path indicato da dirname
            size_t dirname_length = strlen(dirname);
            char abs_path[PATH_MAX];  // => dirname/file_pathname, // TODO: MAX_PATH in linux/limits.h
            memset(abs_path, 0, PATH_MAX);

            // Gestisco il caso in cui dirname termina con '/' e file_pathname inizia con '/'
            //if(dirname[dirname_length-1] == '/' && file_pathname[0] == '/') dirname[dirname_length-1] = '\0';
            // Controllo se dirname termina con '/' oppure file_pathname inizia con '/'
            int slash = dirname[dirname_length - 1] == '/' || file_pathname[0] == '/';
            // Se dirname non termina con '/', e victim_name non inizia con '/', lo aggiungo tra i due
            snprintf(abs_path, PATH_MAX, slash ? "%s%s" : "%s/%s", dirname, file_pathname);

            // Per mantenere l'integrità del path assoluto del file che ho ricevuto dal server
            //  ho eventualmente bisogno di creare all'interno di dirname una struttura di cartelle
            //  per poter contenere il file, in maniera ricorsiva. Un comportamento simile al comando 'mkdir -p <path>'
            if (VERBOSE) printf("Saving file to '%s'\n", abs_path);
            mkdir_p(abs_path);

            // Apro il file in scrittura
            FILE* output_file = fopen(abs_path, "w");
            if (!output_file) {
                free(file_contents);
                return -1;
            }

            // Scrivo il contenuto del file su disco
            if (fputs(file_contents, output_file) == EOF) {
                fclose(output_file);
                free(file_contents);
                return -1;
            }

            // Chiudo il file
            if (fclose(output_file) == -1) {
                free(file_contents);
                return -1;
            }

            if (VERBOSE) printf("%zd bytes saved to '%s'\n", file_size, abs_path);
        }
        free(file_contents);
    }

    return files_no;
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
    
    // Apro il file
    FILE* file = fopen(pathname, "r");
    if(!file) return -1; 

    // Determino la dimensione del file
    struct stat file_stat; // * file_stat.st_size è la dimensione del file
    if(stat(pathname, &file_stat) == -1){
        fclose(file);
        return -1;
    }

    // Invio al server la richiesta di WRITE, il pathname e la dimensione del file
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %s %lld", WRITE, pathname, file_stat.st_size);
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    if(VERBOSE) printf("Request to write %lld bytes in '%s' file...\n", file_stat.st_size, pathname);

    // Alloco la memoria necessaria per leggere il file
    void* contents = malloc(file_stat.st_size);
    if(!contents) return -1;
    // Leggo il contenuto del file come un unico blocco di dimensione file_stat.st_size
    fread(contents, file_stat.st_size, 1, file);
    // Chiudo il file
    if(fclose(file) == -1){
        free(contents);
        return -1;
    }

    // Invio il contenuto del file al server
    if (writen((long)client_socket, contents, file_stat.st_size) == -1) {
        return -1;
    }

    // Libero la memoria dal file letto
    free(contents);

    // Ricevo dal server eventuali file espulsi
    int victims_no = 0;
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }
    if(sscanf(message_buffer, "%d", &victims_no) != 1){
        errno = EBADMSG;
        return -1;
    }
    
    char victim_pathname[MESSAGE_LENGTH];
    size_t victim_size = 0;
    void* victim_contents = NULL;
    char* token = NULL;
    char* strtok_status = NULL;
    int i = 0;

    if(victims_no > 0){
        if (VERBOSE) printf("%d file(s) have been ejected from the server\n", victims_no);
        for(;i<victims_no;i++){
            // Ricevo dal server il nome e la dimensione del file
            memset(message_buffer, 0, MESSAGE_LENGTH);
            memset(victim_pathname,0,MESSAGE_LENGTH);
            if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
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
            if (!token || sscanf(token, "%zd", &victim_size) != 1) {
                errno = EBADMSG;
                return -1;
            }

            if (VERBOSE) printf("Receiving file n.%d (%zd bytes): '%s' \n", i+1, victim_size, victim_pathname);

            // Alloco spazio per il file
            victim_contents = malloc(victim_size);
            if(!victim_contents) return -1;

            memset(victim_contents, 0, victim_size);
            if(readn((long)client_socket, victim_contents, victim_size) == -1){
                return -1;
            }

            // Se il client ha specificato una cartella in cui salvare i file espulsi,
            //  procedo a salvare i file ricreando l'albero delle directories specificato nel pathname
            if(dirname){
                // Creo il path completo per il salvataggio del file
                // Calcolo la lunghezza del path indicato da dirname
                size_t dirname_length = strlen(dirname);
                char abs_path[PATH_MAX]; // => dirname/victim_pathname, // TODO: MAX_PATH in linux/limits.h
                memset(abs_path, 0, PATH_MAX);
                
                // Gestisco il caso in cui dirname termina con '/' e victim_pathname inizia con '/'
                //if(dirname[dirname_length-1] == '/' && victim_pathname[0] == '/') dirname[dirname_length-1] = '\0';
                // Controllo se dirname termina con '/' oppure victim_pathname inizia con '/'
                int slash = dirname[dirname_length-1] == '/' || victim_pathname[0] == '/';
                // Se dirname non termina con '/', e victim_name non inizia con '/', lo aggiungo tra i due
                snprintf(abs_path, PATH_MAX, slash ? "%s%s" : "%s/%s", dirname, victim_pathname);\

                // Per mantenere l'integrità del path assoluto del file che ho ricevuto dal server
                //  ho eventualmente bisogno di creare all'interno di dirname una struttura di cartelle
                //  per poter contenere il file, in maniera ricorsiva. Un comportamento simile al comando 'mkdir -p <path>'
                mkdir_p(abs_path);

                // Salvo il contenuto del file sul disco
                FILE* output_file = fopen(abs_path, "w");
                if(!output_file) return -1;
                if(fputs(victim_contents, output_file) == EOF){
                    fclose(output_file);
                    return -1;
                }
                if(fclose(output_file) == -1) return -1; 
                if(VERBOSE) printf("%zd bytes saved to '%s'!\n", victim_size, abs_path);
            }

            free(victim_contents);
        }
    }

    // Leggo la risposta
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    switch(status){
        case SUCCESS:
           if (VERBOSE) printf("%lld bytes written successfully!\n", file_stat.st_size);
            return 0;

        default:
            if(VERBOSE) printf("Something went wrong!\n");
    }

    return -1;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
    // Controllo la validità degli argomenti
    if(!pathname || !buf || size == 0){
        errno = EINVAL;
        return -1;
    }

    // Controllo che sia stata instaurata una connessione con il server
    if(client_socket == -1){
        errno = ENOTCONN;
        return -1;
    }

    // Invio al server la richiesta di APPEND, il pathname e la dimensione del file
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %s %zd", APPEND, pathname, size);
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    if(VERBOSE) printf("Request to append %zd bytes to '%s' file...\n", size, pathname);
    
    // Invio il contenuto del file che voglio scrivere
    if (writen((long)client_socket, buf, size) == -1) {
        return -1;
    }

    // Ricevo dal server eventuali file espulsi
    int victims_no = 0;
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }
    if(sscanf(message_buffer, "%d", &victims_no) != 1){
        errno = EBADMSG;
        return -1;
    }
    
    char victim_pathname[MESSAGE_LENGTH];
    size_t victim_size = 0;
    void* victim_contents = NULL;
    char* token = NULL;
    char* strtok_status = NULL;
    int i = 0;

    if(victims_no > 0){
        if (VERBOSE) printf("%d file(s) have been ejected from the server\n", victims_no);
        for(;i<victims_no;i++){
            // Ricevo dal server il nome e la dimensione del file
            memset(message_buffer, 0, MESSAGE_LENGTH);
            memset(victim_pathname,0,MESSAGE_LENGTH);
            if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
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
            if (!token || sscanf(token, "%zd", &victim_size) != 1) {
                errno = EBADMSG;
                return -1;
            }

            if (VERBOSE) printf("Receiving file n.%d (%zd bytes): '%s' \n", i+1, victim_size, victim_pathname);

            // Alloco spazio per il file
            victim_contents = malloc(victim_size);
            if(!victim_contents) return -1;
            memset(victim_contents, 0, victim_size);
            if(readn((long)client_socket, victim_contents, victim_size) == -1){
                return -1;
            }

            // Se il client ha specificato una cartella in cui salvare i file espulsi,
            //  procedo a salvare i file ricreando l'albero delle directories specificato nel pathname
            if(dirname){
                // Creo il path completo per il salvataggio del file
                // Calcolo la lunghezza del path indicato da dirname
                size_t dirname_length = strlen(dirname);
                char abs_path[PATH_MAX]; // => dirname/victim_pathname, // TODO: MAX_PATH in linux/limits.h
                memset(abs_path, 0, PATH_MAX);
                
                // Gestisco il caso in cui dirname termina con '/' e victim_pathname inizia con '/'
                //if(dirname[dirname_length-1] == '/' && victim_pathname[0] == '/') dirname[dirname_length-1] = '\0';
                // Controllo se dirname termina con '/' oppure victim_pathname inizia con '/'
                int slash = dirname[dirname_length-1] == '/' || victim_pathname[0] == '/';
                // Se dirname non termina con '/', e victim_name non inizia con '/', lo aggiungo tra i due
                snprintf(abs_path, PATH_MAX, slash ? "%s%s" : "%s/%s", dirname, victim_pathname);

                // Per mantenere l'integrità del path assoluto del file che ho ricevuto dal server
                //  ho eventualmente bisogno di creare all'interno di dirname una struttura di cartelle
                //  per poter contenere il file, in maniera ricorsiva. Un comportamento simile al comando 'mkdir -p <path>'
                mkdir_p(abs_path);

                // Salvo il contenuto del file sul disco
                FILE* output_file = fopen(abs_path, "w");
                if(!output_file) return -1;
                if(fputs(victim_contents, output_file) == EOF){
                    fclose(output_file);
                    return -1;
                }
                if(fclose(output_file) == -1) return -1; 
                if(VERBOSE) printf("%zd bytes saved to '%s'!\n", victim_size, abs_path);
            }

            free(victim_contents);
        }
    }

    // Leggo la risposta
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    switch(status){
        case SUCCESS:
           if (VERBOSE) printf("%zd bytes added successfully!\n", size);
            return 0;

        default:
            if(VERBOSE) printf("Something went wrong!\n");
    }

    return -1;

}

int lockFile(const char* pathname){
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
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %s", LOCK, pathname);
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    if(VERBOSE) printf("Request to lock '%s' file...\n", pathname);

    // Leggo la risposta
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    switch(status){
        case SUCCESS:
           if (VERBOSE) printf("File locked successfully!\n");
            return 0;

        default:
            if(VERBOSE) printf("Something went wrong!\n");
    }
    return -1;
}

int unlockFile(const char* pathname){
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
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %s", UNLOCK, pathname);
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    if(VERBOSE) printf("Request to unlock '%s' file...\n", pathname);

    // Leggo la risposta
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    switch(status){
        case SUCCESS:
           if (VERBOSE) printf("File unlocked successfully!\n");
            return 0;

        default:
            if(VERBOSE) printf("Something went wrong!\n");
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
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %s", CLOSE, pathname);
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    if(VERBOSE) printf("Request to close '%s' file... \n", pathname);

    // Leggo la risposta
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    switch(status){
        case SUCCESS:
            if (VERBOSE) printf("File closed successfully!\n");
            return 0;
        
        default:
            if (VERBOSE) printf("Something went wrong!\n");
    }
    return -1;
}

int removeFile(const char* pathname){
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
    memset(message_buffer, 0, MESSAGE_LENGTH);
    snprintf(message_buffer, MESSAGE_LENGTH, "%d %s", REMOVE, pathname);
    if(writen((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    if(VERBOSE) printf("Request to remove '%s' file... \n", pathname);

    // Leggo la risposta
    memset(message_buffer, 0, MESSAGE_LENGTH);
    if(readn((long)client_socket, (void*)message_buffer, MESSAGE_LENGTH) == -1){
        return -1;
    }

    // Interpreto (il codice del)la risposta ricevuta
    response_code status;
    if(sscanf(message_buffer, "%d", &status) != 1){
        errno = EBADMSG;
        return -1;
    }

    switch(status){
        case SUCCESS:
            if (VERBOSE) printf("File removed successfully!\n");
            return 0;
        
        default:
            if (VERBOSE) printf("Something went wrong!\n");
    }

    return -1;
}

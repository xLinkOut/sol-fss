// @author Luca Cirillo (545480)

#include <request_queue.h>
#include <stdlib.h>

request_t* request_create() {
    // Alloco memoria per la nuova richiesta
    request_t* request = malloc(sizeof(request_t));
    if (!request) return NULL;

    // Imposto il suo contenuto
    request->arguments = NULL;
    request->dirname = NULL;
    request->time = 0;

    // Ritorno un puntatore alla richiesta appena creata
    return request;
}

void request_destroy(void* request) {
    // Controllo la validità degli argomenti
    if (!request) return;
    // Converto il puntatore nel tipo request_t*
    request_t* req = (request_t*) request;
    // Libero l'eventuale memoria allocata per gli argomenti
    if (req->arguments) free(req->arguments);
    // Libero l'eventuale memoria allocata per le opzioni -D oppure -d
    if (req->dirname) free(req->dirname);
    // Infine libero la memoria della richiesta
    free(req);
}

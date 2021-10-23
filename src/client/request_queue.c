// @author Luca Cirillo (545480)

#include <request_queue.h>
#include <stdio.h>
#include <stdlib.h>

queue_t* queue_init() {
    // Alloco memoria per la coda
    queue_t* queue = malloc(sizeof(queue_t));
    if (!queue) return NULL;
    // Imposto il nodo di testa e di coda uguale NULL, la coda al momento è vuota
    queue->head = queue->tail = NULL;
    // Imposto la lunghezza pari a zero, non ci sono elementi in coda
    queue->length = 0;
    // Ritorno la coda pronta all'uso
    return queue;
}

void queue_destroy(queue_t* queue) {
    // Controllo la validità degli argomenti
    if (!queue) return;
    request_t* request = NULL;
    // Finché ci sono elementi in coda
    while (queue->head != queue->tail){
        // Li rimuovo
        request = queue->head;
        queue->head = queue->head->next;
        queue_destroy_request(request);
    }
    // Infine, libero la memoria della coda
    free(queue);
}

int queue_push(queue_t* queue, request_t* new_request) {
    // Controllo la validità degli argomenti
    if (!queue || !new_request) return -1;
    // Se la coda è vuota, il nuovo elemento diventa sia testa che coda
    if (!queue->tail) {
        queue->head = queue->tail = new_request;
        queue->length++;
        return 0;
    }
    // Altrimenti, lo aggiungo in coda
    queue->tail->next = new_request;
    queue->tail = new_request;
    queue->length++;
    return 0;
}

request_t* queue_pop(queue_t* queue) {
    // Controllo la validità degli argomenti e se la coda non è vuota
    if (!queue || !queue->head) return NULL;
    // Tengo traccia della richiesta in testa, che sarà rimossa dalla coda
    request_t* old_head = queue->head;
    // La richiesta successiva diventa ora la nuova testa
    queue->head = queue->head->next;
    // Se la testa diventa NULL, ovvero non ci sono più elementi, imposto a NULL anche la coda
    if (queue->head == NULL) queue->tail = NULL;
    // Decremento il contatore degli elementi attualmente in coda
    queue->length--;
    // Ritorno infine la richiesta estratta
    return old_head;
}

request_t* queue_new_request() {
    // Alloco memoria per la nuova richiesta
    request_t* request = malloc(sizeof(request_t));
    if (!request) return NULL;

    // Imposto il suo contenuto
    request->arguments = NULL;
    request->dirname = NULL;
    request->time = 0;
    request->next = NULL;

    // Ritorno un puntatore alla richiesta appena creata
    return request;
}

void queue_destroy_request(request_t* request) {
    // Controllo la validità degli argomenti
    if (!request) return;
    // Libero l'eventuale memoria allocata per gli argomenti
    if (request->arguments) free(request->arguments);
    // Libero l'eventuale memoria allocata per le opzioni -D oppure -d
    if (request->dirname) free(request->dirname);
    // Infine libero la memoria della richiesta
    free(request);
}

void queue_print(queue_t* queue){
    // Controllo la validità degli argomenti
    if(!queue) return;
    // Salvo un puntatore alla testa della lista
    request_t* scan = queue->head;
    // Inizializzo un contatore per maggiore leggibilità
    int counter = 0;
    // Finché non raggiungo la fine della coda
    while(scan){
        // Stampo le informazioni della richiesta
        printf("[%d]: <%c, %s, %s, %lu>\n",
        counter++, scan->command, scan->arguments, scan->dirname, scan->time);
        // Passo alla richiesta successiva
        scan = scan->next;
    }
}

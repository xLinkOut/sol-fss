// @author Luca Cirillo (545480)
// * Costanti e macro condivise tra client e server

#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

// Flags per la openFile
#define O_READ 0
#define O_CREATE 1
#define O_LOCK 2
// Gestione di flags
#define IS_O_CREATE(mask) (mask & 1)
#define IS_O_LOCK(mask) ((mask >> 1) & 1)

// Lunghezza massima di un qualsiasi messaggio scambiato tra client e server
#define PIPE_LEN 16
#define CLIENT_LEFT 0
#define BUFFER_SIZE 1024
#define MEGABYTES 1048576  // 1024 * 1024
#define MESSAGE_LENGTH 2048
#define CONCURRENT_CONNECTIONS 8

typedef enum ReplacementPolicy {
    FIFO,
    LRU,
    LFU
} replacement_policy_t;

typedef enum RequestCode {
    OPEN,       // openFile
    READ,       // readFile
    READN,      // readNFiles
    WRITE,      // writeFile
    APPEND,     // appendToFile
    LOCK,       // lockFile
    UNLOCK,     // unlockFile
    CLOSE,      // closeFile
    REMOVE,     // removeFile
    DISCONNECT  // closeConnection
} request_code;

typedef enum ResponseCode {
    SUCCESS  // Operazione eseguita con successo
} response_code;

#endif

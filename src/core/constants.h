// @author Luca Cirillo (545480)
// Costanti e macro condivise tra client e server

// Gestione di flags
#define SET_FLAG(mask, flag) mask |= flag
#define RESET_MASK(mask) mask = 0

// Flags per la openFile
#define O_CREATE 1
#define O_LOCK 2
#define IS_O_CREATE(mask) (mask & 1)
#define IS_O_LOCK(mask) ((mask >> 1) & 1)

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
    SUCCESS // Operazione eseguita con successo
} response_code;
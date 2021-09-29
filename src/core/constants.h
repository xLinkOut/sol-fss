// @author Luca Cirillo (545480)

// Costanti condivise tra client e server

typedef enum APICode {
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
} api_code_t;

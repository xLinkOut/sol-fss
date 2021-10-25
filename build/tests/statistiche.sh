#!/bin/bash
# @author Luca Cirillo (545480)

# IFS= prevents leading/trailing whitespace from being trimmed
# -r prevents backslash escapes from being interpreted

BYTES_RE='^[0-9]+$'

READ_NO=0 # Numero di read
READ_BYTES=0 # Bytes letti durante operazioni di read

WRITE_NO=0 # Numero di write
WRITE_BYTES=0 # Bytes scritti durante operazioni di write

LOCK_NO=0 # Numero di lock
UNLOCK_NO=0 # Numero di unlock

OPENLOCK_NO=0 # Numero di operazioni di open con flag O_LOCK
CLOSE_NO=0 # Numero di close

STORAGE_SIZE=0 # Dimensione dello storage in ogni momento
MAX_STORAGE_SIZE=0 # Massima dimensione raggiunta dallo storage

STORAGE_FILE=0 # Numero di file all'interno dello storage
MAX_STORAGE_FILE=0 # Massimo numero di file all'interno dello storage

REPLACEMENT_NO=0 # Numero di esecuzioni dell'algoritmo di rimpiazzo

CLIENTS=0
MAX_CLIENTS=0

# Leggo line-by-line il file di log passato come argomento
while IFS= read -r line; do
    #echo "$line"
    # Parso la singola linea rispetto al separatore '|'
    IFS='|' read -a data <<< "$line"
    
    # data[0] -> Data e ora
    # data[1] -> Livello
    # data[2] -> Informazioni sull'operazione
    
    case "${data[2]}" in
        
        *OPEN:*) # openFile
            # Si tratta di una open-lock se il flag è pari a 2 (O_LOCK) oppre a 3 (O_CREATE | O_LOCK)
            
            IFS=' ' read -a result <<< "${data[2]}"
                # Considero solo operazioni andate a buon fine
                if [[ ${result[${#result[@]}-1]} == "O" ]] ; then
                    # Verifico la presenza del flag O_LOCK
                    if [[ ${result[${#result[@]}-3]} == 2 ]] || [[ ${result[${#result[@]}-3]} == 3 ]] ; then
                        ((OPENLOCK_NO+=1)) # Incremento il numero di open-lock
                    fi

                    # Aggiorno le informazioni sul numero di file nello storage
                    if [[ ${result[${#result[@]}-3]} == 1 ]] || [[ ${result[${#result[@]}-3]} == 3 ]] ; then
                        ((STORAGE_FILE+=1)) # Incremento il numero di file nello storage
                        # Se è stato raggiunto un nuovo picco, aggiorno il massimo
                        if [[ $STORAGE_FILE -gt $MAX_STORAGE_FILE ]] ; then
                            ((MAX_STORAGE_FILE=STORAGE_FILE))
                        fi
                    fi
                fi
            ;;

        *READ:*|*READN:*) # readFile, readNFiles
            
            IFS=' ' read -a bytes <<< "${data[2]}"
                # Incremento il numero di richieste servite dal thread
                #WORKERS[$(echo $S | cut -d "[" -f 2 | cut -d "]" -f 1)]+=1
                # Considero solo operazioni andate a buon fine
                if [[ ${bytes[${#bytes[@]}-1]} == "O" ]] ; then
                    ((READ_NO+=1)) # Incremento il numero di READ
                    # Incremento il numero di bytes letti
                    if [[ ${bytes[${#bytes[@]}-4]} =~ $BYTES_RE ]] ; then
                        ((READ_BYTES+=${bytes[${#bytes[@]}-4]}))
                    fi
                fi
            ;;
        
        *WRITE:*) # writeFile
            
            IFS=' ' read -a bytes <<< "${data[2]}"
                # Considero solo operazioni andate a buon fine
                if [[ ${bytes[${#bytes[@]}-1]} == "O" ]] ; then
                    ((WRITE_NO+=1)) # Incremento il numero di WRITE
                    if [[ ${bytes[${#bytes[@]}-4]} =~ $BYTES_RE ]] ; then
                        # Aggiorno il conteggio dei bytes scritti
                        ((WRITE_BYTES+=${bytes[${#bytes[@]}-4]}))
                        # Incremento la dimensione dello storage
                        ((STORAGE_SIZE+=${bytes[${#bytes[@]}-4]}))
                        # Se è stato raggiunto un nuovo picco, aggiorno il massimo
                        if [[ $STORAGE_SIZE -gt $MAX_STORAGE_SIZE ]] ; then
                            ((MAX_STORAGE_SIZE=STORAGE_SIZE))
                        fi
                    fi
                fi
            ;;
        
        *UNLOCK:*) # unlockFile
            
            IFS=' ' read -a result <<< "${data[2]}"
                # Considero solo operazioni andate a buon fine
                if [[ ${result[${#result[@]}-1]} == "O" ]] ; then
                    ((UNLOCK_NO+=1)) # Incremento il numero di UNLOCK
                fi
            ;;

        *LOCK:*) # lockFile
            
            IFS=' ' read -a result <<< "${data[2]}"
                # Considero solo operazioni andate a buon fine
                if [[ ${result[${#result[@]}-1]} == "O" ]] ; then
                    ((LOCK_NO+=1)) # Incremento il numero di LOCK
                fi
            ;;
        
        *REMOVE:*) # removeFile
            
            IFS=' ' read -a result <<< "${data[2]}"
                # Considero solo operazioni andate a buon fine
                if [[ ${result[${#result[@]}-1]} == "O" ]] ; then
                    ((STORAGE_FILE-=1)) # Decremento il numero di file nello storage
                fi
            ;;
        
        *CLOSE:*) # closeFile
            
            IFS=' ' read -a result <<< "${data[2]}"
                # Considero solo operazioni andate a buon fine
                if [[ ${result[${#result[@]}-1]} == "O" ]] ; then
                    ((CLOSE_NO+=1)) # Incremento il numero di CLOSE
                fi
            ;;

        *VICTIM:*) # Ejecting a file
            IFS=' ' read -a bytes <<< "${data[2]}"
            # Considero solo operazioni andate a buon fine
            if [[ ${bytes[${#bytes[@]}-1]} == "O" ]] ; then
                if [[ ${bytes[${#bytes[@]}-4]} =~ $BYTES_RE ]] ; then
                    # Decremento la dimensione dello storage
                    ((STORAGE_SIZE-=${bytes[${#bytes[@]}-4]}))
                    ((STORAGE_FILE-=1)) # Decremento il numero di file nello storage
                fi
            fi
            ;;

        *REPLACEMENT:*) # Replacement Algorithm
            
            # Incremento il numero di esecuzioni dell'algoritmo di rimpiazzo
            ((REPLACEMENT_NO+=1))
            ;;        
        
        *CLIENT:*) # Client connected or disconnected
            
            IFS=' ' read -a result <<< "${data[2]}"
                case "${result[${#result[@]}-1]}" in
                    
                    *connected*)
                        ((CLIENTS+=1))
                        if [[ $CLIENTS -gt $MAX_CLIENTS ]] ; then
                            ((MAX_CLIENTS=CLIENTS))
                        fi
                        ;;
                    
                    *left*)
                        ((CLIENTS-=1))
                        ;;
                esac
            ;;
    esac 

done < "$1"

echo "Number of reads: $READ_NO, average bytes read: $(($READ_BYTES / $READ_NO)) bytes"
echo "Number of writes: $WRITE_NO, average bytes written: $(($WRITE_BYTES / $WRITE_NO)) bytes"
echo "Number of locks: $LOCK_NO"
echo "Number of unlocks: $UNLOCK_NO"
echo "Number of open-locks: $OPENLOCK_NO"
echo "Number of closes: $CLOSE_NO"
echo "Maximum number of files reached by the storage: $MAX_STORAGE_FILE"
echo "Maximum size reached by the storage: $(($MAX_STORAGE_SIZE / 1024 / 1024)) MBytes"
echo "Number of runs of the replacement algorithm: $REPLACEMENT_NO"
echo "Maximum number of concurrent connections: $MAX_CLIENTS"
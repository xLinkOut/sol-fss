#!/bin/bash
# @author Luca Cirillo (545480)

# * TEST 1:
# *  Configurazione del server (config-1.txt): 10000 files, 128 MB, 1 Thread Worker
# *  Singole istanze del client testano tutte le APIs disponibili
# *  con un ritardo di 200 ms ed il flag -p attivo

BUILD_DIR=./build
TESTS_DIR=./build/tests
SOCKET_FILE=./build/fss.sk

CLIENT="$BUILD_DIR/client -f $SOCKET_FILE -p"
SERVER="$BUILD_DIR/server $TESTS_DIR/config-1.txt"

# Avvio in background il server con Valgrind
valgrind --leak-check=full $SERVER &
# Salvo il PID del processo, per poter successivamente inviare il segnale di arresto
SERVER_PID=$!
# Attendo che Valgrind stampi l'output sullo schermo
sleep 2s

# Testo tutte le operazioni implementate dal server
$CLIENT -h

# Interrompo l'esecuzione del server inviando il segnale di SIGHUP
kill -s SIGHUP $SERVER_PID

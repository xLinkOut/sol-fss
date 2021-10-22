#!/bin/bash
# @author Luca Cirillo (545480)

# * TEST 2:
# *  Configurazione del server (config-2.txt): 10 files, 1 MB, 4 Thread Worker
# *  Multiple istanze contemporanee di clients testano tutte le APIs disponibili
# *  con il flag -p attivo

KILOBYTE=1024
MEGABYTE=1048576

BUILD_DIR=./build
TESTS_DIR=$BUILD_DIR/tests
DUMMY_DIR=$TESTS_DIR/dummy
SAVES_DIR=$DUMMY_DIR/saves

SOCKET_FILE=./fss.sk
CLIENT="$BUILD_DIR/client -f $SOCKET_FILE -p"

# Genero alcuni dummy files per interagire sul server
# Creo la cartella che conterrà i dummy file
mkdir $DUMMY_DIR
# Creo la cartella in cui ospitare eventuali file espulsi/letti
mkdir $SAVES_DIR
# Genero 10 dummy file, da 100 a 1000 Kb
for i in {1..10}; do
    base64 /dev/urandom | head -c $(($i * $KILOBYTE * 100)) > $DUMMY_DIR/dummy-$i
done

# Testo tutte le operazioni implementate dal server, utilizzando più istanze client contemporaneamente
for i in {1..10}; do
    # -w: invio al server un'intera cartella, senza limite superiore al numero di file
    # TODO
    # -W: invio al server una lista di file specificati come argomento, e salvo eventuali file espulsi
    $CLIENT -W $DUMMY_DIR/dummy-1,$DUMMY_DIR/dummy-2 -D $SAVES_DIR &
    # -r: leggo dal server una lista di file
    $CLIENT -r $DUMMY_DIR/dummy-1,$DUMMY_DIR/dummy-3 &
    # -R: leggo dal server 5 file e li salvo 
    $CLIENT -R n=5 -d $SAVES_DIR &
    # -l: richiedo l'accesso in scrittura su due specifici file
    $CLIENT -l $DUMMY_DIR/dummy-1,$DUMMY_DIR/dummy-2 &
    # -d: rilascio l'accesso in scrittura su uno dei due file
    $CLIENT -u $DUMMY_DIR/dummy-2 &
    # -c: cancello il file su cui ho ancora l'accesso in scrittura
    $CLIENT -c $DUMMY_DIR/dummy-1 &
done

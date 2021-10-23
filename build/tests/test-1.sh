#!/bin/bash
# @author Luca Cirillo (545480)

# * TEST 1:
# *  Configurazione del server (config-1.txt): 10000 files, 128 MB, 1 Thread Worker
# *  Singole istanze del client testano tutte le APIs disponibili
# *  con un ritardo di 200 ms ed il flag -p attivo

KILOBYTE=1024
MEGABYTE=1048576 # 1024 * 1024

BUILD_DIR=./build
TESTS_DIR=$BUILD_DIR/tests
DUMMY_DIR=$TESTS_DIR/dummy
SAVES_DIR=$TESTS_DIR/saves

DELAY="-t 200"
SOCKET_FILE=$BUILD_DIR/fss.sk
CLIENT="$BUILD_DIR/client -f $SOCKET_FILE -p"

# Genero alcuni dummy files per interagire sul server
# Creo la cartella che conterrà i dummy file
mkdir -p $DUMMY_DIR
# Creo la cartella in cui ospitare eventuali file espulsi/letti
mkdir -p $SAVES_DIR
# Genero 5 dummy file, da 10 a 50 Mb
for i in {1..5}; do
    base64 /dev/urandom | head -c $((($i * 10) * $MEGABYTE)) > $DUMMY_DIR/dummy-$i
done

# Testo tutte le operazioni implementate dal server
# -h: stampo il messaggio di help
$CLIENT -h
# -w: invio al server un'intera cartella, senza limite superiore al numero di file
$CLIENT -w $DUMMY_DIR $DELAY
# -r: leggo dal server una lista di file
$CLIENT -r $DUMMY_DIR/dummy-1,$DUMMY_DIR/dummy-2 $DELAY
# -l: richiedo l'accesso in scrittura su due specifici file, ne cancello uno e rilascio l'altro
$CLIENT -l $DUMMY_DIR/dummy-1,$DUMMY_DIR/dummy-2 $DELAY -c $DUMMY_DIR/dummy-1 $DELAY -u $DUMMY_DIR/dummy-2
# -W: invio al server una lista di file specificati come argomento, e salvo eventuali file espulsi
$CLIENT -W $DUMMY_DIR/dummy-1 -D $SAVES_DIR $DELAY
# -R: leggo dal server tutti i file e li salvo 
$CLIENT -R n=0 -d $SAVES_DIR $DELAY

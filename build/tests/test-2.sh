#!/bin/bash
# @author Luca Cirillo (545480)

# * TEST 2:
# *  Configurazione del server (config-2.txt): 10 files, 1 MB, 4 Thread Worker
# *  Multiple istanze contemporanee di clients testano tutte le APIs disponibili
# *  con il flag -p attivo

KILOBYTE=1024
MEGABYTE=1048576 # 1024 * 1024

BUILD_DIR=./build
TESTS_DIR=$BUILD_DIR/tests
DUMMY_DIR=$TESTS_DIR/dummy
SAVES_DIR=$TESTS_DIR/saves

SOCKET_FILE=$BUILD_DIR/fss.sk
CLIENT="$BUILD_DIR/client -f $SOCKET_FILE -p"

# Genero alcuni dummy files per interagire sul server
# Creo la cartella che conterrà i dummy file
mkdir -p $DUMMY_DIR
# Creo la cartella in cui ospitare eventuali file espulsi/letti
mkdir -p $SAVES_DIR
# Genero 10 dummy file, da 100 a 1000 Kb
echo "Generating dummy files, please wait..."
for i in {1..10}; do
    base64 /dev/urandom | head -c $((($i * 100) * $KILOBYTE)) > $DUMMY_DIR/dummy-$i
done

# Per testare l'algoritmo di rimpiazzo, faccio molte scritture simultaneamente
for i in {1..20}; do
    # -W: invio al server una lista di file specificati come argomento, e salvo eventuali file espulsi
    $CLIENT -W $DUMMY_DIR/dummy-$((1 + $RANDOM % 10)),$DUMMY_DIR/dummy-$((1 + $RANDOM % 10)),$DUMMY_DIR/dummy-$((1 + $RANDOM % 10)) -D $SAVES_DIR &
    pids[${i}]=$!
done

# Aspetto che tutti i client abbiano finito prima di uscire
for pid in ${pids[*]}; do
    wait $pid
done

#!/bin/bash
# @author Luca Cirillo (545480)

# * TEST 3:
# *  Configurazione del server (config-3.txt): 100 files, 32 MB, 8 Thread Worker
# *  Multiple istanze contemporanee di clients testano tutte le APIs disponibili
# *  con il flag -p disabilitato

KILOBYTE=1024
MEGABYTE=1048576 # 1024 * 1024

BUILD_DIR=./build
TESTS_DIR=$BUILD_DIR/tests
DUMMY_DIR=$TESTS_DIR/dummy
SAVES_DIR=$TESTS_DIR/saves

SOCKET_FILE=$BUILD_DIR/fss.sk
CLIENT="$BUILD_DIR/client -f $SOCKET_FILE"

# Genero alcuni dummy files per interagire sul server
# Creo la cartella che conterrà i dummy file
mkdir -p $DUMMY_DIR
# Creo la cartella in cui ospitare eventuali file espulsi/letti
mkdir -p $SAVES_DIR
# Genero 100 dummy file, da 20 KB a 2 MB
echo "Generating dummy files, please wait..."
for i in {1..100}; do
    base64 /dev/urandom | head -c $((($i * 20) * $KILOBYTE)) > $DUMMY_DIR/dummy-$i
done

# Lancio nuovi client per 30 secondi
end=$((SECONDS + 30))

# Testo tutte le operazioni implementate dal server, utilizzando più istanze client contemporaneamente
while [ $SECONDS -lt $end ]; do
    # Genero sette numeri casuali, che identificheranno, ad ogni iterazione, dieci file differenti
    for i in {0..7}; do
        index[${i}]=$((1 + $RANDOM % 100))
    done;

    # Scrivo, leggo, blocco, cancello e sblocco alcuni file
    $CLIENT -W $DUMMY_DIR/dummy-${index[0]},$DUMMY_DIR/dummy-${index[1]},$DUMMY_DIR/dummy-${index[2]},$DUMMY_DIR/dummy-${index[3]} -D $SAVES_DIR \
            -r $DUMMY_DIR/dummy-${index[0]},$DUMMY_DIR/dummy-${index[1]} -d $SAVES_DIR \
            -l $DUMMY_DIR/dummy-${index[0]},$DUMMY_DIR/dummy-${index[2]} \
            -c $DUMMY_DIR/dummy-${index[0]} \
            -u $DUMMY_DIR/dummy-${index[2]} &

    # Scrivo, blocco, sovrascrivo, cancello e leggo alcuni file
    $CLIENT -W $DUMMY_DIR/dummy-${index[4]},$DUMMY_DIR/dummy-${index[5]},$DUMMY_DIR/dummy-${index[6]},$DUMMY_DIR/dummy-${index[7]} \
            -l $DUMMY_DIR/dummy-${index[4]} \
            -w $DUMMY_DIR/dummy-${index[4]} \
            -c $DUMMY_DIR/dummy-${index[4]} \
            -R n=$((1 + $RANDOM % 10)) -d $SAVES_DIR &

    sleep 0.1
done

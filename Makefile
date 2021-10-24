#!/bin/bash
# @author Luca Cirillo (545480)

CC = gcc
LIBS = -pthread
CFLAGS = -g -Wall -std=c99 -D_POSIX_C_SOURCE=200112L

BUILD_DIR  = ./build
CORE_DIR   = ./src/core
CLIENT_DIR = ./src/client
SERVER_DIR = ./src/server
TESTS_DIR  = ./build/tests

CORE_INCLUDES   = -I ./src/core/includes
CLIENT_INCLUDES = -I ./src/client/includes
SERVER_INCLUDES = -I ./src/server/includes

SERVER_TARGETS = server.o utils.o rwlock.o linkedlist.o queue.o icl_hash.o storage.o
CLIENT_TARGETS = client.o linkedlist.o utils.o API.o request_queue.o

SERVER_OBJS = \
	$(BUILD_DIR)/linkedlist.o $(BUILD_DIR)/rwlock.o $(BUILD_DIR)/utils.o \
	$(BUILD_DIR)/queue.o $(BUILD_DIR)/icl_hash.o $(BUILD_DIR)/storage.o \
	$(BUILD_DIR)/server.o

CLIENT_OBJS = \
	$(BUILD_DIR)/linkedlist.o $(BUILD_DIR)/utils.o \
	$(BUILD_DIR)/API.o $(BUILD_DIR)/request_queue.o \
	$(BUILD_DIR)/client.o

.PHONY: all server client clean cleanall test1 test2 test3

all: server client
	@cp ./config/config-example.txt $(BUILD_DIR)/config.txt

server: $(SERVER_TARGETS)
	$(CC) $(CFLAGS) $(LIBS) $(SERVER_OBJS) -o $(BUILD_DIR)/server 

client: $(CLIENT_TARGETS)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o $(BUILD_DIR)/client

# == SERVER
icl_hash.o: utils.o
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(SERVER_INCLUDES) -c $(SERVER_DIR)/icl_hash.c -o $(BUILD_DIR)/$@

queue.o: utils.o
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(SERVER_INCLUDES) -c $(SERVER_DIR)/queue.c -o $(BUILD_DIR)/$@

storage.o: utils.o rwlock.o icl_hash.o
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(SERVER_INCLUDES) -c $(SERVER_DIR)/storage.c -o $(BUILD_DIR)/$@

server.o: storage.o icl_hash.o queue.o
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(SERVER_INCLUDES) -c $(SERVER_DIR)/server.c -o $(BUILD_DIR)/$@

# == CLIENT
API.o:
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(CLIENT_INCLUDES) -c $(CLIENT_DIR)/API.c -o $(BUILD_DIR)/$@

request_queue.o:
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(CLIENT_INCLUDES) -c $(CLIENT_DIR)/request_queue.c -o $(BUILD_DIR)/$@

client.o: API.o request_queue.o linkedlist.o utils.o
	$(CC) $(CFLAGS) $(CORE_INCLUDES) $(CLIENT_INCLUDES) -c $(CLIENT_DIR)/client.c -o $(BUILD_DIR)/$@

# == CORE
linkedlist.o:
	$(CC) $(CFLAGS) $(CORE_INCLUDES) -c $(CORE_DIR)/linkedlist.c -o $(BUILD_DIR)/$@

rwlock.o:
	$(CC) $(CFLAGS) $(CORE_INCLUDES) -c $(CORE_DIR)/rwlock.c -o $(BUILD_DIR)/$@

utils.o:
	$(CC) $(CFLAGS) $(CORE_INCLUDES) -c $(CORE_DIR)/utils.c -o $(BUILD_DIR)/$@

# == CLEAN

# Objects, Socket, Logs
clean:
	rm -f $(BUILD_DIR)/*.o
	rm -f $(BUILD_DIR)/*.sk
	rm -f $(BUILD_DIR)/*.log

# Objects, Socket, Logs, Dummy, Saves
cleanall: clean
	rm -rf $(TESTS_DIR)/dummy
	rm -rf $(TESTS_DIR)/saves

# == TESTS

test1: client server
	valgrind --leak-check=full $(BUILD_DIR)/server $(TESTS_DIR)/config-1.txt &
	@chmod +x $(TESTS_DIR)/test-1.sh
	$(TESTS_DIR)/test-1.sh
	pkill -HUP -f $(BUILD_DIR)/server

test2: client server
	$(BUILD_DIR)/server $(TESTS_DIR)/config-2.txt &
	@chmod +x $(TESTS_DIR)/test-2.sh
	$(TESTS_DIR)/test-2.sh
	pkill -HUP -f $(BUILD_DIR)/server

test3: client server
	$(BUILD_DIR)/server $(TESTS_DIR)/config-3.txt &
	@chmod +x $(TESTS_DIR)/test-3.sh
	$(TESTS_DIR)/test-3.sh
	pkill -INT -f $(BUILD_DIR)/server

CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
TMP_DIR = tmp

RUNNER_SOURCES = $(SRC_DIR)/runner.c $(SRC_DIR)/ipc.c $(SRC_DIR)/parser.c $(SRC_DIR)/executor.c
CONTROLLER_SOURCES = $(SRC_DIR)/controller.c $(SRC_DIR)/ipc.c $(SRC_DIR)/scheduler.c

RUNNER_OBJECTS = $(OBJ_DIR)/runner.o $(OBJ_DIR)/ipc_runner.o $(OBJ_DIR)/parser.o $(OBJ_DIR)/executor.o
CONTROLLER_OBJECTS = $(OBJ_DIR)/controller.o $(OBJ_DIR)/ipc_controller.o $(OBJ_DIR)/scheduler.o

RUNNER_BIN = $(BIN_DIR)/runner
CONTROLLER_BIN = $(BIN_DIR)/controller

.PHONY: all runner controller clean

all: controller runner

controller: $(CONTROLLER_BIN)

runner: $(RUNNER_BIN)

$(BIN_DIR) $(OBJ_DIR) $(TMP_DIR):
	mkdir -p $@

$(OBJ_DIR)/runner.o: $(SRC_DIR)/runner.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ipc_runner.o: $(SRC_DIR)/ipc.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/controller.o: $(SRC_DIR)/controller.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ipc_controller.o: $(SRC_DIR)/ipc.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/parser.o: $(SRC_DIR)/parser.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/executor.o: $(SRC_DIR)/executor.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/scheduler.o: $(SRC_DIR)/scheduler.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(RUNNER_BIN): $(RUNNER_OBJECTS) | $(BIN_DIR) $(TMP_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(CONTROLLER_BIN): $(CONTROLLER_OBJECTS) | $(BIN_DIR) $(TMP_DIR)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) log.txt
	
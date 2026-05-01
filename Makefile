CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude -Iinclude/runner -Iinclude/controller -Iinclude/common

SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
TMP_DIR = tmp

RUNNER_SOURCES = \
	$(SRC_DIR)/runner/runner.c \
	$(SRC_DIR)/runner/runner_cli.c \
	$(SRC_DIR)/runner/parser.c \
	$(SRC_DIR)/runner/executor.c \
	$(SRC_DIR)/common/ipc.c

CONTROLLER_SOURCES = \
	$(SRC_DIR)/controller/controller.c \
	$(SRC_DIR)/controller/controller_handlers.c \
	$(SRC_DIR)/controller/scheduler.c \
	$(SRC_DIR)/common/ipc.c

RUNNER_OBJECTS = $(RUNNER_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
CONTROLLER_OBJECTS = $(CONTROLLER_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

RUNNER_BIN = $(BIN_DIR)/runner
CONTROLLER_BIN = $(BIN_DIR)/controller

.PHONY: all runner controller clean

all: controller runner

controller: $(CONTROLLER_BIN)

runner: $(RUNNER_BIN)

$(BIN_DIR) $(OBJ_DIR) $(TMP_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(RUNNER_BIN): $(RUNNER_OBJECTS) | $(BIN_DIR) $(TMP_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(CONTROLLER_BIN): $(CONTROLLER_OBJECTS) | $(BIN_DIR) $(TMP_DIR)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) log.txt $(TMP_DIR)/*

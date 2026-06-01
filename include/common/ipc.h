#ifndef IPC_H
#define IPC_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "protocol.h"

/**
 * @brief Create a named pipe if it does not already exist.
 *
 * @param fifo_path Filesystem path where the FIFO should exist.
 * @param permissions Permission bits passed to mkfifo().
 * @return 0 on success, including when the FIFO already exists; -1 on error.
 */
int ipc_create_fifo(const char *fifo_path, mode_t permissions);

/**
 * @brief Remove a named pipe from the filesystem.
 *
 * @param fifo_path Path of the FIFO to remove.
 * @return Result returned by unlink(): 0 on success, -1 on error.
 */
int ipc_destroy_fifo(const char *fifo_path);

/**
 * @brief Send one complete RpcMessage through a FIFO.
 *
 * The function opens the FIFO, writes sizeof(RpcMessage) bytes in a single
 * write() call, and closes the descriptor. The single write is important for
 * preserving message atomicity when the message size is within PIPE_BUF.
 *
 * @param fifo_path FIFO path to open in write-only mode.
 * @param msg Message to send.
 * @return Number of bytes written on success, or -1 on error.
 */
ssize_t ipc_send_atomic(const char *fifo_path, const RpcMessage *msg);

/**
 * @brief Read one fixed-size RpcMessage from an already open FIFO.
 *
 * @param fifo_fd File descriptor of a FIFO opened for reading.
 * @param msg Destination buffer for the received message.
 * @return Number of bytes read, 0 on EOF, or -1 on error.
 */
ssize_t ipc_read_blocking(int fifo_fd, RpcMessage *msg);

#endif /* IPC_H */

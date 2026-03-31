#ifndef IPC_H
#define IPC_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "protocol.h"

int ipc_create_fifo(const char *fifo_path, mode_t permissions);
int ipc_destroy_fifo(const char *fifo_path);
ssize_t ipc_send_atomic(const char *fifo_path, const RpcMessage *msg);
ssize_t ipc_read_blocking(int fifo_fd, RpcMessage *msg);

#endif /* IPC_H */

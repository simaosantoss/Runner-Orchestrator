#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int ipc_create_fifo(const char *fifo_path, mode_t permissions) {
	if (mkfifo(fifo_path, permissions) == -1) {
		if (errno == EEXIST) {
			return 0;
		}
		return -1;
	}
	return 0;
}

int ipc_destroy_fifo(const char *fifo_path) {
	return unlink(fifo_path);
}

ssize_t ipc_send_atomic(const char *fifo_path, const RpcMessage *msg) {
	int fd = open(fifo_path, O_WRONLY);
	ssize_t written;

	if (fd == -1) {
		return -1;
	}

	written = write(fd, msg, sizeof(RpcMessage));

	if (close(fd) == -1 && written != -1) {
		return -1;
	}

	return written;
}

ssize_t ipc_read_blocking(int fifo_fd, RpcMessage *msg) {
	return read(fifo_fd, msg, sizeof(RpcMessage));
}

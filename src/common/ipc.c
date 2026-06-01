#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * @brief Create a FIFO and treat an already existing FIFO as success.
 *
 * @param fifo_path Path where the named pipe should be created.
 * @param permissions Permission bits passed to mkfifo().
 * @return 0 on success, -1 on errors other than EEXIST.
 */
int ipc_create_fifo(const char *fifo_path, mode_t permissions) {
	if (mkfifo(fifo_path, permissions) == -1) {
		if (errno == EEXIST) {
			return 0;
		}
		return -1;
	}
	return 0;
}

/**
 * @brief Remove a FIFO path from the filesystem.
 *
 * @param fifo_path Path of the FIFO to remove.
 * @return 0 on success, -1 on unlink() failure.
 */
int ipc_destroy_fifo(const char *fifo_path) {
	return unlink(fifo_path);
}

/**
 * @brief Open a FIFO, send one complete RpcMessage, and close it.
 *
 * @param fifo_path FIFO path to open for writing.
 * @param msg Message to send in a single write() call.
 * @return Number of bytes written, or -1 on open/write/close error.
 */
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

/**
 * @brief Read one RpcMessage from an open FIFO descriptor.
 *
 * @param fifo_fd File descriptor opened for reading.
 * @param msg Destination message buffer.
 * @return Number of bytes read, 0 on EOF, or -1 on read() error.
 */
ssize_t ipc_read_blocking(int fifo_fd, RpcMessage *msg) {
	return read(fifo_fd, msg, sizeof(RpcMessage));
}

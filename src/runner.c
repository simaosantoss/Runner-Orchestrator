#include "ipc.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	char my_fifo[128];
	int my_fd;
	RpcMessage submit_msg;
	RpcMessage ack_msg;

	snprintf(my_fifo, sizeof(my_fifo), "/tmp/runner_%d", getpid());

	if (ipc_create_fifo(my_fifo, 0666) == -1) {
		perror("ipc_create_fifo");
		return 1;
	}

	memset(&submit_msg, 0, sizeof(submit_msg));
	submit_msg.type = SUBMIT;
	submit_msg.sender_pid = getpid();
	submit_msg.user_id = 1;

	if (ipc_send_atomic(SERVER_FIFO_PATH, &submit_msg) == -1) {
		perror("ipc_send_atomic");
		ipc_destroy_fifo(my_fifo);
		return 1;
	}

	my_fd = open(my_fifo, O_RDONLY);
	if (my_fd == -1) {
		perror("open runner fifo");
		ipc_destroy_fifo(my_fifo);
		return 1;
	}

	if (ipc_read_blocking(my_fd, &ack_msg) == (ssize_t)sizeof(RpcMessage) && ack_msg.type == ACK) {
		printf("[runner] command %ld submitted\n", ack_msg.command_id);
	}

	close(my_fd);
	ipc_destroy_fifo(my_fifo);
	return 0;
}

#include "ipc.h"

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	char my_fifo[128];
	int my_fd;
	int user_id;
	size_t payload_len;
	RpcMessage submit_msg;
	RpcMessage ack_msg;

	if (argc < 3 || strcmp(argv[1], "-e") != 0) {
		fprintf(stderr, "Usage: ./runner -e <user_id> command...\n");
		return 1;
	}

	user_id = atoi(argv[2]);

	snprintf(my_fifo, sizeof(my_fifo), "/tmp/runner_%d", getpid());

	if (ipc_create_fifo(my_fifo, 0666) == -1) {
		perror("ipc_create_fifo");
		return 1;
	}

	memset(&submit_msg, 0, sizeof(submit_msg));
	submit_msg.type = SUBMIT;
	submit_msg.sender_pid = getpid();
	submit_msg.user_id = user_id;

	payload_len = 0;
	for (int i = 3; i < argc; i++) {
		size_t arg_len = strlen(argv[i]);
		size_t sep_len = (payload_len > 0) ? 1 : 0;

		if (payload_len + sep_len >= sizeof(submit_msg.payload)) {
			break;
		}

		if (sep_len == 1) {
			submit_msg.payload[payload_len] = ' ';
			payload_len++;
		}

		if (arg_len > (sizeof(submit_msg.payload) - 1 - payload_len)) {
			arg_len = sizeof(submit_msg.payload) - 1 - payload_len;
		}

		memcpy(submit_msg.payload + payload_len, argv[i], arg_len);
		payload_len += arg_len;
		submit_msg.payload[payload_len] = '\0';
	}

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

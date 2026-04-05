#include "ipc.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
	int server_fd;
	RpcMessage msg;
	long command_counter = 1;

	if (ipc_create_fifo(SERVER_FIFO_PATH, 0666) == -1) {
		perror("ipc_create_fifo");
		return 1;
	}

	while (1) {
		server_fd = open(SERVER_FIFO_PATH, O_RDONLY);
		if (server_fd == -1) {
			perror("open server fifo");
			return 1;
		}

		while (ipc_read_blocking(server_fd, &msg) == (ssize_t)sizeof(RpcMessage)) {
			if (msg.type == SUBMIT) {
				char resp_fifo[128];
				RpcMessage resp;

				snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", msg.sender_pid);
				memset(&resp, 0, sizeof(resp));
				resp.type = ACK;
				resp.sender_pid = getpid();
				resp.user_id = msg.user_id;
				resp.command_id = command_counter;
				command_counter++;

				if (ipc_send_atomic(resp_fifo, &resp) == -1) {
					perror("ipc_send_atomic");
				}
			}

			if (msg.type == DONE) {
				printf("O comando %ld do utilizador %d terminou.\n", msg.command_id, msg.user_id);
			}
		}

		close(server_fd);
	}

	return 0;
}

#include "controller_handlers.h"
#include "ipc.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	controller_state_t state;
	int server_fd;
	RpcMessage msg;

	srand(time(NULL));
	controller_configure(&state, argc, argv);

	if (controller_init_queues(&state) == -1) {
		fprintf(stderr, "failed to create scheduler queues\n");
		controller_destroy_queues(&state);
		return 1;
	}

	if (ipc_create_fifo(SERVER_FIFO_PATH, 0666) == -1) {
		perror("ipc_create_fifo");
		controller_destroy_queues(&state);
		return 1;
	}

	while (state.keep_running) {
		server_fd = open(SERVER_FIFO_PATH, O_RDONLY);
		if (server_fd == -1) {
			perror("open server fifo");
			controller_destroy_queues(&state);
			return 1;
		}

		while (ipc_read_blocking(server_fd, &msg) == (ssize_t)sizeof(RpcMessage)) {
			if (controller_handle_message(&state, &msg)) {
				break;
			}
		}

		close(server_fd);
	}

	controller_destroy_queues(&state);
	ipc_destroy_fifo(SERVER_FIFO_PATH);
	return 0;
}

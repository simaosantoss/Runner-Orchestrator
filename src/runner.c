#include "ipc.h"
#include "parser.h"
#include "executor.h"

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
	RpcMessage msg_done;
	RpcMessage status_req;
	RpcMessage status_msg;

	if (argc == 2 && strcmp(argv[1], "-c") == 0) {
		snprintf(my_fifo, sizeof(my_fifo), "/tmp/runner_%d", getpid());

		if (ipc_create_fifo(my_fifo, 0666) == -1) {
			perror("ipc_create_fifo");
			return 1;
		}

		memset(&status_req, 0, sizeof(status_req));
		status_req.type = STATUS_REQ;
		status_req.sender_pid = getpid();

		if (ipc_send_atomic(SERVER_FIFO_PATH, &status_req) == -1) {
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

		while (ipc_read_blocking(my_fd, &status_msg) == (ssize_t)sizeof(RpcMessage)) {

			if (status_msg.type == STATUS_RESP) {
				printf("%s", status_msg.payload);
			}

			if (status_msg.type == STATUS_END) {
				break;
			}
		}

		close(my_fd);
		ipc_destroy_fifo(my_fifo);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "-s") == 0) {
		RpcMessage shutdown_req;

		snprintf(my_fifo, sizeof(my_fifo), "/tmp/runner_%d", getpid());

		if (ipc_create_fifo(my_fifo, 0666) == -1) {
			perror("ipc_create_fifo");
			return 1;
		}

		memset(&shutdown_req, 0, sizeof(shutdown_req));
		shutdown_req.type = SHUTDOWN_REQ;
		shutdown_req.sender_pid = getpid();

		if (ipc_send_atomic(SERVER_FIFO_PATH, &shutdown_req) == -1) {
			perror("ipc_send_atomic");
			ipc_destroy_fifo(my_fifo);
			return 1;
		}

		printf("[runner] sent shutdown notification\n");
		printf("[runner] waiting for controller to shutdown...\n");

		my_fd = open(my_fifo, O_RDONLY);
		if (my_fd == -1) {
			perror("open runner fifo");
			ipc_destroy_fifo(my_fifo);
			return 1;
		}

		while (ipc_read_blocking(my_fd, &status_msg) == (ssize_t)sizeof(RpcMessage)) {
			if (status_msg.type == SHUTDOWN_ACK) {
				break;
			}
		}

		printf("[runner] controller exited.\n");

		close(my_fd);
		ipc_destroy_fifo(my_fifo);
		return 0;
	}

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
		parsed_command_t *cmd;

		printf("[runner] executing command %ld...\n", ack_msg.command_id);

		cmd = parser_parse(submit_msg.payload);
		if (cmd == NULL) {
			fprintf(stderr, "parser_parse failed\n");
		} else {
			execute_pipeline(cmd);
			parser_destroy(cmd);
		}

		memset(&msg_done, 0, sizeof(msg_done));
		msg_done.type = DONE;
		msg_done.sender_pid = getpid();
		msg_done.user_id = ack_msg.user_id;
		msg_done.command_id = ack_msg.command_id;

		if (ipc_send_atomic(SERVER_FIFO_PATH, &msg_done) == -1) {
			perror("ipc_send_atomic");
			close(my_fd);
			ipc_destroy_fifo(my_fifo);
			return 1;
		}

		printf("[runner] command %ld finished\n", ack_msg.command_id);
	} else {
		fprintf(stderr, "runner did not receive ACK\n");
		close(my_fd);
		ipc_destroy_fifo(my_fifo);
		return 1;
	}

	close(my_fd);
	ipc_destroy_fifo(my_fifo);
	return 0;
}

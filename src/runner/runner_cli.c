#include "runner_cli.h"

#include "executor.h"
#include "ipc.h"
#include "parser.h"

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief Build the private FIFO path for the current runner process.
 *
 * @param buffer Destination buffer.
 * @param buffer_size Size of the destination buffer.
 */
static void runner_fifo_path(char *buffer, size_t buffer_size) {
	snprintf(buffer, buffer_size, "/tmp/runner_%d", getpid());
}

/**
 * @brief Create the private FIFO used by this runner to receive controller replies.
 *
 * @param my_fifo Buffer that receives the FIFO path.
 * @param fifo_size Size of my_fifo.
 * @return 0 on success, -1 on FIFO creation failure.
 */
static int create_runner_fifo(char *my_fifo, size_t fifo_size) {
	runner_fifo_path(my_fifo, fifo_size);

	if (ipc_create_fifo(my_fifo, 0666) == -1) {
		perror("ipc_create_fifo");
		return -1;
	}

	return 0;
}

/**
 * @brief Parse the runner command-line interface.
 *
 * @param argc Argument count passed to runner main().
 * @param argv Argument vector passed to runner main().
 * @param request Output structure filled with mode, user id and command.
 * @return 0 on valid usage, non-zero otherwise.
 */
int runner_parse_args(int argc, char *argv[], runner_request_t *request) {
	if (request == NULL) {
		printf("Usage: ./runner -e <user_id> \"<command...>\"\n");
		return 1;
	}

	memset(request, 0, sizeof(*request));

	if (argc == 2 && strcmp(argv[1], "-c") == 0) {
		request->mode = RUNNER_MODE_STATUS;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "-s") == 0) {
		request->mode = RUNNER_MODE_SHUTDOWN;
		return 0;
	}

	if (argc != 4 || strcmp(argv[1], "-e") != 0) {
		printf("Usage: ./runner -e <user_id> \"<command...>\"\n");
		return 1;
	}

	request->mode = RUNNER_MODE_EXECUTE;
	request->user_id = atoi(argv[2]);
	request->command = argv[3];
	return 0;
}

/**
 * @brief Execute runner -c by asking the controller for current state.
 *
 * @return 0 on success, non-zero on FIFO/IPC failure.
 */
int runner_run_status(void) {
	char my_fifo[128];
	int my_fd;
	RpcMessage status_req;
	RpcMessage status_msg;

	if (create_runner_fifo(my_fifo, sizeof(my_fifo)) == -1) {
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

/**
 * @brief Execute runner -s by requesting controlled controller shutdown.
 *
 * @return 0 on success, non-zero on FIFO/IPC failure.
 */
int runner_run_shutdown(void) {
	char my_fifo[128];
	int my_fd;
	RpcMessage shutdown_req;
	RpcMessage status_msg;

	if (create_runner_fifo(my_fifo, sizeof(my_fifo)) == -1) {
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

/**
 * @brief Execute runner -e by submitting and running a command.
 *
 * The runner first sends SUBMIT, waits for ACK, parses and executes the
 * command locally, and finally sends DONE back to the controller.
 *
 * @param user_id User identifier to send with the command.
 * @param command Command string to execute after authorization.
 * @return 0 on success, non-zero on communication or execution setup failure.
 */
int runner_run_execute(int user_id, const char *command) {
	char my_fifo[128];
	int my_fd;
	RpcMessage submit_msg;
	RpcMessage ack_msg;
	RpcMessage msg_done;

	if (create_runner_fifo(my_fifo, sizeof(my_fifo)) == -1) {
		return 1;
	}

	memset(&submit_msg, 0, sizeof(submit_msg));
	submit_msg.type = SUBMIT;
	submit_msg.sender_pid = getpid();
	submit_msg.user_id = user_id;
	submit_msg.command_id = getpid();
	strncpy(submit_msg.payload, command, sizeof(submit_msg.payload) - 1);
	submit_msg.payload[sizeof(submit_msg.payload) - 1] = '\0';

	printf("[runner] command %ld submitted\n", (long)submit_msg.command_id);

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
			printf("parser_parse failed\n");
		} else {
			fflush(stdout);
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
		printf("runner did not receive ACK\n");
		close(my_fd);
		ipc_destroy_fifo(my_fifo);
		return 1;
	}

	close(my_fd);
	ipc_destroy_fifo(my_fifo);
	return 0;
}

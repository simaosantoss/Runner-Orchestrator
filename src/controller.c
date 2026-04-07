#include "ipc.h"
#include "scheduler.h"

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	int server_fd;
	int parallel_limit = 4;
	int running_count = 0;
	RpcMessage msg;
	job_info_t dummy;
	job_queue_t *waiting_queue;
	job_queue_t *running_queue;
	long command_counter = 1;

	if (argc > 1) {
		int parsed_limit = atoi(argv[1]);
		if (parsed_limit > 0) {
			parallel_limit = parsed_limit;
		}
	}

	waiting_queue = queue_create();
	running_queue = queue_create();
	if (waiting_queue == NULL || running_queue == NULL) {
		fprintf(stderr, "failed to create scheduler queues\n");
		queue_destroy(waiting_queue);
		queue_destroy(running_queue);
		return 1;
	}

	if (ipc_create_fifo(SERVER_FIFO_PATH, 0666) == -1) {
		perror("ipc_create_fifo");
		queue_destroy(waiting_queue);
		queue_destroy(running_queue);
		return 1;
	}

	while (1) {
		server_fd = open(SERVER_FIFO_PATH, O_RDONLY);
		if (server_fd == -1) {
			perror("open server fifo");
			queue_destroy(waiting_queue);
			queue_destroy(running_queue);
			return 1;
		}

		while (ipc_read_blocking(server_fd, &msg) == (ssize_t)sizeof(RpcMessage)) {
			if (msg.type == SUBMIT) {
				job_info_t job;

				memset(&job, 0, sizeof(job));
				job.command_id = command_counter;
				job.user_id = msg.user_id;
				job.runner_pid = msg.sender_pid;
				command_counter++;

				if (running_count < parallel_limit) {
					char resp_fifo[128];
					RpcMessage resp;

					job.state = JOB_RUNNING;
					if (!queue_enqueue(running_queue, &job)) {
						fprintf(stderr, "failed to enqueue running job\n");
						continue;
					}
					running_count++;

					snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", job.runner_pid);
					memset(&resp, 0, sizeof(resp));
					resp.type = ACK;
					resp.sender_pid = getpid();
					resp.user_id = job.user_id;
					resp.command_id = job.command_id;

					if (ipc_send_atomic(resp_fifo, &resp) == -1) {
						perror("ipc_send_atomic");
					}
				} else {
					job.state = JOB_QUEUED;
					if (!queue_enqueue(waiting_queue, &job)) {
						fprintf(stderr, "failed to enqueue waiting job\n");
					}
				}
			}

			if (msg.type == DONE) {
				if (queue_remove_by_command_id(running_queue, msg.command_id, &dummy)) {
					if (running_count > 0) {
						running_count--;
					}
				}

				printf("O comando %ld do utilizador %d terminou.\n", msg.command_id, msg.user_id);

				while (running_count < parallel_limit && !queue_is_empty(waiting_queue)) {
					char resp_fifo[128];
					RpcMessage resp;
					job_info_t next_job;

					if (!queue_dequeue(waiting_queue, &next_job)) {
						break;
					}

					next_job.state = JOB_RUNNING;
					if (!queue_enqueue(running_queue, &next_job)) {
						fprintf(stderr, "failed to move job to running queue\n");
						continue;
					}
					running_count++;

					snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", next_job.runner_pid);
					memset(&resp, 0, sizeof(resp));
					resp.type = ACK;
					resp.sender_pid = getpid();
					resp.user_id = next_job.user_id;
					resp.command_id = next_job.command_id;

					if (ipc_send_atomic(resp_fifo, &resp) == -1) {
						perror("ipc_send_atomic");
					}
				}
			}
		}

		close(server_fd);
	}

	return 0;
}

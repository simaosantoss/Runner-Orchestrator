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
	int keep_running = 1;
	int is_shutting_down = 0;
	pid_t shutdown_requester_pid = -1;
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

	while (keep_running) {
		server_fd = open(SERVER_FIFO_PATH, O_RDONLY);
		if (server_fd == -1) {
			perror("open server fifo");
			queue_destroy(waiting_queue);
			queue_destroy(running_queue);
			return 1;
		}

		while (ipc_read_blocking(server_fd, &msg) == (ssize_t)sizeof(RpcMessage)) {
			if (msg.type == SUBMIT && is_shutting_down) {
				continue;
			}

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

			if (msg.type == STATUS_REQ) {
				char resp_fifo[128];
				int fd_resp;
				RpcMessage status_resp;
				RpcMessage status_end;
				job_info_t run_arr[100];
				job_info_t wait_arr[100];
				int run_count;
				int wait_count;

				snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", msg.sender_pid);

				run_count = queue_copy_to_array(running_queue, run_arr, 100);
				wait_count = queue_copy_to_array(waiting_queue, wait_arr, 100);
				fd_resp = open(resp_fifo, O_WRONLY);
				if (fd_resp == -1) {
					perror("open response fifo");
					continue;
				}

				memset(&status_resp, 0, sizeof(status_resp));
				status_resp.type = STATUS_RESP;
				status_resp.sender_pid = getpid();

				snprintf(status_resp.payload, sizeof(status_resp.payload), "---Executing\n");
				if (write(fd_resp, &status_resp, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
					perror("write");
					close(fd_resp);
					continue;
				}

				for (int i = 0; i < run_count; i++) {
					snprintf(status_resp.payload, sizeof(status_resp.payload), "user-id %d - command-id %ld\n", run_arr[i].user_id, run_arr[i].command_id);
					if (write(fd_resp, &status_resp, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
						perror("write");
						break;
					}
				}

				snprintf(status_resp.payload, sizeof(status_resp.payload), "---Scheduled\n");
				if (write(fd_resp, &status_resp, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
					perror("write");
					close(fd_resp);
					continue;
				}

				for (int i = 0; i < wait_count; i++) {
					snprintf(status_resp.payload, sizeof(status_resp.payload), "user-id %d - command-id %ld\n", wait_arr[i].user_id, wait_arr[i].command_id);
					if (write(fd_resp, &status_resp, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
						perror("write");
						break;
					}
				}

				memset(&status_end, 0, sizeof(status_end));
				status_end.type = STATUS_END;
				status_end.sender_pid = getpid();
				if (write(fd_resp, &status_end, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
					perror("write");
				}

				close(fd_resp);
			}

			if (msg.type == SHUTDOWN_REQ) {
				is_shutting_down = 1;
				shutdown_requester_pid = msg.sender_pid;
			}

			if (is_shutting_down && running_count == 0 && queue_is_empty(waiting_queue)) {
				char resp_fifo[128];
				RpcMessage ack;

				snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", shutdown_requester_pid);
				memset(&ack, 0, sizeof(ack));
				ack.type = SHUTDOWN_ACK;
				ipc_send_atomic(resp_fifo, &ack);
				keep_running = 0;
				break;
			}
		}

		close(server_fd);
	}

	queue_destroy(waiting_queue);
	queue_destroy(running_queue);
	ipc_destroy_fifo(SERVER_FIFO_PATH);

	return 0;
}

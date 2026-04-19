#include "ipc.h"
#include "scheduler.h"

#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
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
	job_info_t finished_job;
	job_queue_t *waiting_queue;
	job_queue_t *running_queue;
	long command_counter = 1;
	char policy[32] = "fcfs";

	srand(time(NULL));

	if (argc > 1) {
		int parsed_limit = atoi(argv[1]);
		if (parsed_limit > 0) {
			parallel_limit = parsed_limit;
		}
	}

	if (argc >= 3) {
		strncpy(policy, argv[2], sizeof(policy) - 1);
		policy[sizeof(policy) - 1] = '\0';
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
					gettimeofday(&job.start_time, NULL);
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
				if (queue_remove_by_command_id(running_queue, msg.command_id, &finished_job)) {
					struct timeval end_time;
					long elapsed_ms;
					int fd_log;
					char log_buf[256];

					gettimeofday(&end_time, NULL);
					elapsed_ms = (end_time.tv_sec - finished_job.start_time.tv_sec) * 1000 + (end_time.tv_usec - finished_job.start_time.tv_usec) / 1000;

					fd_log = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
					if (fd_log != -1) {
						snprintf(log_buf, sizeof(log_buf), "user-id %d - command-id %ld - %ld ms\n", finished_job.user_id, finished_job.command_id, elapsed_ms);
						write(fd_log, log_buf, strlen(log_buf));
						close(fd_log);
					}

					if (running_count > 0) {
						running_count--;
					}
				}

				while (running_count < parallel_limit && !queue_is_empty(waiting_queue)) {
					char resp_fifo[128];
					RpcMessage resp;
					job_info_t next_job;
					int dequeued;

					if (strcmp(policy, "random") == 0) {
						dequeued = queue_dequeue_random(waiting_queue, &next_job);
					} else {
						dequeued = queue_dequeue(waiting_queue, &next_job);
					}

					if (!dequeued) {
						break;
					}

					next_job.state = JOB_RUNNING;
					gettimeofday(&next_job.start_time, NULL);
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

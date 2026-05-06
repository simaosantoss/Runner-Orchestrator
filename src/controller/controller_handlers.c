#include "controller_handlers.h"

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static void send_ack_to_runner(const job_info_t *job) {
	char resp_fifo[128];
	RpcMessage resp;

	snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", job->runner_pid);
	memset(&resp, 0, sizeof(resp));
	resp.type = ACK;
	resp.sender_pid = getpid();
	resp.user_id = job->user_id;
	resp.command_id = job->command_id;

	if (ipc_send_atomic(resp_fifo, &resp) == -1) {
		perror("ipc_send_atomic");
	}
}

static void send_shutdown_ack_to_runner(const RpcMessage *msg) {
	char resp_fifo[128];
	RpcMessage resp;

	snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", msg->sender_pid);
	memset(&resp, 0, sizeof(resp));
	resp.type = SHUTDOWN_ACK;
	resp.sender_pid = getpid();
	resp.user_id = msg->user_id;
	resp.command_id = msg->command_id;

	if (ipc_send_atomic(resp_fifo, &resp) == -1) {
		perror("ipc_send_atomic");
	}
}

static int is_fair_policy(const controller_state_t *state) {
	return strcmp(state->policy, "fair") == 0;
}

static void start_waiting_jobs(controller_state_t *state) {
	while (state->running_count < state->parallel_limit && !queue_is_empty(state->waiting_queue)) {
		job_info_t next_job;
		int dequeued;

		if (strcmp(state->policy, "random") == 0) {
			dequeued = queue_dequeue_random(state->waiting_queue, &next_job);
		} else if (is_fair_policy(state)) {
			dequeued = queue_dequeue_fair(state->waiting_queue, &state->last_scheduled_user, &next_job);
		} else {
			dequeued = queue_dequeue(state->waiting_queue, &next_job);
		}

		if (!dequeued) {
			break;
		}

		next_job.state = JOB_RUNNING;
		if (!queue_enqueue(state->running_queue, &next_job)) {
			fprintf(stderr, "failed to move job to running queue\n");
			continue;
		}

		state->running_count++;
		state->last_scheduled_user = next_job.user_id;
		send_ack_to_runner(&next_job);
	}
}

static void handle_submit(controller_state_t *state, const RpcMessage *msg) {
	job_info_t job;

	memset(&job, 0, sizeof(job));
	job.command_id = msg->command_id;
	job.user_id = msg->user_id;
	job.runner_pid = msg->sender_pid;
	gettimeofday(&job.start_time, NULL);

	if (state->running_count < state->parallel_limit) {
		job.state = JOB_RUNNING;
		if (!queue_enqueue(state->running_queue, &job)) {
			fprintf(stderr, "failed to enqueue running job\n");
			return;
		}

		state->running_count++;
		state->last_scheduled_user = job.user_id;
		send_ack_to_runner(&job);
	} else {
		job.state = JOB_QUEUED;
		if (!queue_enqueue(state->waiting_queue, &job)) {
			fprintf(stderr, "failed to enqueue waiting job\n");
		}
	}
}

static void log_finished_job(const job_info_t *finished_job) {
	struct timeval end_time;
	long elapsed_ms;
	int fd_log;
	char log_buf[256];

	gettimeofday(&end_time, NULL);
	elapsed_ms = (end_time.tv_sec - finished_job->start_time.tv_sec) * 1000 + (end_time.tv_usec - finished_job->start_time.tv_usec) / 1000;

	fd_log = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd_log != -1) {
		snprintf(log_buf, sizeof(log_buf), "user-id %d - command-id %ld - %ld ms\n", finished_job->user_id, finished_job->command_id, elapsed_ms);
		write(fd_log, log_buf, strlen(log_buf));
		close(fd_log);
	}
}

static void handle_done(controller_state_t *state, const RpcMessage *msg) {
	job_info_t finished_job;

	if (queue_remove_by_command_id(state->running_queue, msg->command_id, &finished_job)) {
		log_finished_job(&finished_job);

		if (state->running_count > 0) {
			state->running_count--;
		}
	}

	start_waiting_jobs(state);
}

static int write_status_message(int fd_resp, RpcMessage *status_resp, const char *payload) {
	snprintf(status_resp->payload, sizeof(status_resp->payload), "%s", payload);
	if (write(fd_resp, status_resp, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
		perror("write");
		return -1;
	}

	return 0;
}

static void write_status_jobs(int fd_resp, RpcMessage *status_resp, job_info_t *jobs, int job_count) {
	for (int i = 0; i < job_count; i++) {
		snprintf(status_resp->payload, sizeof(status_resp->payload), "user-id %d - command-id %ld\n", jobs[i].user_id, jobs[i].command_id);
		if (write(fd_resp, status_resp, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
			perror("write");
			break;
		}
	}
}

static void handle_status_req(controller_state_t *state, const RpcMessage *msg) {
	char resp_fifo[128];
	int fd_resp;
	RpcMessage status_resp;
	RpcMessage status_end;
	job_info_t *run_arr = NULL;
	job_info_t *wait_arr = NULL;
	int run_count;
	int wait_count;
	int run_size;
	int wait_size;

	snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", msg->sender_pid);

	run_size = queue_get_size(state->running_queue);
	wait_size = queue_get_size(state->waiting_queue);

	if (run_size > 0) {
		run_arr = malloc((size_t)run_size * sizeof(job_info_t));
		if (run_arr == NULL) {
			fprintf(stderr, "failed to allocate running queue snapshot\n");
			return;
		}
	}

	if (wait_size > 0) {
		wait_arr = malloc((size_t)wait_size * sizeof(job_info_t));
		if (wait_arr == NULL) {
			fprintf(stderr, "failed to allocate waiting queue snapshot\n");
			free(run_arr);
			return;
		}
	}

	run_count = queue_copy_to_array(state->running_queue, run_arr, run_size);
	if (is_fair_policy(state)) {
		wait_count = queue_copy_fair_to_array(state->waiting_queue, wait_arr, wait_size, state->last_scheduled_user);
	} else {
		wait_count = queue_copy_to_array(state->waiting_queue, wait_arr, wait_size);
	}
	fd_resp = open(resp_fifo, O_WRONLY);
	if (fd_resp == -1) {
		perror("open response fifo");
		free(run_arr);
		free(wait_arr);
		return;
	}

	memset(&status_resp, 0, sizeof(status_resp));
	status_resp.type = STATUS_RESP;
	status_resp.sender_pid = getpid();

	if (write_status_message(fd_resp, &status_resp, "---\nExecuting\n") == -1) {
		free(run_arr);
		free(wait_arr);
		close(fd_resp);
		return;
	}

	write_status_jobs(fd_resp, &status_resp, run_arr, run_count);

	if (write_status_message(fd_resp, &status_resp, "---\nScheduled\n") == -1) {
		free(run_arr);
		free(wait_arr);
		close(fd_resp);
		return;
	}

	write_status_jobs(fd_resp, &status_resp, wait_arr, wait_count);

	memset(&status_end, 0, sizeof(status_end));
	status_end.type = STATUS_END;
	status_end.sender_pid = getpid();
	if (write(fd_resp, &status_end, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
		perror("write");
	}

	free(run_arr);
	free(wait_arr);
	close(fd_resp);
}

static void handle_shutdown_req(controller_state_t *state, const RpcMessage *msg) {
	state->is_shutting_down = 1;
	state->shutdown_requester_pid = msg->sender_pid;
}

static int try_finish_shutdown(controller_state_t *state) {
	char resp_fifo[128];
	RpcMessage ack;

	if (!state->is_shutting_down || state->running_count != 0 || !queue_is_empty(state->waiting_queue)) {
		return 0;
	}

	snprintf(resp_fifo, sizeof(resp_fifo), "/tmp/runner_%d", state->shutdown_requester_pid);
	memset(&ack, 0, sizeof(ack));
	ack.type = SHUTDOWN_ACK;
	ipc_send_atomic(resp_fifo, &ack);
	state->keep_running = 0;
	return 1;
}

void controller_configure(controller_state_t *state, int argc, char *argv[]) {
	memset(state, 0, sizeof(*state));
	state->parallel_limit = 4;
	state->running_count = 0;
	state->keep_running = 1;
	state->is_shutting_down = 0;
	state->shutdown_requester_pid = -1;
	state->last_scheduled_user = -1;
	strcpy(state->policy, "fcfs");

	if (argc > 1) {
		int parsed_limit = atoi(argv[1]);
		if (parsed_limit > 0) {
			state->parallel_limit = parsed_limit;
		}
	}

	if (argc >= 3) {
		strncpy(state->policy, argv[2], sizeof(state->policy) - 1);
		state->policy[sizeof(state->policy) - 1] = '\0';
	}
}

int controller_init_queues(controller_state_t *state) {
	state->waiting_queue = queue_create();
	state->running_queue = queue_create();

	if (state->waiting_queue == NULL || state->running_queue == NULL) {
		return -1;
	}

	return 0;
}

void controller_destroy_queues(controller_state_t *state) {
	queue_destroy(state->waiting_queue);
	queue_destroy(state->running_queue);
	state->waiting_queue = NULL;
	state->running_queue = NULL;
}

int controller_handle_message(controller_state_t *state, const RpcMessage *msg) {
	if (msg->type == SUBMIT && state->is_shutting_down) {
		send_shutdown_ack_to_runner(msg);
		return 0;
	}

	if (msg->type == SUBMIT) {
		handle_submit(state, msg);
	}

	if (msg->type == DONE) {
		handle_done(state, msg);
	}

	if (msg->type == STATUS_REQ) {
		handle_status_req(state, msg);
	}

	if (msg->type == SHUTDOWN_REQ) {
		handle_shutdown_req(state, msg);
	}

	return try_finish_shutdown(state);
}

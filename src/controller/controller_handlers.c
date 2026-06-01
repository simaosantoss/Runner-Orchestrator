#include "controller_handlers.h"

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/**
 * @brief Send an execution authorization to the runner that owns a job.
 *
 * @param job Job that is moving to the running queue.
 */
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

/**
 * @brief Notify a runner that the controller is already shutting down.
 *
 * This prevents new runners from blocking forever waiting for a normal ACK
 * after shutdown has started.
 *
 * @param msg Submission message received after shutdown mode was entered.
 */
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

/**
 * @brief Check whether the configured scheduling policy is fair.
 *
 * @param state Controller state containing the policy name.
 * @return 1 when the policy is "fair", 0 otherwise.
 */
static int is_fair_policy(const controller_state_t *state) {
	return strcmp(state->policy, "fair") == 0;
}

/**
 * @brief Register a user in the global order used by fair scheduling.
 *
 * @param state Controller state containing the dynamic fair user array.
 * @param user_id User identifier to remember if not already present.
 */
static void remember_fair_user(controller_state_t *state, int user_id) {
	for (int i = 0; i < state->fair_user_count; i++) {
		if (state->fair_users[i] == user_id) {
			return;
		}
	}

	if (state->fair_user_count == state->fair_user_capacity) {
		int new_capacity;
		int *new_users;

		if (state->fair_user_capacity == 0) {
			new_capacity = 8;
		} else {
			new_capacity = state->fair_user_capacity * 2;
		}

		new_users = realloc(state->fair_users, (size_t)new_capacity * sizeof(int));
		if (new_users == NULL) {
			perror("realloc");
			return;
		}

		state->fair_users = new_users;
		state->fair_user_capacity = new_capacity;
	}

	state->fair_users[state->fair_user_count] = user_id;
	state->fair_user_count++;
}

/**
 * @brief Promote waiting jobs to running jobs while parallel capacity exists.
 *
 * The selected job depends on the configured scheduling policy. Each promoted
 * job is moved to the running queue and receives an ACK through its runner FIFO.
 *
 * @param state Controller state containing queues, policy and counters.
 */
static void start_waiting_jobs(controller_state_t *state) {
	while (state->running_count < state->parallel_limit && !queue_is_empty(state->waiting_queue)) {
		job_info_t next_job;
		int dequeued;

		if (strcmp(state->policy, "random") == 0) {
			dequeued = queue_dequeue_random(state->waiting_queue, &next_job);
		} else if (is_fair_policy(state)) {
			dequeued = queue_dequeue_fair(state->waiting_queue, state->fair_users, state->fair_user_count, &state->last_scheduled_user, &next_job);
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

/**
 * @brief Handle a command submission from a runner.
 *
 * The controller records the submission time, stores the job, and either starts
 * it immediately or queues it depending on the current parallel limit.
 *
 * @param state Controller state to update.
 * @param msg SUBMIT message received from a runner.
 */
static void handle_submit(controller_state_t *state, const RpcMessage *msg) {
	job_info_t job;

	memset(&job, 0, sizeof(job));
	job.command_id = msg->command_id;
	job.user_id = msg->user_id;
	job.runner_pid = msg->sender_pid;
	gettimeofday(&job.start_time, NULL);
	remember_fair_user(state, job.user_id);

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

/**
 * @brief Persist the duration of a finished job in log.txt.
 *
 * Duration is measured as turnaround time from controller submission reception
 * to controller reception of DONE.
 *
 * @param finished_job Job removed from the running queue.
 */
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

/**
 * @brief Handle a DONE message from a runner.
 *
 * Removes the corresponding job from the running queue, writes the persistent
 * log entry, frees one execution slot, and tries to start waiting jobs.
 *
 * @param state Controller state to update.
 * @param msg DONE message received from a runner.
 */
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

/**
 * @brief Write one status response message to a runner FIFO.
 *
 * @param fd_resp Open file descriptor of the runner response FIFO.
 * @param status_resp Reusable STATUS_RESP message buffer.
 * @param payload Text to place in the payload field.
 * @return 0 on success, -1 on write failure.
 */
static int write_status_message(int fd_resp, RpcMessage *status_resp, const char *payload) {
	snprintf(status_resp->payload, sizeof(status_resp->payload), "%s", payload);
	if (write(fd_resp, status_resp, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
		perror("write");
		return -1;
	}

	return 0;
}

/**
 * @brief Send one formatted status line per job.
 *
 * @param fd_resp Open file descriptor of the runner response FIFO.
 * @param status_resp Reusable STATUS_RESP message buffer.
 * @param jobs Array of jobs to print.
 * @param job_count Number of jobs in the array.
 */
static void write_status_jobs(int fd_resp, RpcMessage *status_resp, job_info_t *jobs, int job_count) {
	for (int i = 0; i < job_count; i++) {
		snprintf(status_resp->payload, sizeof(status_resp->payload), "user-id %d - command-id %ld\n", jobs[i].user_id, jobs[i].command_id);
		if (write(fd_resp, status_resp, sizeof(RpcMessage)) != (ssize_t)sizeof(RpcMessage)) {
			perror("write");
			break;
		}
	}
}

/**
 * @brief Handle a status query from a runner.
 *
 * Builds snapshots of the running and waiting queues, formats them into
 * multiple fixed-size STATUS_RESP messages, and terminates the response with
 * STATUS_END.
 *
 * @param state Controller state containing scheduler queues.
 * @param msg STATUS_REQ message received from a runner.
 */
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
		wait_count = queue_copy_fair_to_array(state->waiting_queue, wait_arr, wait_size, state->last_scheduled_user, state->fair_users, state->fair_user_count);
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

/**
 * @brief Mark the controller as shutting down.
 *
 * @param state Controller state to update.
 * @param msg SHUTDOWN_REQ message; its sender receives the final ACK.
 */
static void handle_shutdown_req(controller_state_t *state, const RpcMessage *msg) {
	state->is_shutting_down = 1;
	state->shutdown_requester_pid = msg->sender_pid;
}

/**
 * @brief Finish shutdown if there are no running or waiting jobs.
 *
 * @param state Controller state to inspect and update.
 * @return 1 if the controller should stop its main loop, 0 otherwise.
 */
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

/**
 * @brief Initialize controller configuration from command-line arguments.
 *
 * @param state State object to initialize.
 * @param argc Argument count passed to controller main().
 * @param argv Argument vector passed to controller main().
 */
void controller_configure(controller_state_t *state, int argc, char *argv[]) {
	memset(state, 0, sizeof(*state));
	state->parallel_limit = 4;
	state->running_count = 0;
	state->keep_running = 1;
	state->is_shutting_down = 0;
	state->shutdown_requester_pid = -1;
	state->last_scheduled_user = -1;
	state->fair_users = NULL;
	state->fair_user_count = 0;
	state->fair_user_capacity = 0;
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

/**
 * @brief Allocate the waiting and running queues used by the controller.
 *
 * @param state Controller state to initialize.
 * @return 0 on success, -1 if either queue allocation fails.
 */
int controller_init_queues(controller_state_t *state) {
	state->waiting_queue = queue_create();
	state->running_queue = queue_create();

	if (state->waiting_queue == NULL || state->running_queue == NULL) {
		return -1;
	}

	return 0;
}

/**
 * @brief Release scheduler queues and fair-policy auxiliary memory.
 *
 * @param state Controller state to clean up.
 */
void controller_destroy_queues(controller_state_t *state) {
	queue_destroy(state->waiting_queue);
	queue_destroy(state->running_queue);
	state->waiting_queue = NULL;
	state->running_queue = NULL;
	free(state->fair_users);
	state->fair_users = NULL;
	state->fair_user_count = 0;
	state->fair_user_capacity = 0;
}

/**
 * @brief Dispatch one incoming controller message.
 *
 * @param state Controller state to update.
 * @param msg Message received from the public FIFO.
 * @return 1 if shutdown is complete and the main loop should stop, 0 otherwise.
 */
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

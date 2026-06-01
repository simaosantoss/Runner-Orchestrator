#include "scheduler.h"

#include <stdlib.h>
#include <string.h>

typedef struct job_node {
	job_info_t job;
	struct job_node *next;
} job_node_t;

struct job_queue {
	job_node_t *head;
	job_node_t *tail;
	int size;
};

/**
 * @brief Allocate an empty linked-list job queue.
 *
 * @return New queue pointer, or NULL on allocation failure.
 */
job_queue_t* queue_create() {
	job_queue_t *q;

	q = (job_queue_t*)malloc(sizeof(job_queue_t));
	if (q == NULL) {
		return NULL;
	}

	q->head = NULL;
	q->tail = NULL;
	q->size = 0;

	return q;
}

/**
 * @brief Free all nodes in a queue and then free the queue object itself.
 *
 * @param q Queue to destroy. NULL is accepted.
 */
void queue_destroy(job_queue_t *q) {
	job_node_t *curr;

	if (q == NULL) {
		return;
	}

	curr = q->head;
	while (curr != NULL) {
		job_node_t *next = curr->next;
		free(curr);
		curr = next;
	}

	free(q);
}

/**
 * @brief Append a job to the tail of a queue.
 *
 * @param q Queue that receives the new job.
 * @param job Job data copied into the queue node.
 * @return 1 on success, 0 on invalid input or allocation failure.
 */
int queue_enqueue(job_queue_t *q, const job_info_t *job) {
	job_node_t *new_node;

	if (q == NULL || job == NULL) {
		return 0;
	}

	new_node = (job_node_t*)malloc(sizeof(job_node_t));
	if (new_node == NULL) {
		return 0;
	}

	new_node->job = *job;
	new_node->next = NULL;

	if (q->tail == NULL) {
		q->head = new_node;
		q->tail = new_node;
	} else {
		q->tail->next = new_node;
		q->tail = new_node;
	}

	q->size++;
	return 1;
}

/**
 * @brief Remove the head of a queue.
 *
 * This is the primitive used by the FCFS policy.
 *
 * @param q Queue to remove from.
 * @param out_job Destination for the removed job.
 * @return 1 when a job is removed, 0 otherwise.
 */
int queue_dequeue(job_queue_t *q, job_info_t *out_job) {
	job_node_t *old_head;

	if (q == NULL || out_job == NULL || q->head == NULL) {
		return 0;
	}

	old_head = q->head;
	*out_job = old_head->job;
	q->head = old_head->next;

	if (q->head == NULL) {
		q->tail = NULL;
	}

	free(old_head);
	q->size--;
	return 1;
}

/**
 * @brief Remove a randomly selected job from a queue.
 *
 * @param q Queue to remove from.
 * @param out_job Destination for the removed job.
 * @return 1 when a job is removed, 0 otherwise.
 */
int queue_dequeue_random(job_queue_t *q, job_info_t *out_job) {
	int target_idx;
	job_node_t *prev;
	job_node_t *curr;

	if (q == NULL || out_job == NULL || q->head == NULL) {
		return 0;
	}

	target_idx = rand() % q->size;
	if (target_idx == 0) {
		return queue_dequeue(q, out_job);
	}

	prev = q->head;
	curr = q->head->next;
	for (int i = 1; i < target_idx && curr != NULL; i++) {
		prev = curr;
		curr = curr->next;
	}

	if (curr == NULL) {
		return 0;
	}

	prev->next = curr->next;
	if (curr->next == NULL) {
		q->tail = prev;
	}

	*out_job = curr->job;
	free(curr);
	q->size--;
	return 1;
}

/**
 * @brief Check if a user identifier is already present in an array.
 *
 * @param users Array of user identifiers.
 * @param user_count Number of valid entries in users.
 * @param user_id User identifier to find.
 * @return 1 if present, 0 otherwise.
 */
static int user_exists(const int *users, int user_count, int user_id) {
	for (int i = 0; i < user_count; i++) {
		if (users[i] == user_id) {
			return 1;
		}
	}

	return 0;
}

/**
 * @brief Choose the next user in circular order from a compact user array.
 *
 * @param users Array of active users.
 * @param user_count Number of users in the array.
 * @param last_user_id User chosen in the previous scheduling decision.
 * @return Next user identifier, or -1 if the array is empty.
 */
static int choose_next_user(const int *users, int user_count, int last_user_id) {
	int last_idx;

	if (user_count <= 0) {
		return -1;
	}

	last_idx = -1;
	for (int i = 0; i < user_count; i++) {
		if (users[i] == last_user_id) {
			last_idx = i;
			break;
		}
	}

	if (last_idx == -1 || last_idx == user_count - 1) {
		return users[0];
	}

	return users[last_idx + 1];
}

/**
 * @brief Choose the next active user according to the fair policy order.
 *
 * @param active_users Users that currently have queued jobs.
 * @param active_user_count Number of active users.
 * @param fair_users Global order of users seen by the controller.
 * @param fair_user_count Number of entries in fair_users.
 * @param last_user_id User selected in the previous scheduling decision.
 * @return Next user identifier, or -1 if no active user exists.
 */
static int choose_next_fair_user(const int *active_users, int active_user_count, const int *fair_users, int fair_user_count, int last_user_id) {
	int last_idx;

	if (active_user_count <= 0) {
		return -1;
	}

	if (fair_users == NULL || fair_user_count <= 0) {
		return choose_next_user(active_users, active_user_count, last_user_id);
	}

	last_idx = -1;
	for (int i = 0; i < fair_user_count; i++) {
		if (fair_users[i] == last_user_id) {
			last_idx = i;
			break;
		}
	}

	for (int offset = 1; offset <= fair_user_count; offset++) {
		int idx = (last_idx + offset + fair_user_count) % fair_user_count;
		if (user_exists(active_users, active_user_count, fair_users[idx])) {
			return fair_users[idx];
		}
	}

	return active_users[0];
}

/**
 * @brief Remove the first queued job belonging to the next fair user.
 *
 * @param q Waiting queue to remove from.
 * @param fair_users Global order of users seen by the controller.
 * @param fair_user_count Number of entries in fair_users.
 * @param last_user_id In/out last scheduled user identifier.
 * @param out_job Destination for the removed job.
 * @return 1 if a job was removed, 0 otherwise.
 */
int queue_dequeue_fair(job_queue_t *q, const int *fair_users, int fair_user_count, int *last_user_id, job_info_t *out_job) {
	int *users;
	int user_count;
	int target_user;
	job_node_t *prev;
	job_node_t *curr;

	if (q == NULL || last_user_id == NULL || out_job == NULL || q->head == NULL) {
		return 0;
	}

	users = malloc((size_t)q->size * sizeof(int));
	if (users == NULL) {
		if (queue_dequeue(q, out_job)) {
			*last_user_id = out_job->user_id;
			return 1;
		}
		return 0;
	}

	user_count = 0;
	for (curr = q->head; curr != NULL; curr = curr->next) {
		if (!user_exists(users, user_count, curr->job.user_id)) {
			users[user_count] = curr->job.user_id;
			user_count++;
		}
	}

	target_user = choose_next_fair_user(users, user_count, fair_users, fair_user_count, *last_user_id);
	free(users);

	prev = NULL;
	curr = q->head;
	while (curr != NULL) {
		if (curr->job.user_id == target_user) {
			*out_job = curr->job;

			if (prev == NULL) {
				q->head = curr->next;
			} else {
				prev->next = curr->next;
			}

			if (q->tail == curr) {
				q->tail = prev;
			}

			free(curr);
			q->size--;
			*last_user_id = target_user;
			return 1;
		}

		prev = curr;
		curr = curr->next;
	}

	return 0;
}

/**
 * @brief Remove the job with the given command identifier.
 *
 * @param q Queue to search.
 * @param command_id Command identifier to remove.
 * @param out_job Destination for the removed job.
 * @return 1 if the job was found and removed, 0 otherwise.
 */
int queue_remove_by_command_id(job_queue_t *q, long command_id, job_info_t *out_job) {
	job_node_t *prev;
	job_node_t *curr;

	if (q == NULL || out_job == NULL) {
		return 0;
	}

	prev = NULL;
	curr = q->head;

	while (curr != NULL) {
		if (curr->job.command_id == command_id) {
			*out_job = curr->job;

			if (prev == NULL) {
				q->head = curr->next;
			} else {
				prev->next = curr->next;
			}

			if (q->tail == curr) {
				q->tail = prev;
			}

			free(curr);
			q->size--;
			return 1;
		}

		prev = curr;
		curr = curr->next;
	}

	return 0;
}

/**
 * @brief Check if a queue is empty.
 *
 * @param q Queue to inspect.
 * @return 1 if empty or NULL, 0 otherwise.
 */
int queue_is_empty(const job_queue_t *q) {
	if (q == NULL) {
		return 1;
	}

	return (q->size == 0) ? 1 : 0;
}

/**
 * @brief Return the current queue size.
 *
 * @param q Queue to inspect.
 * @return Number of jobs in the queue, or 0 if q is NULL.
 */
int queue_size(const job_queue_t *q) {
	if (q == NULL) {
		return 0;
	}

	return q->size;
}

/**
 * @brief Return the current queue size.
 *
 * @param q Queue to inspect.
 * @return Number of jobs in the queue, or 0 if q is NULL.
 */
int queue_get_size(const job_queue_t *q) {
	if (q == NULL) {
		return 0;
	}

	return q->size;
}

/**
 * @brief Copy jobs from a queue into an array without modifying the queue.
 *
 * @param q Queue to copy.
 * @param array Destination array.
 * @param max_size Maximum number of jobs to copy.
 * @return Number of jobs copied.
 */
int queue_copy_to_array(const job_queue_t *q, job_info_t *array, int max_size) {
	const job_node_t *curr;
	int copied;

	if (q == NULL || array == NULL || max_size <= 0) {
		return 0;
	}

	curr = q->head;
	copied = 0;
	while (curr != NULL && copied < max_size) {
		array[copied] = curr->job;
		copied++;
		curr = curr->next;
	}

	return copied;
}

/**
 * @brief Copy queued jobs in the order that fair scheduling would select them.
 *
 * This is used to present a coherent Scheduled section for runner -c without
 * mutating the real waiting queue.
 *
 * @param q Waiting queue to simulate.
 * @param array Destination array for the simulated order.
 * @param max_size Maximum number of jobs to copy.
 * @param last_user_id Last user selected by the real scheduler.
 * @param fair_users Global user order remembered by the controller.
 * @param fair_user_count Number of entries in fair_users.
 * @return Number of jobs copied.
 */
int queue_copy_fair_to_array(const job_queue_t *q, job_info_t *array, int max_size, int last_user_id, const int *fair_users, int fair_user_count) {
	job_info_t *jobs;
	int *used;
	int copied;
	int current_last_user;

	if (q == NULL || array == NULL || max_size <= 0 || q->size <= 0) {
		return 0;
	}

	jobs = malloc((size_t)q->size * sizeof(job_info_t));
	used = malloc((size_t)q->size * sizeof(int));
	if (jobs == NULL || used == NULL) {
		free(jobs);
		free(used);
		return queue_copy_to_array(q, array, max_size);
	}

	memset(used, 0, (size_t)q->size * sizeof(int));
	queue_copy_to_array(q, jobs, q->size);

	copied = 0;
	current_last_user = last_user_id;
	while (copied < max_size && copied < q->size) {
		int *users;
		int user_count;
		int target_user;
		int target_idx;

		users = malloc((size_t)q->size * sizeof(int));
		if (users == NULL) {
			break;
		}

		user_count = 0;
		for (int i = 0; i < q->size; i++) {
			if (!used[i] && !user_exists(users, user_count, jobs[i].user_id)) {
				users[user_count] = jobs[i].user_id;
				user_count++;
			}
		}

		target_user = choose_next_fair_user(users, user_count, fair_users, fair_user_count, current_last_user);
		free(users);

		if (target_user == -1) {
			break;
		}

		target_idx = -1;
		for (int i = 0; i < q->size; i++) {
			if (!used[i] && jobs[i].user_id == target_user) {
				target_idx = i;
				break;
			}
		}

		if (target_idx == -1) {
			break;
		}

		array[copied] = jobs[target_idx];
		used[target_idx] = 1;
		current_last_user = target_user;
		copied++;
	}

	free(jobs);
	free(used);
	return copied;
}

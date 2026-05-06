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

static int user_exists(const int *users, int user_count, int user_id) {
	for (int i = 0; i < user_count; i++) {
		if (users[i] == user_id) {
			return 1;
		}
	}

	return 0;
}

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

int queue_dequeue_fair(job_queue_t *q, int *last_user_id, job_info_t *out_job) {
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

	target_user = choose_next_user(users, user_count, *last_user_id);
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

int queue_is_empty(const job_queue_t *q) {
	if (q == NULL) {
		return 1;
	}

	return (q->size == 0) ? 1 : 0;
}

int queue_size(const job_queue_t *q) {
	if (q == NULL) {
		return 0;
	}

	return q->size;
}

int queue_get_size(const job_queue_t *q) {
	if (q == NULL) {
		return 0;
	}

	return q->size;
}

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

int queue_copy_fair_to_array(const job_queue_t *q, job_info_t *array, int max_size, int last_user_id) {
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

		target_user = choose_next_user(users, user_count, current_last_user);
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

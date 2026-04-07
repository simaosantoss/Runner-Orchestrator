#include "scheduler.h"

#include <stdlib.h>

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

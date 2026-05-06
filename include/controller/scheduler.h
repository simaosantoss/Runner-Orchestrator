#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>
#include <sys/time.h>

typedef enum {
	JOB_QUEUED,
	JOB_RUNNING
} JobState;

typedef struct {
	long command_id;
	int user_id;
	JobState state;
	pid_t runner_pid;
	struct timeval start_time;
} job_info_t;

typedef struct job_queue job_queue_t;

job_queue_t* queue_create();
void queue_destroy(job_queue_t *q);
int queue_enqueue(job_queue_t *q, const job_info_t *job);
int queue_dequeue(job_queue_t *q, job_info_t *out_job);
int queue_dequeue_random(job_queue_t *q, job_info_t *out_job);
int queue_dequeue_fair(job_queue_t *q, int *last_user_id, job_info_t *out_job);
int queue_remove_by_command_id(job_queue_t *q, long command_id, job_info_t *out_job);
int queue_is_empty(const job_queue_t *q);
int queue_size(const job_queue_t *q);
int queue_get_size(const job_queue_t *q);
int queue_copy_to_array(const job_queue_t *q, job_info_t *array, int max_size);
int queue_copy_fair_to_array(const job_queue_t *q, job_info_t *array, int max_size, int last_user_id);

#endif /* SCHEDULER_H */

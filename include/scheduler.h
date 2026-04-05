#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>

typedef enum {
	JOB_QUEUED,
	JOB_RUNNING
} JobState;

typedef struct {
	long command_id;
	int user_id;
	JobState state;
	pid_t runner_pid;
} job_info_t;

typedef struct job_queue job_queue_t;

job_queue_t* queue_create();
void queue_destroy(job_queue_t *q);
int queue_enqueue(job_queue_t *q, const job_info_t *job);
int queue_dequeue(job_queue_t *q, job_info_t *out_job);
int queue_remove_by_command_id(job_queue_t *q, long command_id, job_info_t *out_job);
int queue_is_empty(const job_queue_t *q);
int queue_size(const job_queue_t *q);

#endif /* SCHEDULER_H */

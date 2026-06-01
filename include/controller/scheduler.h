#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>
#include <sys/time.h>

/**
 * @brief State of a job from the controller's point of view.
 */
typedef enum {
	JOB_QUEUED,  /**< Job has been submitted but not authorized yet. */
	JOB_RUNNING  /**< Job has been authorized and is expected to notify DONE. */
} JobState;

/**
 * @brief Information tracked by the controller for each submitted command.
 */
typedef struct {
	long command_id;             /**< Unique command identifier chosen by the runner. */
	int user_id;                 /**< User identifier used for status, logging and fair scheduling. */
	JobState state;              /**< Current scheduler state of the job. */
	pid_t runner_pid;            /**< PID of the runner that owns this command. */
	struct timeval start_time;   /**< Time at which the controller received the submission. */
} job_info_t;

/**
 * @brief Opaque linked-list queue type used by the controller scheduler.
 */
typedef struct job_queue job_queue_t;

/**
 * @brief Allocate and initialize an empty job queue.
 *
 * @return Pointer to a new queue, or NULL on allocation failure.
 */
job_queue_t* queue_create();

/**
 * @brief Destroy a queue and all nodes stored in it.
 *
 * @param q Queue to destroy. NULL is accepted.
 */
void queue_destroy(job_queue_t *q);

/**
 * @brief Append a job to the tail of a queue.
 *
 * @param q Destination queue.
 * @param job Job data to copy into the new queue node.
 * @return 1 on success, 0 on invalid input or allocation failure.
 */
int queue_enqueue(job_queue_t *q, const job_info_t *job);

/**
 * @brief Remove the head of the queue, implementing FCFS order.
 *
 * @param q Queue to remove from.
 * @param out_job Destination for the removed job.
 * @return 1 if a job was removed, 0 otherwise.
 */
int queue_dequeue(job_queue_t *q, job_info_t *out_job);

/**
 * @brief Remove one randomly selected job from the queue.
 *
 * @param q Queue to remove from.
 * @param out_job Destination for the removed job.
 * @return 1 if a job was removed, 0 otherwise.
 */
int queue_dequeue_random(job_queue_t *q, job_info_t *out_job);

/**
 * @brief Remove the next job according to the fair user-based policy.
 *
 * @param q Queue to remove from.
 * @param fair_users Global order of users known to the controller.
 * @param fair_user_count Number of entries in fair_users.
 * @param last_user_id In/out parameter with the last scheduled user.
 * @param out_job Destination for the removed job.
 * @return 1 if a job was removed, 0 otherwise.
 */
int queue_dequeue_fair(job_queue_t *q, const int *fair_users, int fair_user_count, int *last_user_id, job_info_t *out_job);

/**
 * @brief Remove a job by command identifier.
 *
 * Used when a runner sends DONE and the controller must remove the job from
 * the running queue.
 *
 * @param q Queue to search.
 * @param command_id Command identifier to remove.
 * @param out_job Destination for the removed job.
 * @return 1 if the job was found and removed, 0 otherwise.
 */
int queue_remove_by_command_id(job_queue_t *q, long command_id, job_info_t *out_job);

/**
 * @brief Check whether a queue is empty.
 *
 * @param q Queue to inspect. NULL is treated as empty.
 * @return 1 if empty, 0 otherwise.
 */
int queue_is_empty(const job_queue_t *q);

/**
 * @brief Return the number of jobs in a queue.
 *
 * @param q Queue to inspect.
 * @return Queue size, or 0 if q is NULL.
 */
int queue_size(const job_queue_t *q);

/**
 * @brief Return the number of jobs in a queue.
 *
 * @param q Queue to inspect.
 * @return Queue size, or 0 if q is NULL.
 */
int queue_get_size(const job_queue_t *q);

/**
 * @brief Copy queue contents to an array without modifying the queue.
 *
 * @param q Queue to copy from.
 * @param array Destination array.
 * @param max_size Maximum number of jobs to copy.
 * @return Number of jobs copied.
 */
int queue_copy_to_array(const job_queue_t *q, job_info_t *array, int max_size);

/**
 * @brief Copy queue contents in the simulated order of the fair policy.
 *
 * This is used for status output: the controller can show the expected fair
 * scheduling order without changing the real waiting queue.
 *
 * @param q Queue to copy from.
 * @param array Destination array.
 * @param max_size Maximum number of jobs to copy.
 * @param last_user_id Last user scheduled in the real controller state.
 * @param fair_users Global fair user order.
 * @param fair_user_count Number of users in fair_users.
 * @return Number of jobs copied.
 */
int queue_copy_fair_to_array(const job_queue_t *q, job_info_t *array, int max_size, int last_user_id, const int *fair_users, int fair_user_count);

#endif /* SCHEDULER_H */

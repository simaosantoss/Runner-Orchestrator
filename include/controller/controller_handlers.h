#ifndef CONTROLLER_HANDLERS_H
#define CONTROLLER_HANDLERS_H

#include "ipc.h"
#include "scheduler.h"

/**
 * @brief Complete mutable state of the controller process.
 */
typedef struct {
	int parallel_limit;             /**< Maximum number of commands authorized concurrently. */
	int running_count;              /**< Current number of authorized commands still running. */
	int keep_running;               /**< Main loop flag; set to 0 when shutdown can finish. */
	int is_shutting_down;           /**< Whether a shutdown request has already been received. */
	pid_t shutdown_requester_pid;   /**< Runner PID that must receive the final shutdown ACK. */
	int last_scheduled_user;        /**< Last user selected by the scheduler, used by fair policy. */
	int *fair_users;                /**< Dynamic array containing the global order of seen users. */
	int fair_user_count;            /**< Number of valid entries in fair_users. */
	int fair_user_capacity;         /**< Allocated capacity of fair_users. */
	job_queue_t *waiting_queue;     /**< Jobs submitted but not yet authorized. */
	job_queue_t *running_queue;     /**< Jobs authorized and waiting for DONE. */
	char policy[32];                /**< Scheduling policy name: fcfs, random or fair. */
} controller_state_t;

/**
 * @brief Initialize controller state from command-line arguments.
 *
 * @param state State object to initialize.
 * @param argc Argument count received by controller main().
 * @param argv Argument vector received by controller main().
 */
void controller_configure(controller_state_t *state, int argc, char *argv[]);

/**
 * @brief Allocate the controller waiting and running queues.
 *
 * @param state Controller state whose queues will be initialized.
 * @return 0 on success, -1 on allocation failure.
 */
int controller_init_queues(controller_state_t *state);

/**
 * @brief Free the controller queues and fair-policy auxiliary memory.
 *
 * @param state Controller state to clean up.
 */
void controller_destroy_queues(controller_state_t *state);

/**
 * @brief Process one message received from the public controller FIFO.
 *
 * @param state Controller state to update.
 * @param msg Message received from a runner.
 * @return 1 if the controller should leave its main loop, 0 otherwise.
 */
int controller_handle_message(controller_state_t *state, const RpcMessage *msg);

#endif /* CONTROLLER_HANDLERS_H */

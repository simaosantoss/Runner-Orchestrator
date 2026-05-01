#ifndef CONTROLLER_HANDLERS_H
#define CONTROLLER_HANDLERS_H

#include "ipc.h"
#include "scheduler.h"

typedef struct {
	int parallel_limit;
	int running_count;
	int keep_running;
	int is_shutting_down;
	pid_t shutdown_requester_pid;
	job_queue_t *waiting_queue;
	job_queue_t *running_queue;
	char policy[32];
} controller_state_t;

void controller_configure(controller_state_t *state, int argc, char *argv[]);
int controller_init_queues(controller_state_t *state);
void controller_destroy_queues(controller_state_t *state);
int controller_handle_message(controller_state_t *state, const RpcMessage *msg);

#endif /* CONTROLLER_HANDLERS_H */

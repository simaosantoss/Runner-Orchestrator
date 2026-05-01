#ifndef RUNNER_CLI_H
#define RUNNER_CLI_H

typedef enum {
	RUNNER_MODE_EXECUTE,
	RUNNER_MODE_STATUS,
	RUNNER_MODE_SHUTDOWN
} runner_mode_t;

typedef struct {
	runner_mode_t mode;
	int user_id;
	const char *command;
} runner_request_t;

int runner_parse_args(int argc, char *argv[], runner_request_t *request);
int runner_run_status(void);
int runner_run_shutdown(void);
int runner_run_execute(int user_id, const char *command);

#endif /* RUNNER_CLI_H */

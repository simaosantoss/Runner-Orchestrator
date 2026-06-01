#ifndef RUNNER_CLI_H
#define RUNNER_CLI_H

/**
 * @brief Operating mode requested by the runner command line.
 */
typedef enum {
	RUNNER_MODE_EXECUTE,  /**< Execute a command after controller authorization (-e). */
	RUNNER_MODE_STATUS,   /**< Query controller state (-c). */
	RUNNER_MODE_SHUTDOWN  /**< Request controlled controller shutdown (-s). */
} runner_mode_t;

/**
 * @brief Parsed runner command-line request.
 */
typedef struct {
	runner_mode_t mode;  /**< Selected runner mode. */
	int user_id;         /**< User identifier used by -e. */
	const char *command; /**< Command string used by -e. */
} runner_request_t;

/**
 * @brief Parse runner command-line arguments.
 *
 * @param argc Argument count received by runner main().
 * @param argv Argument vector received by runner main().
 * @param request Output structure filled with the selected mode and values.
 * @return 0 on success, non-zero on invalid usage.
 */
int runner_parse_args(int argc, char *argv[], runner_request_t *request);

/**
 * @brief Implement runner -c by requesting and printing controller status.
 *
 * @return 0 on success, non-zero on communication error.
 */
int runner_run_status(void);

/**
 * @brief Implement runner -s by requesting controlled controller shutdown.
 *
 * @return 0 on success, non-zero on communication error.
 */
int runner_run_shutdown(void);

/**
 * @brief Implement runner -e by submitting, executing and completing a command.
 *
 * @param user_id User identifier to send to the controller.
 * @param command Command string to execute after receiving ACK.
 * @return 0 on success, non-zero on error.
 */
int runner_run_execute(int user_id, const char *command);

#endif /* RUNNER_CLI_H */

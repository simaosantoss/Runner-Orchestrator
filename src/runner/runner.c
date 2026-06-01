#include "runner_cli.h"

#include <stdlib.h>
#include <stdio.h>

/**
 * @brief Runner entry point.
 *
 * Parses the command-line mode and delegates to the corresponding runner
 * operation: execute (-e), status (-c), or shutdown (-s).
 *
 * @param argc Number of command-line arguments.
 * @param argv Runner arguments.
 * @return Exit status of the selected runner operation.
 */
int main(int argc, char *argv[]) {
	runner_request_t request;
	int parse_result;

	parse_result = runner_parse_args(argc, argv, &request);
	if (parse_result != 0) {
		return parse_result;
	}

	if (request.mode == RUNNER_MODE_STATUS) {
		return runner_run_status();
	}

	if (request.mode == RUNNER_MODE_SHUTDOWN) {
		return runner_run_shutdown();
	}

	if (request.mode == RUNNER_MODE_EXECUTE) {
		return runner_run_execute(request.user_id, request.command);
	}

	printf("Usage: ./runner -e <user_id> \"<command...>\"\n");
	return 1;
}

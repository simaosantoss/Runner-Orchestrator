#include "runner_cli.h"

#include <stdlib.h>
#include <stdio.h>

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

#include "parser.h"

#include <string.h>

void parse_command(const char *input, parsed_command_t *parsed) {
	char *save_pipe = NULL;
	char *stage_str;

	memset(parsed, 0, sizeof(*parsed));

	if (input == NULL) {
		return;
	}

	strncpy(parsed->buffer, input, sizeof(parsed->buffer) - 1);
	parsed->buffer[sizeof(parsed->buffer) - 1] = '\0';

	stage_str = strtok_r(parsed->buffer, "|", &save_pipe);
	while (stage_str != NULL && parsed->num_stages < PARSER_MAX_STAGES) {
		command_stage_t *stage = &parsed->stages[parsed->num_stages];
		char *save_arg = NULL;
		char *arg;
		int argc = 0;

		arg = strtok_r(stage_str, " ", &save_arg);
		while (arg != NULL && argc < (PARSER_MAX_ARGS - 1)) {
			stage->argv[argc] = arg;
			argc++;
			arg = strtok_r(NULL, " ", &save_arg);
		}

		stage->argv[argc] = NULL;
		parsed->num_stages++;
		stage_str = strtok_r(NULL, "|", &save_pipe);
	}
}

#include "parser.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	char *argv[PARSER_MAX_ARGS];
} command_stage_t;

struct parsed_command {
	int stage_count;
	command_stage_t stages[PARSER_MAX_STAGES];
	char buffer[PARSER_INPUT_BUFFER];
};

parsed_command_t* parser_parse(const char *input) {
	parsed_command_t *parsed;
	char *save_pipe = NULL;
	char *stage_str;

	if (input == NULL) {
		return NULL;
	}

	parsed = malloc(sizeof(*parsed));
	if (parsed == NULL) {
		return NULL;
	}

	memset(parsed, 0, sizeof(*parsed));

	strncpy(parsed->buffer, input, sizeof(parsed->buffer) - 1);
	parsed->buffer[sizeof(parsed->buffer) - 1] = '\0';

	stage_str = strtok_r(parsed->buffer, "|", &save_pipe);
	while (stage_str != NULL && parsed->stage_count < PARSER_MAX_STAGES) {
		command_stage_t *stage = &parsed->stages[parsed->stage_count];
		char *save_arg = NULL;
		char *arg;
		int argc = 0;

		arg = strtok_r(stage_str, " ", &save_arg);
		while (arg != NULL && argc < (PARSER_MAX_ARGS - 1)) {
			stage->argv[argc] = strdup(arg);
			if (stage->argv[argc] == NULL) {
				parser_destroy(parsed);
				return NULL;
			}
			argc++;
			arg = strtok_r(NULL, " ", &save_arg);
		}

		stage->argv[argc] = NULL;
		parsed->stage_count++;
		stage_str = strtok_r(NULL, "|", &save_pipe);
	}

	return parsed;
}

int parser_get_stage_count(const parsed_command_t *cmd) {
	if (cmd == NULL) {
		return 0;
	}

	return cmd->stage_count;
}

char** parser_get_stage_argv(const parsed_command_t *cmd, int stage_idx) {
	if (cmd == NULL || stage_idx < 0 || stage_idx >= cmd->stage_count) {
		return NULL;
	}

	return (char **)cmd->stages[stage_idx].argv;
}

void parser_destroy(parsed_command_t *cmd) {
	int i;

	if (cmd == NULL) {
		return;
	}

	for (i = 0; i < cmd->stage_count; i++) {
		int j;

		for (j = 0; j < PARSER_MAX_ARGS && cmd->stages[i].argv[j] != NULL; j++) {
			free(cmd->stages[i].argv[j]);
		}
	}

	free(cmd);
}

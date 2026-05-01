#include "parser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char *argv[PARSER_MAX_ARGS];
	char *input_file;
	char *output_file;
	char *error_file;
} command_stage_t;

struct parsed_command {
	int stage_count;
	command_stage_t stages[PARSER_MAX_STAGES];
	char buffer[PARSER_INPUT_BUFFER];
};

static const char* skip_spaces(const char *cursor) {
	while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
		cursor++;
	}

	return cursor;
}

static char* duplicate_range(const char *start, size_t len) {
	char *copy;

	copy = malloc(len + 1);
	if (copy == NULL) {
		return NULL;
	}

	memcpy(copy, start, len);
	copy[len] = '\0';
	return copy;
}

static const char* parse_token(const char *cursor, char **out_token) {
	const char *start;

	cursor = skip_spaces(cursor);
	if (*cursor == '\0' || *cursor == '<' || *cursor == '>') {
		return NULL;
	}

	start = cursor;
	while (*cursor != '\0' && !isspace((unsigned char)*cursor) && *cursor != '<' && *cursor != '>') {
		cursor++;
	}

	*out_token = duplicate_range(start, (size_t)(cursor - start));
	if (*out_token == NULL) {
		return NULL;
	}

	return cursor;
}

static int parse_stage(command_stage_t *stage, char *stage_str) {
	const char *cursor;
	int argc;

	cursor = stage_str;
	argc = 0;

	while (1) {
		char *token;
		char **redirect_target;
		const char *next_cursor;

		cursor = skip_spaces(cursor);
		if (*cursor == '\0') {
			break;
		}

		redirect_target = NULL;
		if (cursor[0] == '2' && cursor[1] == '>') {
			redirect_target = &stage->error_file;
			cursor += 2;
		} else if (*cursor == '<') {
			redirect_target = &stage->input_file;
			cursor++;
		} else if (*cursor == '>') {
			redirect_target = &stage->output_file;
			cursor++;
		}

		if (redirect_target != NULL) {
			token = NULL;
			next_cursor = parse_token(cursor, &token);
			if (next_cursor == NULL) {
				return -1;
			}

			free(*redirect_target);
			*redirect_target = token;
			cursor = next_cursor;
			continue;
		}

		token = NULL;
		next_cursor = parse_token(cursor, &token);
		if (next_cursor == NULL) {
			return -1;
		}

		if (argc < (PARSER_MAX_ARGS - 1)) {
			stage->argv[argc] = token;
			argc++;
		} else {
			free(token);
		}

		cursor = next_cursor;
	}

	stage->argv[argc] = NULL;
	return 0;
}

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
		if (parse_stage(stage, stage_str) == -1) {
			parser_destroy(parsed);
			return NULL;
		}

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

const char* parser_get_input_file(const parsed_command_t *cmd, int stage_idx) {
	if (cmd == NULL || stage_idx < 0 || stage_idx >= cmd->stage_count) {
		return NULL;
	}

	return cmd->stages[stage_idx].input_file;
}

const char* parser_get_output_file(const parsed_command_t *cmd, int stage_idx) {
	if (cmd == NULL || stage_idx < 0 || stage_idx >= cmd->stage_count) {
		return NULL;
	}

	return cmd->stages[stage_idx].output_file;
}

const char* parser_get_error_file(const parsed_command_t *cmd, int stage_idx) {
	if (cmd == NULL || stage_idx < 0 || stage_idx >= cmd->stage_count) {
		return NULL;
	}

	return cmd->stages[stage_idx].error_file;
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

		free(cmd->stages[i].input_file);
		free(cmd->stages[i].output_file);
		free(cmd->stages[i].error_file);
	}

	free(cmd);
}

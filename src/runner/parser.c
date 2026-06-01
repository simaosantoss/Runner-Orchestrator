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

/**
 * @brief Advance a string cursor over whitespace.
 *
 * @param cursor Current parsing position.
 * @return First non-space position, or the string terminator.
 */
static const char* skip_spaces(const char *cursor) {
	while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
		cursor++;
	}

	return cursor;
}

/**
 * @brief Allocate a null-terminated copy of a string range.
 *
 * @param start Start of the range to copy.
 * @param len Number of characters to copy.
 * @return Newly allocated string, or NULL on allocation failure.
 */
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

/**
 * @brief Parse one token until whitespace or a redirection operator.
 *
 * @param cursor Current parsing position.
 * @param out_token Destination for the allocated token.
 * @return Position immediately after the token, or NULL on parse/allocation failure.
 */
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

/**
 * @brief Parse one pipeline stage into argv and redirection fields.
 *
 * Operators are not stored in argv. Instead, their target filenames are stored
 * in input_file, output_file or error_file.
 *
 * @param stage Stage structure to fill.
 * @param stage_str Mutable string for this pipeline stage.
 * @return 0 on success, -1 on parse/allocation failure.
 */
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

/**
 * @brief Parse a full command string into a pipeline representation.
 *
 * @param input Command string received by the runner.
 * @return Newly allocated parsed command, or NULL on failure.
 */
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

/**
 * @brief Get the number of stages in a parsed pipeline.
 *
 * @param cmd Parsed command.
 * @return Stage count, or 0 if cmd is NULL.
 */
int parser_get_stage_count(const parsed_command_t *cmd) {
	if (cmd == NULL) {
		return 0;
	}

	return cmd->stage_count;
}

/**
 * @brief Get the argv array for a pipeline stage.
 *
 * @param cmd Parsed command.
 * @param stage_idx Zero-based stage index.
 * @return NULL-terminated argv array, or NULL if invalid.
 */
char** parser_get_stage_argv(const parsed_command_t *cmd, int stage_idx) {
	if (cmd == NULL || stage_idx < 0 || stage_idx >= cmd->stage_count) {
		return NULL;
	}

	return (char **)cmd->stages[stage_idx].argv;
}

/**
 * @brief Get the input redirection filename for a stage.
 *
 * @param cmd Parsed command.
 * @param stage_idx Zero-based stage index.
 * @return Input filename, or NULL if absent/invalid.
 */
const char* parser_get_input_file(const parsed_command_t *cmd, int stage_idx) {
	if (cmd == NULL || stage_idx < 0 || stage_idx >= cmd->stage_count) {
		return NULL;
	}

	return cmd->stages[stage_idx].input_file;
}

/**
 * @brief Get the output redirection filename for a stage.
 *
 * @param cmd Parsed command.
 * @param stage_idx Zero-based stage index.
 * @return Output filename, or NULL if absent/invalid.
 */
const char* parser_get_output_file(const parsed_command_t *cmd, int stage_idx) {
	if (cmd == NULL || stage_idx < 0 || stage_idx >= cmd->stage_count) {
		return NULL;
	}

	return cmd->stages[stage_idx].output_file;
}

/**
 * @brief Get the stderr redirection filename for a stage.
 *
 * @param cmd Parsed command.
 * @param stage_idx Zero-based stage index.
 * @return Error filename, or NULL if absent/invalid.
 */
const char* parser_get_error_file(const parsed_command_t *cmd, int stage_idx) {
	if (cmd == NULL || stage_idx < 0 || stage_idx >= cmd->stage_count) {
		return NULL;
	}

	return cmd->stages[stage_idx].error_file;
}

/**
 * @brief Free a parsed command and every token allocated while parsing it.
 *
 * @param cmd Parsed command to destroy. NULL is accepted.
 */
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

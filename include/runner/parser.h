#ifndef PARSER_H
#define PARSER_H

#define PARSER_MAX_STAGES 16
#define PARSER_MAX_ARGS 32
#define PARSER_INPUT_BUFFER 1024

/**
 * @brief Opaque parsed representation of a user command.
 */
typedef struct parsed_command parsed_command_t;

/**
 * @brief Parse a command string into pipeline stages, argv arrays and redirections.
 *
 * Supported syntax is intentionally limited to the assignment subset:
 * arguments, pipes '|', input '<', output '>' and stderr '2>'.
 *
 * @param input Command string received by the runner.
 * @return Newly allocated parsed command, or NULL on parse/allocation failure.
 */
parsed_command_t* parser_parse(const char *input);

/**
 * @brief Return the number of pipeline stages.
 *
 * @param cmd Parsed command.
 * @return Number of stages, or 0 if cmd is NULL.
 */
int parser_get_stage_count(const parsed_command_t *cmd);

/**
 * @brief Return the argv array of one pipeline stage.
 *
 * @param cmd Parsed command.
 * @param stage_idx Zero-based stage index.
 * @return NULL-terminated argv array, or NULL if the index is invalid.
 */
char** parser_get_stage_argv(const parsed_command_t *cmd, int stage_idx);

/**
 * @brief Return the input redirection file for one stage.
 *
 * @param cmd Parsed command.
 * @param stage_idx Zero-based stage index.
 * @return File path associated with '<', or NULL if absent/invalid.
 */
const char* parser_get_input_file(const parsed_command_t *cmd, int stage_idx);

/**
 * @brief Return the output redirection file for one stage.
 *
 * @param cmd Parsed command.
 * @param stage_idx Zero-based stage index.
 * @return File path associated with '>', or NULL if absent/invalid.
 */
const char* parser_get_output_file(const parsed_command_t *cmd, int stage_idx);

/**
 * @brief Return the stderr redirection file for one stage.
 *
 * @param cmd Parsed command.
 * @param stage_idx Zero-based stage index.
 * @return File path associated with '2>', or NULL if absent/invalid.
 */
const char* parser_get_error_file(const parsed_command_t *cmd, int stage_idx);

/**
 * @brief Free a parsed command and every token owned by it.
 *
 * @param cmd Parsed command to destroy. NULL is accepted.
 */
void parser_destroy(parsed_command_t *cmd);

#endif /* PARSER_H */

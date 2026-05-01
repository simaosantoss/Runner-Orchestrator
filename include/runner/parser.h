#ifndef PARSER_H
#define PARSER_H

#define PARSER_MAX_STAGES 16
#define PARSER_MAX_ARGS 32
#define PARSER_INPUT_BUFFER 1024

typedef struct parsed_command parsed_command_t;

parsed_command_t* parser_parse(const char *input);
int parser_get_stage_count(const parsed_command_t *cmd);
char** parser_get_stage_argv(const parsed_command_t *cmd, int stage_idx);
const char* parser_get_input_file(const parsed_command_t *cmd, int stage_idx);
const char* parser_get_output_file(const parsed_command_t *cmd, int stage_idx);
const char* parser_get_error_file(const parsed_command_t *cmd, int stage_idx);
void parser_destroy(parsed_command_t *cmd);

#endif /* PARSER_H */

#ifndef PARSER_H
#define PARSER_H

#define PARSER_MAX_STAGES 16
#define PARSER_MAX_ARGS 32
#define PARSER_INPUT_BUFFER 1024

typedef struct {
	char *argv[PARSER_MAX_ARGS];
} command_stage_t;

typedef struct {
	int num_stages;
	command_stage_t stages[PARSER_MAX_STAGES];
	char buffer[PARSER_INPUT_BUFFER];
} parsed_command_t;

void parse_command(const char *input, parsed_command_t *parsed);

#endif /* PARSER_H */

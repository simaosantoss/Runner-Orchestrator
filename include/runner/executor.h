#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

/**
 * @brief Execute a parsed command pipeline in the runner process.
 *
 * The function creates one child process per pipeline stage, connects stages
 * with anonymous pipes, applies redirections with dup2(), executes commands
 * with execvp(), and waits for all children.
 *
 * @param cmd Parsed command returned by parser_parse().
 * @return 0 if every child exits successfully, -1 otherwise.
 */
int execute_pipeline(parsed_command_t *cmd);

#endif /* EXECUTOR_H */

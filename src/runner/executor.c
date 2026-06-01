#include "executor.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @brief Execute all stages of a parsed pipeline.
 *
 * For each stage, this function creates a child process with fork(), connects
 * pipeline endpoints with pipe()/dup2(), applies file redirections with
 * open()/dup2(), executes the program with execvp(), and waits for all child
 * processes before returning.
 *
 * @param cmd Parsed command produced by parser_parse().
 * @return 0 if all children exit with status 0, -1 otherwise.
 */
int execute_pipeline(parsed_command_t *cmd) {
	int num_stages;
	int prev_read_fd;
	int children_created;
	int any_child_failed;

	if (cmd == NULL) {
		return -1;
	}

	num_stages = parser_get_stage_count(cmd);
	if (num_stages <= 0) {
		return -1;
	}

	prev_read_fd = -1;
	children_created = 0;
	any_child_failed = 0;

	for (int i = 0; i < num_stages; i++) {
		int pipefd[2] = { -1, -1 };
		pid_t pid;

		if (i < num_stages - 1) {
			if (pipe(pipefd) == -1) {
				perror("pipe");
				if (prev_read_fd != -1) {
					close(prev_read_fd);
				}
				for (int j = 0; j < children_created; j++) {
					wait(NULL);
				}
				return -1;
			}
		}

		fflush(stdout);
		pid = fork();
		if (pid < 0) {
			perror("fork");
			if (pipefd[0] != -1) {
				close(pipefd[0]);
			}
			if (pipefd[1] != -1) {
				close(pipefd[1]);
			}
			if (prev_read_fd != -1) {
				close(prev_read_fd);
			}
			for (int j = 0; j < children_created; j++) {
				wait(NULL);
			}
			return -1;
		}

		if (pid == 0) {
			char **argv;
			const char *in_file;
			const char *out_file;
			const char *err_file;

			if (prev_read_fd != -1) {
				if (dup2(prev_read_fd, STDIN_FILENO) == -1) {
					perror("dup2");
					_exit(1);
				}
			}

			if (i < num_stages - 1) {
				if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
					perror("dup2");
					_exit(1);
				}
			}

			if (prev_read_fd != -1) {
				close(prev_read_fd);
			}
			if (pipefd[0] != -1) {
				close(pipefd[0]);
			}
			if (pipefd[1] != -1) {
				close(pipefd[1]);
			}

			argv = parser_get_stage_argv(cmd, i);
			if (argv == NULL || argv[0] == NULL) {
				_exit(1);
			}

			in_file = parser_get_input_file(cmd, i);
			out_file = parser_get_output_file(cmd, i);
			err_file = parser_get_error_file(cmd, i);

			if (in_file != NULL) {
				int fd_in = open(in_file, O_RDONLY);
				if (fd_in == -1) {
					perror("open");
					_exit(1);
				}

				if (dup2(fd_in, STDIN_FILENO) == -1) {
					perror("dup2");
					close(fd_in);
					_exit(1);
				}

				close(fd_in);
			}

			if (out_file != NULL) {
				int fd_out = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (fd_out == -1) {
					perror("open");
					_exit(1);
				}

				if (dup2(fd_out, STDOUT_FILENO) == -1) {
					perror("dup2");
					close(fd_out);
					_exit(1);
				}

				close(fd_out);
			}

			if (err_file != NULL) {
				int fd_err = open(err_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (fd_err == -1) {
					perror("open");
					_exit(1);
				}

				if (dup2(fd_err, STDERR_FILENO) == -1) {
					perror("dup2");
					close(fd_err);
					_exit(1);
				}

				close(fd_err);
			}

			execvp(argv[0], argv);
			perror("execvp");
			_exit(1);
		}

		children_created++;

		if (prev_read_fd != -1) {
			close(prev_read_fd);
		}

		if (i < num_stages - 1) {
			close(pipefd[1]);
			prev_read_fd = pipefd[0];
		} else {
			prev_read_fd = -1;
		}
	}

	for (int i = 0; i < children_created; i++) {
		int status;

		if (wait(&status) == -1) {
			perror("wait");
			return -1;
		}

		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			any_child_failed = 1;
		}
	}

	return any_child_failed ? -1 : 0;
}

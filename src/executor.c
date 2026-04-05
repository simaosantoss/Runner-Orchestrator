#include "executor.h"

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

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

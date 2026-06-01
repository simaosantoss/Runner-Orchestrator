# 🧭 Runner-Orchestrator

## 📌 About

**Runner-Orchestrator** is a multi-runner command orchestration environment developed for the Operating Systems course, a 2nd-year, 2nd-semester course in the Software Engineering degree at the University of Minho, academic year 2025/2026.

The goal was to build a Linux/POSIX system where multiple users submit shell-like commands through independent `runner` processes, while a central `controller` schedules those requests, enforces a configurable parallelism limit, keeps track of running and waiting commands, and shuts down in a controlled way.

The original assignment statement is available in [`assignment.pdf`](./assignment.pdf).

The submitted project report is available in [`report.pdf`](./report.pdf).

Both documents are written in Portuguese, as this was an academic project developed and evaluated in Portuguese.

This repository is especially focused on Operating Systems concepts:

- process creation and replacement with `fork()` and `execvp()`;
- inter-process communication with named pipes/FIFOs;
- anonymous pipes for command pipelines;
- file descriptor redirection with `dup2()`;
- direct use of POSIX system calls instead of shell delegation;
- scheduling policies, queues, concurrency limits, and controlled shutdown;
- automated validation of IPC, parsing, scheduling, and stress scenarios.

## 🧠 What This Project Demonstrates

This is not just a command runner. It is a small orchestration system that exercises the core primitives expected in an Operating Systems project.

| Area | What was implemented |
|------|----------------------|
| Process management | `fork()`, `execvp()`, `wait()`, child process execution, pipeline process creation |
| IPC | public controller FIFO, private runner FIFOs, fixed-size protocol messages |
| Pipes | named pipes for runner-controller communication, anonymous pipes for `\|` |
| File descriptors | `open()`, `read()`, `write()`, `close()`, `dup2()` |
| Scheduling | FCFS, Random, and Fair user-based scheduling |
| Concurrency | configurable maximum number of commands running in parallel |
| State tracking | running queue, waiting queue, command identifiers, user identifiers |
| Persistence | command completion log with turnaround time |
| Parsing | manual parser for `>`, `2>`, `<`, and `\|`, including operators attached to arguments |
| Testing | Bash test scripts for parsing, policies, IPC stress, and scheduling behavior |

## 🧩 System Overview

The system is composed of two executables:

| Program | Role |
|---------|------|
| `controller` | Central scheduler. Receives requests, keeps state, authorizes execution, logs completed commands, and handles shutdown. |
| `runner` | User-facing client. Submits commands, waits for authorization, executes the command locally, and notifies completion. |

The key design decision is that the **controller schedules but does not execute commands**. Execution happens inside each `runner`, after receiving authorization from the controller.

This keeps responsibilities separated:

- the `controller` manages policy, queues, and global state;
- the `runner` handles command parsing, process creation, pipes, redirections, and command output;
- command output naturally appears in the user's terminal because the executed child processes inherit the runner's standard descriptors.

## 🏗️ Architecture

```text
User terminal
    |
    | ./bin/runner -e 1 "grep system /etc/passwd | wc -l > out.txt"
    v
runner process
    |
    | SUBMIT(user_id, command_id, command)
    v
/tmp/server_fifo
    |
    v
controller process
    |
    | scheduling policy + parallelism limit
    v
/tmp/runner_<pid>
    |
    | ACK
    v
runner executes command locally
    |
    | fork + pipe + dup2 + execvp + wait
    v
DONE notification
    |
    v
controller logs completion time and starts next waiting job
```

## 🔗 IPC Protocol

The `runner` and `controller` communicate using fixed-size `RpcMessage` structures through FIFOs.

There are two kinds of named pipes:

| FIFO | Created by | Used for |
|------|------------|----------|
| `/tmp/server_fifo` | `controller` | Public FIFO where all runners send requests |
| `/tmp/runner_<pid>` | each `runner` | Private FIFO where the controller replies to that runner |

The protocol uses message types such as:

| Message | Direction | Purpose |
|---------|-----------|---------|
| `SUBMIT` | runner -> controller | Submit a command request |
| `ACK` | controller -> runner | Authorize command execution |
| `DONE` | runner -> controller | Notify command completion |
| `STATUS_REQ` | runner -> controller | Request current running/waiting state |
| `STATUS_RESP` | controller -> runner | Send one status response chunk |
| `STATUS_END` | controller -> runner | Mark the end of a status response |
| `SHUTDOWN_REQ` | runner -> controller | Request controlled shutdown |
| `SHUTDOWN_ACK` | controller -> runner | Confirm shutdown completion or reject new work after shutdown starts |

The message payload size is deliberately bounded so each `RpcMessage` can be sent with a single `write()` call and remain within the atomic write guarantees associated with `PIPE_BUF`.

## 🎛️ Supported Runner Modes

```bash
./bin/runner -e <user-id> "<command>"
./bin/runner -c
./bin/runner -s
```

| Mode | Meaning |
|------|---------|
| `-e` | Submit a command and execute it only after controller authorization |
| `-c` | Query commands currently executing and waiting |
| `-s` | Request controlled controller shutdown |

## ⚖️ Scheduling Policies

The controller supports three scheduling policies.

| Policy | Behavior | Why it matters |
|--------|----------|----------------|
| `fcfs` | First-Come, First-Served. Jobs are authorized in arrival order. | Simple, deterministic baseline. |
| `random` | Selects a random waiting job when a slot becomes available. | Useful as an experimental comparison policy. |
| `fair` | Alternates between users with pending work. | Prevents one user from monopolizing the queue with many early submissions. |

The `fair` policy is **non-preemptive**: it does not suspend or resume already running commands. It only decides which waiting command should receive the next authorization.

## ⚙️ Command Execution

Command execution happens inside the `runner`, after an `ACK` from the controller.

The runner:

1. parses the command string manually;
2. splits pipelines into stages;
3. creates anonymous pipes for `|`;
4. forks one child process per pipeline stage;
5. applies redirections with `open()` and `dup2()`;
6. executes each program with `execvp()`;
7. waits for all children with `wait()`;
8. sends `DONE` to the controller.

No `system()` call is used, and commands are not delegated to `bash` or another shell.

## 🧵 Supported Operators

The manual parser supports the operators required by the assignment:

| Operator | Meaning | Implementation |
|----------|---------|----------------|
| `>` | Redirect standard output | `open(..., O_WRONLY \| O_CREAT \| O_TRUNC)` + `dup2(..., STDOUT_FILENO)` |
| `2>` | Redirect standard error | `open(..., O_WRONLY \| O_CREAT \| O_TRUNC)` + `dup2(..., STDERR_FILENO)` |
| `<` | Redirect standard input | `open(..., O_RDONLY)` + `dup2(..., STDIN_FILENO)` |
| `\|` | Pipeline between commands | `pipe()` + `fork()` + `dup2()` |

Operators can appear attached to arguments, for example:

```bash
./bin/runner -e 1 "cat<input.txt|wc -l>out.txt"
```

The parser does not try to implement a complete Bash grammar. It intentionally implements the subset required by the project statement.

## 🗂️ Project Structure

```text
Makefile
README.md
assignment.pdf
report.pdf

include/
  common/
    protocol.h              Message types, FIFO paths, RpcMessage structure
    ipc.h                   FIFO helper API
  controller/
    controller_handlers.h   Controller state and message handling API
    scheduler.h             Job queues and scheduling policy API
  runner/
    runner_cli.h            Runner CLI modes and operations
    parser.h                Command parser API
    executor.h              Pipeline executor API

src/
  common/
    ipc.c                   mkfifo/open/read/write/close helpers
  controller/
    controller.c            Controller main loop
    controller_handlers.c   SUBMIT, DONE, STATUS, SHUTDOWN handling
    scheduler.c             Linked-list queues and FCFS/Random/Fair logic
  runner/
    runner.c                Runner entry point
    runner_cli.c            -e, -c, -s implementation
    parser.c                Manual parser for pipelines and redirections
    executor.c              fork/execvp/pipe/dup2/wait execution engine

tests/
  test_parsing.sh           Redirections, pipes, attached operators
  test_fcfs.sh              FCFS and parallelism limit
  test_random.sh            Random scheduling behavior
  test_fair.sh              Fair scheduling by user
  test_stress.sh            IPC and status response stress test
```

## 🛠️ Build

The project is written in C and uses a simple `Makefile`.

```bash
make
```

The executables are generated in:

```text
bin/controller
bin/runner
```

Clean generated files, logs, and runtime FIFOs:

```bash
make clean
```

## 🚀 Usage

Start the controller:

```bash
./bin/controller <parallel-commands> <sched-policy>
```

Example:

```bash
./bin/controller 2 fcfs
```

Submit commands from other terminals:

```bash
./bin/runner -e 1 "echo hello"
./bin/runner -e 2 "sleep 10"
./bin/runner -e 3 "cat</etc/passwd|wc -l>count.txt"
```

Query state:

```bash
./bin/runner -c
```

Example output:

```text
---
Executing
user-id 1 - command-id 12345
user-id 2 - command-id 12346
---
Scheduled
user-id 3 - command-id 12347
```

Request controlled shutdown:

```bash
./bin/runner -s
```

The controller only exits after all running and waiting commands have completed.

## 🖥️ Quick Demo

Terminal 1:

```bash
make
./bin/controller 2 fcfs
```

Terminal 2:

```bash
./bin/runner -e 1 "sleep 6" &
./bin/runner -e 2 "sleep 6" &
./bin/runner -e 3 "sleep 6" &
./bin/runner -e 4 "sleep 6" &
```

Terminal 3:

```bash
./bin/runner -c
./bin/runner -s
```

Pipeline and redirection examples:

```bash
./bin/controller 2 fcfs
./bin/runner -e 1 "echo OlaMundo>out.txt"
cat out.txt

./bin/runner -e 1 "cat</etc/passwd|wc -l>count.txt"
cat count.txt

./bin/runner -e 1 "ls missing_file 2>err.txt"
cat err.txt

./bin/runner -s
```

Fair scheduling example:

```bash
./bin/controller 1 fair
```

```bash
./bin/runner -e 1 "sleep 8" &
./bin/runner -e 1 "sleep 8" &
./bin/runner -e 1 "sleep 8" &
./bin/runner -e 2 "sleep 8" &
./bin/runner -e 3 "sleep 8" &
./bin/runner -c
./bin/runner -s
```

With `fair`, users 2 and 3 should not remain behind all remaining commands from user 1.

## ✅ Automated Tests

Run all tests individually:

```bash
./tests/test_parsing.sh
./tests/test_fcfs.sh
./tests/test_random.sh
./tests/test_fair.sh
./tests/test_stress.sh
```

The tests validate:

- command parsing and attached operators;
- `>`, `2>`, `<`, and `|`;
- FCFS ordering and concurrency limit;
- Random scheduling without lost or duplicated commands;
- Fair scheduling between users;
- large status responses with 150 concurrent submissions;
- controlled shutdown behavior.

The stress test starts a controller with a parallelism limit of 10 and submits 150 `sleep` commands. Shutdown may appear to wait, but this is expected: the controller only exits after all pending and running jobs are completed.

## 🧾 Persistent Log

The controller writes completed commands to `log.txt`.

Each line contains:

```text
user-id <id> - command-id <id> - <duration> ms
```

The measured duration is the command turnaround time from the moment the controller receives the submission until it receives the final `DONE` notification.

## 🔍 Validation Summary

The project was validated through:

- manual multi-terminal execution;
- automated Bash scripts;
- concurrency tests with multiple background runners;
- parsing tests for redirections and pipelines;
- stress tests for FIFO message integrity and large `-c` responses;
- policy tests for `fcfs`, `random`, and `fair`;
- inspection of `log.txt` turnaround-time behavior.

## 📚 What I Learned

This project was mainly an exercise in understanding how UNIX-like systems expose powerful low-level mechanisms through small, composable system calls.

The most important takeaways were:

- how independent processes can coordinate through named pipes;
- why anonymous pipes are ideal for parent-child pipeline execution;
- how file descriptors make redirection possible;
- why `dup2()` is central to shell-like behavior;
- how `fork()` and `execvp()` separate process creation from program replacement;
- why fixed-size protocol messages simplify IPC;
- how scheduling choices affect user-perceived fairness;
- how shutdown protocols need to account for pending work;
- how to validate concurrent behavior with repeatable scripts.

## 🤖 AI-Assisted Development

AI tools were used only to help generate the Bash test scripts. This usage was allowed by the assignment rules, provided that it did not replace the implementation of the core Operating Systems concepts.

The main project logic, architecture, use of system calls, IPC protocol, parser, executor, scheduler, and controller behavior were implemented and understood by the group.

All generated test scripts were reviewed, executed, and validated by the group. The prompts used for the test scripts are included in the submitted Portuguese report.

## ⚠️ Limitations

The parser intentionally supports only the subset required by the assignment:

- simple arguments;
- `<`;
- `>`;
- `2>`;
- `|`.

It does not implement full Bash behavior such as:

- complex quoting and escaping;
- variable expansion;
- globbing such as `*.c`;
- `>>` or `2>>`;
- `&&`, `||`, or `;`;
- `2>&1`.

This was a deliberate scope decision: the project focus was Operating Systems primitives, not building a complete shell.

## 👥 Authors

- [Simão Santos](https://github.com/simaosantoss)

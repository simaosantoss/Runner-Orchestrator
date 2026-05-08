#!/usr/bin/env bash

set -euo pipefail

VERDE='\033[0;32m'
AZUL='\033[0;34m'
AMARELO='\033[1;33m'
SEM_COR='\033[0m'

controller_pid=""
runner_pids=()
STATUS_FILE="tmp/fcfs_status.txt"

info() {
	printf "${AZUL}[*] %s${SEM_COR}\n" "$1"
}

success() {
	printf "${VERDE}[+] %s${SEM_COR}\n" "$1"
}

fail() {
	printf "${AMARELO}[!] %s${SEM_COR}\n" "$1"
	if [ -f "$STATUS_FILE" ]; then
		cat "$STATUS_FILE"
	fi
	exit 1
}

cleanup() {
	for pid in "${runner_pids[@]:-}"; do
		if kill -0 "$pid" 2>/dev/null; then
			kill "$pid" 2>/dev/null || true
			wait "$pid" 2>/dev/null || true
		fi
	done

	if [ -n "$controller_pid" ] && kill -0 "$controller_pid" 2>/dev/null; then
		kill "$controller_pid" 2>/dev/null || true
		wait "$controller_pid" 2>/dev/null || true
	fi

	rm -f "$STATUS_FILE"
}

trap cleanup EXIT INT TERM

extract_section_users() {
	local section="$1"
	local file="$2"

	awk -v wanted="$section" '
		/^Executing$/ { current = "Executing"; next }
		/^Scheduled$/ { current = "Scheduled"; next }
		/^---$/ { next }
		current == wanted && /^user-id / { print $2 }
	' "$file" | tr '\n' ' ' | sed 's/[[:space:]]*$//'
}

assert_equals() {
	local expected="$1"
	local actual="$2"
	local message="$3"

	if [ "$actual" != "$expected" ]; then
		fail "$message Esperado: '$expected'. Obtido: '${actual:-vazio}'."
	fi
}

assert_user_set() {
	local expected="$1"
	local actual="$2"
	local message="$3"
	local normalized

	normalized=$(printf "%s\n" "$actual" | tr ' ' '\n' | sed '/^$/d' | sort -n | tr '\n' ' ' | sed 's/[[:space:]]*$//')
	assert_equals "$expected" "$normalized" "$message"
}

assert_log_wave() {
	local range="$1"
	local expected="$2"
	local actual

	actual=$(sed -n "$range" log.txt | awk '{ print $2 }' | sort -n | tr '\n' ' ' | sed 's/[[:space:]]*$//')
	assert_equals "$expected" "$actual" "Ordem de conclusão no log não respeitou a vaga esperada ($range)."
}

start_controller() {
	local policy="$1"

	rm -f log.txt "$STATUS_FILE"
	./bin/controller 2 "$policy" &
	controller_pid=$!
	sleep 1

	if ! kill -0 "$controller_pid" 2>/dev/null; then
		fail "O controller não iniciou corretamente para a política $policy."
	fi
}

submit_staggered_jobs() {
	runner_pids=()

	for user_id in 1 2 3 4 5; do
		./bin/runner -e "$user_id" "sleep 3" >/dev/null 2>&1 &
		runner_pids+=("$!")
		sleep 0.2
	done

	sleep 0.5
}

shutdown_and_wait() {
	./bin/runner -s >/dev/null
	wait "$controller_pid"
	controller_pid=""

	for pid in "${runner_pids[@]}"; do
		wait "$pid"
	done
	runner_pids=()
}

info "A iniciar compilacao do projeto..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1
success "Compilacao concluida com sucesso."

info "Teste FCFS: validar ordem de execução e fila com submissões controladas."
start_controller "fcfs"
submit_staggered_jobs

./bin/runner -c > "$STATUS_FILE"
cat "$STATUS_FILE"

EXEC_USERS=$(extract_section_users "Executing" "$STATUS_FILE")
SCHEDULED_USERS=$(extract_section_users "Scheduled" "$STATUS_FILE")

assert_user_set "1 2" "$EXEC_USERS" "FCFS devia ter os dois primeiros utilizadores em execução."
assert_equals "3 4 5" "$SCHEDULED_USERS" "FCFS devia manter a fila por ordem de chegada."

shutdown_and_wait

LOG_LINES=$(grep -c '^user-id ' log.txt || true)
assert_equals "5" "$LOG_LINES" "O log FCFS devia conter exatamente 5 comandos concluídos."
assert_log_wave '1,2p' "1 2"
assert_log_wave '3,4p' "3 4"
assert_log_wave '5p' "5"
success "Teste FCFS validou ordem de fila e conclusão por vagas."

rm -f "$STATUS_FILE"
success "Execucao automatizada FCFS concluida com validações."

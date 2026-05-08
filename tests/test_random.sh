#!/usr/bin/env bash

set -euo pipefail

VERDE='\033[0;32m'
AZUL='\033[0;34m'
AMARELO='\033[1;33m'
SEM_COR='\033[0m'

controller_pid=""
runner_pids=()
RUNS=6
JOBS=5
RANDOM_ORDER=""

info() {
	printf "${AZUL}[*] %s${SEM_COR}\n" "$1"
}

success() {
	printf "${VERDE}[+] %s${SEM_COR}\n" "$1"
}

fail() {
	printf "${AMARELO}[!] %s${SEM_COR}\n" "$1"
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
}

trap cleanup EXIT INT TERM

assert_equals() {
	local expected="$1"
	local actual="$2"
	local message="$3"

	if [ "$actual" != "$expected" ]; then
		fail "$message Esperado: '$expected'. Obtido: '${actual:-vazio}'."
	fi
}

run_random_round() {
	local round="$1"
	local order
	local sorted_users

	rm -f log.txt
	runner_pids=()

	./bin/controller 1 random &
	controller_pid=$!
	sleep 1

	if ! kill -0 "$controller_pid" 2>/dev/null; then
		fail "O controller random não iniciou corretamente na ronda $round."
	fi

	for user_id in $(seq 1 "$JOBS"); do
		./bin/runner -e "$user_id" "sleep 1" >/dev/null 2>&1 &
		runner_pids+=("$!")
		sleep 0.1
	done

	./bin/runner -s >/dev/null
	wait "$controller_pid"
	controller_pid=""

	for pid in "${runner_pids[@]}"; do
		wait "$pid"
	done
	runner_pids=()

	order=$(awk '/^user-id / { print $2 }' log.txt | tr '\n' ' ' | sed 's/[[:space:]]*$//')
	sorted_users=$(printf "%s\n" "$order" | tr ' ' '\n' | sed '/^$/d' | sort -n | tr '\n' ' ' | sed 's/[[:space:]]*$//')

	assert_equals "1 2 3 4 5" "$sorted_users" "A ronda random $round perdeu ou duplicou comandos."

	printf "%b[*] Ronda random %d terminou por ordem: %s%b\n" "$AZUL" "$round" "$order" "$SEM_COR"
	RANDOM_ORDER="$order"
}

info "A iniciar compilacao do projeto..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1
success "Compilacao concluida com sucesso."

info "A executar $RUNS rondas random com limite de concorrência 1."

orders_seen=""
non_fcfs_seen=0

for round in $(seq 1 "$RUNS"); do
	run_random_round "$round"
	order="$RANDOM_ORDER"
	orders_seen="${orders_seen}${order}
"

	if [ "$order" != "1 2 3 4 5" ]; then
		non_fcfs_seen=1
	fi
done

unique_orders=$(printf "%s" "$orders_seen" | sed '/^$/d' | sort -u | wc -l | tr -d '[:space:]')

if [ "$non_fcfs_seen" -ne 1 ]; then
	fail "Em $RUNS rondas, a política random produziu sempre a ordem FCFS. Isto é possível por acaso, mas é muito improvável; repetir o teste ou rever queue_dequeue_random."
fi

if [ "$unique_orders" -lt 2 ]; then
	fail "A política random produziu sempre a mesma ordem em $RUNS rondas."
fi

rm -f log.txt
success "Teste RANDOM validou ordens não determinísticas sem perdas nem duplicados."

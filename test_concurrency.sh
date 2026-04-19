#!/usr/bin/env bash

set -euo pipefail

VERDE='\033[0;32m'
AZUL='\033[0;34m'
AMARELO='\033[1;33m'
SEM_COR='\033[0m'

info() {
	printf "${AZUL}[*] %s${SEM_COR}\n" "$1"
}

success() {
	printf "${VERDE}[+] %s${SEM_COR}\n" "$1"
}

warn() {
	printf "${AMARELO}[TESTE %s] %s${SEM_COR}\n" "$1" "$2"
}

info "A iniciar compilacao do projeto..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1
success "Compilacao concluida com sucesso."

warn "1" "Configuracao: Limite de Concorrencia = 2 | Politica = FCFS"
./bin/controller 2 fcfs &
controller_pid=$!

sleep 1

info "A despachar 5 trabalhos concorrentes (sleep 4)..."
runner_pids=()
for user_id in 1 2 3 4 5; do
	./bin/runner -e "$user_id" "sleep 4" &
	runner_pids+=("$!")
done

sleep 0.5

info "A obter o estado do sistema (-c):"
./bin/runner -c

info "A iniciar encerramento suave (-s)..."
./bin/runner -s

wait "$controller_pid"
for pid in "${runner_pids[@]}"; do
	wait "$pid"
done

success "Teste 1 concluido."

warn "2" "Configuracao: Limite de Concorrencia = 2 | Politica = RANDOM"
./bin/controller 2 random &
controller_pid=$!

sleep 1

info "A despachar 5 trabalhos concorrentes (sleep 4)..."
runner_pids=()
for user_id in 1 2 3 4 5; do
	./bin/runner -e "$user_id" "sleep 4" &
	runner_pids+=("$!")
done

sleep 0.5

info "A obter o estado do sistema (-c):"
./bin/runner -c

info "A iniciar encerramento suave (-s)..."
./bin/runner -s

wait "$controller_pid"
for pid in "${runner_pids[@]}"; do
	wait "$pid"
done

success "Teste 2 concluido."
success "Execucao automatizada concluida. As metricas foram registadas no ficheiro log.txt."

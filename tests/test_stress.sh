#!/usr/bin/env bash

set -u

Verde='\033[0;32m'
Azul='\033[0;34m'
Amarelo='\033[1;33m'
SemCor='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

SNAPSHOT="tmp/stress_snapshot.txt"
SERVER_PID=""
RUNNER_PIDS=""
CLEANED_UP=0
TOTAL_JOBS=150

cleanup() {
    if [ "$CLEANED_UP" -eq 1 ]; then
        return
    fi
    CLEANED_UP=1

    rm -f "$SNAPSHOT"

    for pid in $RUNNER_PIDS; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
        fi
    done

    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
    fi
}

trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM

printf "%b[*] A executar compilação silenciosa...%b\n" "$Azul" "$SemCor"
if ! make clean >/dev/null 2>&1; then
    printf "%b[!] A limpeza da compilação falhou.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

if ! make >/dev/null 2>&1; then
    printf "%b[!] A compilação falhou.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

printf "%b[*] A iniciar servidor em background...%b\n" "$Azul" "$SemCor"
./bin/controller 10 fcfs &
SERVER_PID=$!

sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    printf "%b[!] O servidor não foi iniciado corretamente.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

for tentativa in $(seq 1 20); do
    if [ -p /tmp/server_fifo ]; then
        break
    fi
    sleep 0.1
done

if [ ! -p /tmp/server_fifo ]; then
    printf "%b[!] O FIFO do servidor não ficou disponível.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

printf "%b[*] A injetar carga extrema: %d comandos concorrentes....%b\n" "$Azul" "$TOTAL_JOBS" "$SemCor"
for i in $(seq 1 "$TOTAL_JOBS"); do
    ./bin/runner -e 1 "sleep 5" >/dev/null 2>&1 &
    RUNNER_PIDS="$RUNNER_PIDS $!"
    if [ $((i % 10)) -eq 0 ]; then
        sleep 0.05
    fi
done

sleep 2

printf "%b[*] A capturar estado do sistema com -c....%b\n" "$Azul" "$SemCor"
JOB_LINES=0
for tentativa in $(seq 1 10); do
    if ! ./bin/runner -c > "$SNAPSHOT"; then
        printf "%b[!] A consulta de estado falhou.%b\n" "$Amarelo" "$SemCor"
        exit 1
    fi

    JOB_LINES=$(grep -c '^user-id ' "$SNAPSHOT" || true)
    if [ "$JOB_LINES" -eq "$TOTAL_JOBS" ]; then
        break
    fi

    sleep 0.5
done

LINHAS=$(wc -l < "$SNAPSHOT")
LINHAS=${LINHAS//[[:space:]]/}
printf "%b[+] O snapshot tem %s linhas e %s comandos, demonstrando que a consulta suporta filas grandes.%b\n" "$Verde" "$LINHAS" "$JOB_LINES" "$SemCor"

if [ "$JOB_LINES" -ne "$TOTAL_JOBS" ]; then
    printf "%b[!] Esperava encontrar %d comandos no snapshot.%b\n" "$Amarelo" "$TOTAL_JOBS" "$SemCor"
    exit 1
fi

printf "%b[*] A iniciar encerramento suave (-s)...%b\n" "$Azul" "$SemCor"
if ! ./bin/runner -s; then
    printf "%b[!] O pedido de encerramento falhou.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

wait "$SERVER_PID" 2>/dev/null
SERVER_PID=""

for pid in $RUNNER_PIDS; do
    wait "$pid" 2>/dev/null
done
RUNNER_PIDS=""

rm -f "$SNAPSHOT"

printf "%b[+] Teste de stress concluído com sucesso.%b\n" "$Verde" "$SemCor"

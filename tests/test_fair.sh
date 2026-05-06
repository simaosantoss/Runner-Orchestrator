#!/usr/bin/env bash

set -u

Verde='\033[0;32m'
Azul='\033[0;34m'
Amarelo='\033[1;33m'
SemCor='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT" || exit 1

SERVER_PID=""
RUNNER_PIDS=""
SNAPSHOT="tmp/fair_snapshot.txt"
CLEANED_UP=0

cleanup() {
    if [ "$CLEANED_UP" -eq 1 ]; then
        return
    fi
    CLEANED_UP=1

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

    rm -f "$SNAPSHOT"
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

printf "%b[*] A iniciar controller com política fair e limite 1...%b\n" "$Azul" "$SemCor"
./bin/controller 1 fair &
SERVER_PID=$!

sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    printf "%b[!] O servidor não foi iniciado corretamente.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

printf "%b[*] A submeter vários comandos do utilizador 1...%b\n" "$Azul" "$SemCor"
./bin/runner -e 1 "sleep 2" >/dev/null 2>&1 &
RUNNER_PIDS="$RUNNER_PIDS $!"
sleep 0.1
./bin/runner -e 1 "sleep 2" >/dev/null 2>&1 &
RUNNER_PIDS="$RUNNER_PIDS $!"
sleep 0.1
./bin/runner -e 1 "sleep 2" >/dev/null 2>&1 &
RUNNER_PIDS="$RUNNER_PIDS $!"
sleep 0.1

printf "%b[*] A submeter um comando do utilizador 2 depois dos pedidos do utilizador 1...%b\n" "$Azul" "$SemCor"
./bin/runner -e 2 "sleep 2" >/dev/null 2>&1 &
RUNNER_PIDS="$RUNNER_PIDS $!"

sleep 0.5

printf "%b[*] A capturar estado com -c...%b\n" "$Azul" "$SemCor"
if ! ./bin/runner -c > "$SNAPSHOT"; then
    printf "%b[!] A consulta de estado falhou.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

cat "$SNAPSHOT"

FIRST_SCHEDULED_USER=$(awk '
    /^---$/ && seen_scheduled == 1 { exit }
    /^Scheduled$/ { seen_scheduled = 1; next }
    seen_scheduled == 1 && /^user-id / { print $2; exit }
' "$SNAPSHOT")

USER2_EXECUTING=$(awk '
    /^---$/ && seen_executing == 1 { exit }
    /^Executing$/ { seen_executing = 1; next }
    seen_executing == 1 && /^user-id 2 / { found = 1 }
    END { print found + 0 }
' "$SNAPSHOT")

if [ "$USER2_EXECUTING" -ne 1 ] && [ "$FIRST_SCHEDULED_USER" != "2" ]; then
    printf "%b[!] A política fair deveria ter o utilizador 2 em execução ou como primeiro na fila Scheduled, mas o primeiro Scheduled foi %s.%b\n" "$Amarelo" "${FIRST_SCHEDULED_USER:-nenhum}" "$SemCor"
    exit 1
fi

printf "%b[+] A política fair respeita a alternância: o utilizador 2 não ficou bloqueado atrás dos restantes comandos do utilizador 1.%b\n" "$Verde" "$SemCor"

for pid in $RUNNER_PIDS; do
    wait "$pid" 2>/dev/null
done
RUNNER_PIDS=""

printf "%b[*] A iniciar encerramento suave (-s)...%b\n" "$Azul" "$SemCor"
if ! ./bin/runner -s >/dev/null 2>&1; then
    printf "%b[!] O pedido de encerramento falhou.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

wait "$SERVER_PID" 2>/dev/null
SERVER_PID=""

rm -f "$SNAPSHOT"

printf "%b[+] Teste da política fair concluído com sucesso.%b\n" "$Verde" "$SemCor"

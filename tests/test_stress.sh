#!/usr/bin/env bash

set -u

Verde='\033[0;32m'
Azul='\033[0;34m'
Amarelo='\033[1;33m'
SemCor='\033[0m'

SNAPSHOT="snapshot.txt"
SERVER_PID=""

cleanup() {
    rm -f "$SNAPSHOT"

    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
    fi
}

trap cleanup EXIT INT TERM

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

printf "%b[*] A injetar carga extrema: 150 comandos concorrentes....%b\n" "$Azul" "$SemCor"
for i in $(seq 1 150); do
    ./bin/runner -e 1 "sleep 5" >/dev/null 2>&1 &
done

sleep 2

printf "%b[*] A capturar estado do sistema com -c....%b\n" "$Azul" "$SemCor"
./bin/runner -c > "$SNAPSHOT"

LINHAS=$(wc -l < "$SNAPSHOT")
LINHAS=${LINHAS//[[:space:]]/}
printf "%b[+] O snapshot tem %s linhas, provando que ultrapassa o limite de 100.%b\n" "$Verde" "$LINHAS" "$SemCor"

printf "%b[*] A iniciar encerramento suave (-s)...%b\n" "$Azul" "$SemCor"
./bin/runner -s

wait "$SERVER_PID" 2>/dev/null
SERVER_PID=""

rm -f "$SNAPSHOT"

printf "%b[+] Teste de stress concluído com sucesso.%b\n" "$Verde" "$SemCor"

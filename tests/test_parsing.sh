#!/usr/bin/env bash

set -u

Verde='\033[0;32m'
Azul='\033[0;34m'
Amarelo='\033[1;33m'
SemCor='\033[0m'

SERVER_PID=""
FICHEIROS_TEMP="test_in.txt test_out.txt test_count.txt test_error.txt"

cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
    fi

    rm -f $FICHEIROS_TEMP
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

printf "%b[*] A criar ficheiro temporário de entrada...%b\n" "$Azul" "$SemCor"
echo "Linha 1" > test_in.txt
echo "Linha 2" >> test_in.txt
echo "Linha 3" >> test_in.txt

printf "%b[*] A iniciar servidor em background...%b\n" "$Azul" "$SemCor"
./bin/controller 2 fcfs &
SERVER_PID=$!

sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    printf "%b[!] O servidor não foi iniciado corretamente.%b\n" "$Amarelo" "$SemCor"
    exit 1
fi

printf "%b[*] A testar redirecionamento de saída sem espaços....%b\n" "$Azul" "$SemCor"
./bin/runner -e 1 "echo OlaMundo>test_out.txt"

printf "%b[*] A testar redirecionamento de entrada e pipe sem espaços....%b\n" "$Azul" "$SemCor"
./bin/runner -e 1 "cat<test_in.txt|wc -l>test_count.txt"

sleep 1

printf "%b[+] Conteúdo gerado em test_out.txt:%b\n" "$Verde" "$SemCor"
cat test_out.txt

printf "%b[+] Conteúdo gerado em test_count.txt:%b\n" "$Verde" "$SemCor"
cat test_count.txt

printf "%b[*] A testar redirecionamento de erro (2>)...%b\n" "$Azul" "$SemCor"
./bin/runner -e 1 "ls ficheiro_que_nao_existe 2>test_error.txt"

sleep 1

printf "%b[+] Conteúdo gerado em test_error.txt:%b\n" "$Verde" "$SemCor"
cat test_error.txt

printf "%b[*] A iniciar encerramento suave (-s)...%b\n" "$Azul" "$SemCor"
./bin/runner -s

wait "$SERVER_PID" 2>/dev/null
SERVER_PID=""

rm -f $FICHEIROS_TEMP

printf "%b[+] Teste de parsing concluído com sucesso.%b\n" "$Verde" "$SemCor"

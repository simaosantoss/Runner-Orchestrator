# Orquestração de um Ambiente Multi-Runner (Sistemas Operativos)

## Visão Geral

Este projeto implementa um sistema de orquestração de comandos baseado numa topologia cliente-servidor, composta por múltiplos processos **Runner** e um processo **Controller**. Os utilizadores submetem comandos através do Runner, que comunica com o Controller por FIFOs. O Controller é responsável por escalonar os pedidos recebidos, respeitando o limite de paralelismo configurado, enquanto a execução efetiva dos comandos ocorre no Runner.

## Arquitetura (Highlights)

- **Separação de Execução**: o Runner é responsável por executar os comandos através de `fork`, `execvp`, `pipe`, `dup2` e `wait`, herdando o terminal do utilizador para apresentar o output. O Controller apenas orquestra, escalona e mantém o estado dos comandos.

- **Comunicação IPC via Mensagens Atómicas**: a comunicação entre Runner e Controller é feita através de pipes com nome (FIFOs), usando mensagens de tamanho fixo representadas pela estrutura `RpcMessage`.

- **Motor de Parsing Manual**: o Runner inclui um parser próprio para interpretar comandos com argumentos, pipes e redirecionamentos. São suportados os operadores `>`, `2>`, `<` e `|`, incluindo quando aparecem colados aos argumentos, por exemplo `cat<input.txt|wc -l>out.txt`.

- **Filas Encapsuladas**: o Controller usa estruturas de fila encapsuladas para manter os comandos em execução e os comandos em espera, separando a lógica de escalonamento da lógica de comunicação.

- **Políticas de Escalonamento**: o Controller suporta as políticas `fcfs`, `random` e `fair`. A política `fair` alterna entre utilizadores com comandos pendentes na escolha do próximo comando a autorizar, sem interromper comandos que já estejam em execução.

- **Escalabilidade Dinâmica no `-c`**: a consulta de estado cria snapshots das filas com `malloc` e envia a resposta em múltiplas mensagens `STATUS_RESP`, permitindo listar um número variável de comandos sem depender de um limite fixo pequeno.

## Como Compilar

O projeto inclui uma `Makefile` com os targets pedidos no enunciado.

```bash
make
```

Após a compilação, os executáveis ficam disponíveis em:

```bash
./bin/controller
./bin/runner
```

Para limpar os ficheiros gerados e temporários:

```bash
make clean
```

## Suite de Testes Automatizados

A pasta `tests/` contém scripts para validar automaticamente os principais comportamentos do sistema.

- `test_parsing.sh`: valida o parser e a execução de comandos com redirecionamentos de entrada, saída e erro, e pipes, incluindo operadores colados aos argumentos.

- `test_fcfs.sh`: valida o limite de concorrência do Controller e a ordem de escalonamento da política `fcfs`.

- `test_random.sh`: valida a política `random` em várias rondas, confirmando ordens não determinísticas sem perdas nem duplicados.

- `test_fair.sh`: valida a política `fair`, confirmando que um utilizador que chega depois não fica bloqueado atrás de todos os comandos já pendentes de outro utilizador.

- `test_stress.sh`: submete uma carga elevada de comandos concorrentes para confirmar que o sistema suporta filas grandes e que a consulta `-c` escala dinamicamente.

Para correr os testes:

```bash
./tests/test_parsing.sh
./tests/test_fcfs.sh
./tests/test_random.sh
./tests/test_fair.sh
./tests/test_stress.sh
```

Cada script compila o projeto, inicia o Controller, executa os cenários de teste e termina o serviço de forma controlada.

## Guião de Demonstração (Para a Defesa Ao Vivo)

Este guião demonstra o funcionamento do sistema com vários terminais em paralelo. Antes de começar, compilar o projeto:

```bash
make
```

### Terminal 1: Servidor

Iniciar o Controller com limite de 2 comandos em execução simultânea e política `fcfs`:

```bash
./bin/controller 2 fcfs
```

Este terminal fica bloqueado enquanto o Controller está ativo.

### Terminal 2: Cliente 1

Submeter uma tarefa pesada pelo utilizador 1:

```bash
./bin/runner -e 1 "sleep 10"
```

### Terminal 3: Cliente 2

Submeter uma segunda tarefa pesada pelo utilizador 2:

```bash
./bin/runner -e 2 "sleep 10"
```

### Terminal 4: Cliente 3

Submeter uma terceira tarefa pesada pelo utilizador 3:

```bash
./bin/runner -e 3 "sleep 10"
```

Como o Controller foi iniciado com limite 2, apenas dois comandos devem estar em execução. O terceiro deve ficar em espera até haver uma vaga.

### Terminal 5: Monitorização

Consultar o estado atual do sistema:

```bash
./bin/runner -c
```

O output deve mostrar uma secção `Executing` com até 2 comandos e uma secção `Scheduled` com os comandos ainda em espera.

Exemplo esperado:

```text
---
Executing
user-id 1 - command-id 12345
user-id 2 - command-id 12346
---
Scheduled
user-id 3 - command-id 12347
```

Com a política `fcfs`, os comandos em espera aparecem pela ordem de chegada. Com a política `fair`, a secção `Scheduled` apresenta os comandos pela ordem em que seriam escolhidos, alternando entre utilizadores com comandos pendentes.

### Demonstração de Pipes e Redirecionamentos

Também é possível demonstrar operadores da linha de comandos suportados pelo parser:

```bash
./bin/runner -e 1 "grep system /etc/passwd | wc -l > out.txt"
```

Verificar o resultado:

```bash
cat out.txt
```

Exemplo com operadores colados:

```bash
./bin/runner -e 1 "cat</etc/passwd|wc -l>count.txt"
cat count.txt
```

### Demonstração do Escalonador Aleatório (Random)

Antes de demonstrar a política `random`, terminar o servidor atual caso ainda esteja em execução:

```bash
./bin/runner -s
```

De seguida, iniciar novamente o Controller com limite de 2 comandos em execução simultânea e política `random`:

```bash
./bin/controller 2 random
```

Submeter 4 tarefas rapidamente. Podem ser executadas em terminais separados ou enviadas para background:

```bash
./bin/runner -e 1 "sleep 8"
./bin/runner -e 2 "sleep 8"
./bin/runner -e 3 "sleep 8"
./bin/runner -e 4 "sleep 8"
```

Consultar imediatamente o estado do sistema:

```bash
./bin/runner -c
```

Nesta fase, deverão aparecer até 2 comandos na secção `Executing`, enquanto os restantes comandos deverão aparecer na secção `Scheduled`.

Após a conclusão dos primeiros comandos, uma nova consulta com:

```bash
./bin/runner -c
```

permite demonstrar a aleatoriedade do escalonador, uma vez que o servidor poderá promover o comando 4 antes do comando 3 para a secção de execução.

### Demonstração da Política Fair

Antes de demonstrar a política `fair`, terminar o servidor atual caso ainda esteja em execução:

```bash
./bin/runner -s
```

De seguida, iniciar novamente o Controller com limite de 1 comando em execução simultânea e política `fair`:

```bash
./bin/controller 1 fair
```

Submeter vários comandos do utilizador 1 e depois um comando do utilizador 2:

```bash
./bin/runner -e 1 "sleep 5"
./bin/runner -e 1 "sleep 5"
./bin/runner -e 1 "sleep 5"
./bin/runner -e 2 "sleep 5"
```

Quando houver uma vaga, o Controller deve alternar para o utilizador 2 antes de continuar a autorizar os restantes comandos do utilizador 1. Esta política não usa fatias de tempo nem interrompe processos já iniciados; apenas decide de forma justa qual é o próximo comando em espera a passar para execução.

### Comando Final: Encerramento

Para pedir ao Controller que termine:

```bash
./bin/runner -s
```

O Controller só termina depois de concluir os comandos pendentes e em execução. O Runner de shutdown apresenta:

```text
[runner] sent shutdown notification
[runner] waiting for controller to shutdown...
[runner] controller exited.
```

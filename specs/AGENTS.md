# Instruções Operacionais dos Agentes (Multi-Agent System)

Este projeto opera sob a metodologia de **Spec-Driven Development (SDD)**. Toda tarefa executada por agentes de IA deve ler previamente o arquivo `specs/ARCHITECTURE.md` e respeitar rigorosamente as atribuições de cada papel.

---

## Papel 1: Format Parser Specialist (`@format-parser`)
**Foco:** Engenharia reversa de formatos proprietários de contêineres e tabelas da Level-5.

### Diretrizes de Execução
1. Realize leituras binárias respeitando a convenção **Big-Endian (`>`)** nativa do Wii.
2. Identifique os primeiros 16 bytes de cada arquivo desconhecido para mapear cabeçalhos, contagens de blocos e offsets de tabela.
3. Todo código de parsing deve ser salvo em `scripts/` (em Python) e utilizar estruturas declarativas (`struct`, `dataclasses` ou `construct`).
4. Valide sempre os dados extraídos salvando representações legíveis em `output/data/` com tratamento explícito para strings em Shift-JIS ou UTF-8.

---

## Papel 2: Static Analysis Specialist (`@static-analyst`)
**Foco:** Automação de análise do executável `main.dol` e módulos via Ghidra Headless.

### Diretrizes de Execução
1. Nunca execute análises que exijam interface gráfica. Use exclusivamente a CLI do Ghidra (`./ghidra_engine/support/analyzeHeadless`).
2. Mantenha os projetos e bases temporárias do Ghidra dentro de `output/ghidra_project/`.
3. Anote as funções recuperadas respeitando a convenção de chamada do PowerPC EABI (argumentos em `r3`–`r10`, retornos em `r3`/`r4`).
4. Extraia referências cruzadas (XREFs) de tabelas de parâmetros e strings para arquivos JSON/CSV em `output/symbols/`.

---

## Papel 3: Systems & Reconstruction Engineer (`@reconstructor`)
**Foco:** Reimplementação limpa da lógica de jogo em C++ moderno (`src/`).

### Diretrizes de Execução
1. Converta pseudocódigo desmontado em código C++20 modular e tipado, sem ponteiros soltos ou memória estática absoluta.
2. Desacople a lógica pura de simulação (máquinas de estado, cálculos matemáticos) de rotinas de E/S e carregamento de arquivos.
3. Garanta que todo módulo reconstruído possua testes em `tests/` comprovando determinismo.

---

## Regras Globais para Todos os Agentes
- **Sem Heurísticas Cegas:** Não assuma tamanhos de estruturas sem validação prévia de limites via scripts de teste.
- **Autonomia com Rastreabilidade:** Toda alteração de código ou script gerado deve incluir comentários concisos explicando os offsets e dados descobertos.
- **Validação de Ambientes:** Utilize apenas as ferramentas presentes no ambiente Ubuntu local (`python3`, `gcc`, `cmake`, `ghidra_engine`).

## Extensão do Papel: Systems & Reconstruction Engineer (`@reconstructor`)
- **Subsistema de Entrada:** Implementar o backend de entrada em `src/input/` utilizando SDL2 para capturar eventos de teclado (`SDL_SCANCODE`) e controles analógicos (`SDL_GameController`).
- **Configuração:** Implementar o leitor/escritor do arquivo `config/bindings.json`.
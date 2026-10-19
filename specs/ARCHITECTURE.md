# Arquitetura do Sistema: Engenharia Reversa e Reconstrução do Strikers 2013

## 1. Visão Geral
Este documento define os contratos de dados, restrições arquiteturais e o pipeline automatizado de engenharia reversa para o jogo *Inazuma Eleven GO Strikers 2013* (Nintendo Wii).

O objetivo final é a documentação exaustiva das estruturas internas e a reconstrução portável da lógica do motor de jogo para Linux x86_64 utilizando C++20.

---

## 2. Plataforma de Origem vs. Plataforma Alvo

| Propriedade | Plataforma de Origem (Wii) | Plataforma Alvo (Reconstrução) |
| :--- | :--- | :--- |
| **CPU / ISA** | PowerPC 750CL (Broadway) 32-bit | x86_64 / ARM64 moderno |
| **Endianness** | Big-Endian (`>`) | Little-Endian (`<`) |
| **Executável Base** | `extracted/files/sys/main.dol` | Binário ELF nativo via CMake |
| **Formatos de Dados**| Contêineres Level-5 (`.pack`, `.bin`, `.g4b`) | Schemas descritivos / JSON estruturado |

---

## 3. Estrutura do Workspace

```text
strikers-ai-core/
├── specs/
│   ├── ARCHITECTURE.md      # Especificação técnica mestre (SDD)
│   └── AGENTS.md            # Diretrizes operacionais e papéis dos agentes
├── extracted/               # Conteúdo bruto descompactado da ISO
│   ├── files/sys/main.dol   # Executável compilado em PowerPC
│   └── files/               # Contêineres de assets, scripts e tabelas
├── ghidra_engine/           # Instalação local do Ghidra (modo headless)
├── scripts/                 # Automações em Python (parsers, inspeção e bridges)
├── output/                  # Artefatos gerados (dumps, tabelas de símbolos, JSONs)
│   ├── symbols/             # Mapeamentos exportados do Ghidra
│   └── data/                # Tabelas decodificadas da Level-5
└── src/                     # Código C++20 reconstruído

## 4. Pipeline de Desenvolvimento (Fases)
```text
Fase 1: Mapeamento e Decodificação de Contêineres

    Entrada: Arquivos brutos em extracted/files/.

    Procedimento: Varredura de cabeçalhos (magic bytes), detecção de esquemas de compressão (ex: variantes de LZSS, RLE, zlib) e análise de tabelas de índices.

    Critério de Aceite (SDD): Todo parser Python deve ser determinístico, ler Big-Endian e exportar as tabelas em formatos estruturados sob output/data/.

Fase 2: Análise Estática e Descompilação (Ghidra Headless)

    Entrada: extracted/files/sys/main.dol.

    Procedimento: Execução automatizada via ./ghidra_engine/support/analyzeHeadless para gerar tabelas de símbolos, descompilar funções de controle de fluxo e mapear referências a strings e tabelas de dados.

    Critério de Aceite (SDD): Exportação de relatórios legíveis em output/symbols/ documentando assinaturas de rotinas críticas (fórmulas de duelo, física da bola, máquinas de estado).

Fase 3: Reconstrução e Verificação em C++20

    Entrada: Schemas decodificados em output/data/ e lógica descompilada em output/symbols/.

    Procedimento: Implementação da lógica de domínio em C++ limpo, eliminando acoplamentos de registradores PowerPC em favor de funções puras e testáveis.

    Critério de Aceite (SDD): Testes automatizados que validam a equivalência numérica com as regras do jogo original.

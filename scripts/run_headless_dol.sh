#!/usr/bin/env bash
# ==============================================================================
# Script Utilitario (@static-analyst): Analise Headless do main.dol via Ghidra
# Jogo Alvo: Inazuma Eleven GO Strikers 2013 (Nintendo Wii)
# Arquitetura: PowerPC 750CL (Broadway) 32-bit Big-Endian (PowerPC:BE:32:default)
#
# Diretrizes atendidas (specs/AGENTS.md & specs/ARCHITECTURE.md):
# 1. Utiliza exclusivamente ./ghidra_engine/support/analyzeHeadless (100% CLI).
# 2. Importa extracted/files/sys/main.dol para output/ghidra_project/.
# 3. Remapeia as secoes do cabeçalho DOL (.text0..1, .data0..7, .sbss, .bss) e
#    extrai todas as strings legiveis (ASCII / Shift-JIS), tabelas de ponteiros
#    e referencias cruzadas (XREFs) respeitando a EABI do PowerPC (r3-r10, r2, r13).
# 4. Exporta o relatorio estruturado em output/symbols/strings_xref.json.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

ANALYZE_HEADLESS="./ghidra_engine/support/analyzeHeadless"
DOL_PATH="extracted/files/sys/main.dol"
GHIDRA_PROJECT_DIR="output/ghidra_project"
GHIDRA_PROJECT_NAME="strikers2013_main_dol"
PROCESSOR_ID="PowerPC:BE:32:default"
COMPILER_SPEC="default"
OUTPUT_JSON="output/symbols/strings_xref.json"
HEADLESS_LOG="${GHIDRA_PROJECT_DIR}/headless_analysis.log"
SCRIPT_LOG="${GHIDRA_PROJECT_DIR}/headless_script.log"

if [[ ! -x "${ANALYZE_HEADLESS}" ]]; then
    echo "[ERRO] Binario analyzeHeadless nao encontrado ou sem permissao de execucao em: ${ANALYZE_HEADLESS}" >&2
    exit 1
fi

if [[ ! -f "${DOL_PATH}" ]]; then
    echo "[ERRO] Executavel main.dol nao encontrado em: ${DOL_PATH}" >&2
    exit 1
fi

mkdir -p "${GHIDRA_PROJECT_DIR}"
mkdir -p "$(dirname "${OUTPUT_JSON}")"

# Configura diretorio de usuario isolado dentro de output/ghidra_project/ para que
# o LaunchSupport do Ghidra nao solicite caminho do JDK interativamente via TTY
GHIDRA_USER_HOME="${PROJECT_ROOT}/${GHIDRA_PROJECT_DIR}/.ghidra_user"
GHIDRA_CONFIG_DIR="${GHIDRA_USER_HOME}/.config/ghidra/ghidra_12.1.4_PUBLIC"
mkdir -p "${GHIDRA_CONFIG_DIR}" "${GHIDRA_USER_HOME}/.cache" "${GHIDRA_USER_HOME}/.local/share"

if [[ -z "${JAVA_HOME:-}" ]]; then
    JAVAC_BIN="$(command -v javac || true)"
    if [[ -n "${JAVAC_BIN}" ]]; then
        RESOLVED_JAVAC="$(readlink -f "${JAVAC_BIN}")"
        JAVA_HOME="$(cd "$(dirname "${RESOLVED_JAVAC}")/.." && pwd)"
    fi
fi

if [[ -n "${JAVA_HOME:-}" ]]; then
    echo "${JAVA_HOME}" > "${GHIDRA_CONFIG_DIR}/java_home.save"
fi

export HOME="${GHIDRA_USER_HOME}"
export XDG_CONFIG_HOME="${GHIDRA_USER_HOME}/.config"
export XDG_CACHE_HOME="${GHIDRA_USER_HOME}/.cache"
export XDG_DATA_HOME="${GHIDRA_USER_HOME}/.local/share"
export GHIDRA_HEADLESS_MAXMEM="${GHIDRA_HEADLESS_MAXMEM:-4G}"
export JAVA_TOOL_OPTIONS="-Duser.home=${GHIDRA_USER_HOME} -Djava.awt.headless=true"

echo "======================================================================"
echo "[@static-analyst] Iniciando Analise Estática Headless (Ghidra CLI)"
echo "  Binario Alvo : ${DOL_PATH}"
echo "  Processador  : ${PROCESSOR_ID} (PowerPC 750CL 32-bit Big-Endian)"
echo "  Projeto Temp : ${GHIDRA_PROJECT_DIR}/${GHIDRA_PROJECT_NAME}"
echo "  Saida JSON   : ${OUTPUT_JSON}"
echo "======================================================================"

"${ANALYZE_HEADLESS}" \
    "${GHIDRA_PROJECT_DIR}" \
    "${GHIDRA_PROJECT_NAME}" \
    -import "${DOL_PATH}" \
    -processor "${PROCESSOR_ID}" \
    -cspec "${COMPILER_SPEC}" \
    -overwrite \
    -noanalysis \
    -scriptPath "${PROJECT_ROOT}/scripts" \
    -postScript ExtractDolStringsXref.java "${PROJECT_ROOT}/${OUTPUT_JSON}" \
    -log "${PROJECT_ROOT}/${HEADLESS_LOG}" \
    -scriptlog "${PROJECT_ROOT}/${SCRIPT_LOG}" \
    < /dev/null

echo "======================================================================"
echo "[@static-analyst] Execucao finalizada com sucesso."
echo "  Relatorio exportado: ${OUTPUT_JSON}"
echo "======================================================================"

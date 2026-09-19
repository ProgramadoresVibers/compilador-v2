#!/usr/bin/env bash
# Testa o parser C com os casos .c e compara a saída com o resultado esperado.
# Uso: ./testar_parser_c.sh [diretorio_dos_testes] [codigo_do_parser]
# Sem argumentos, usa parser.c e tests da pasta deste script.

set -u
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="${1:-$SCRIPT_DIR/tests}"
PARSER_SOURCE="${2:-$SCRIPT_DIR/parser.c}"
KEEP_TMP="${KEEP_TMP:-0}"

fail() { echo "ERRO: $1" >&2; exit 1; }
COMPILER="$(command -v "${CC:-gcc}")" || fail 'GCC não encontrado no PATH.'
# No Git Bash, as DLLs do GCC precisam ter prioridade sobre as DLLs do Git.
case "$OSTYPE" in
    msys*|cygwin*) export PATH="$(dirname -- "$COMPILER"):$PATH" ;;
esac

if [[ ! -d "$TEST_DIR" ]]; then
    echo "ERRO: diretório de testes não encontrado: $TEST_DIR" >&2
    exit 2
fi
if [[ ! -f "$PARSER_SOURCE" ]]; then
    echo "ERRO: código do parser não encontrado: $PARSER_SOURCE" >&2
    exit 2
fi

TEST_DIR="$(cd -- "$TEST_DIR" && pwd)"
PARSER_DIR="$(cd -- "$(dirname -- "$PARSER_SOURCE")" && pwd)"
PARSER_SOURCE="$PARSER_DIR/$(basename -- "$PARSER_SOURCE")"

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/parser-c-XXXXXX")"
cleanup() {
    if [[ "$KEEP_TMP" == 1 ]]; then
        printf 'Arquivos temporários: %s\n' "$TMP_DIR"
    else
        rm -rf -- "$TMP_DIR"
    fi
}
trap cleanup EXIT
PARSER="$TMP_DIR/parser.exe"
"$COMPILER" -Wall -Wextra -std=c11 "$PARSER_SOURCE" -o "$PARSER" || fail 'a compilação falhou.'

find_expected() {
    local source="$1" base stem candidate dir
    base="$(basename "$source")"; stem="${base%.*}"; dir="$(dirname "$source")"
    for candidate in \
        "${source}.expected" "${source}.out" "${source}.ast" "${source}.resultado" "${source}.txt" \
        "$dir/$stem.expected" "$dir/$stem.out" "$dir/$stem.ast" "$dir/$stem.resultado" "$dir/$stem.txt" \
        "$TEST_DIR/esperados/$stem.txt" "$TEST_DIR/expected/$stem.txt" "${source/codigo.c/ast.esperada.txt}"; do
        [[ -f "$candidate" ]] && { printf '%s\n' "$candidate"; return 0; }
    done
    return 1
}

normalize() {
    sed -e 's/\r$//' -e 's/[[:space:]]//g' "$1" | awk 'NF || n { print; n=NF }'
}

mapfile -d '' CASES < <(find "$TEST_DIR" -type f -name '*.c' -print0 | sort -z)
TOTAL=${#CASES[@]}
[[ "$TOTAL" -gt 0 ]] || { echo "ERRO: nenhum caso .c encontrado" >&2; exit 2; }

PASS=0; FAIL=0; SYNTAX_ERRORS=0; RUNTIME_ERRORS=0; MISSING_EXPECTED=0; INDEX=0
printf 'Parser: %s\nDiretório: %s\nCasos encontrados: %d\n\n' "$PARSER" "$TEST_DIR" "$TOTAL"

for source in "${CASES[@]}"; do
    INDEX=$((INDEX + 1)); name="${source##*/}"
    out="$TMP_DIR/$INDEX.stdout"; err="$TMP_DIR/$INDEX.stderr"
    if expected="$(find_expected "$source")"; then :; else
        printf '[%02d/%02d] %-36s SEM ESPERADO\n' "$INDEX" "$TOTAL" "$name"
        MISSING_EXPECTED=$((MISSING_EXPECTED + 1)); FAIL=$((FAIL + 1)); continue
    fi

    status=0
    "$PARSER" "$source" >"$out" 2>"$err" || status=$?
    if grep -Eiq 'erro[[:space:]_-]*(sint[aá]tico|de sintaxe)|syntax[[:space:]_-]*error|parse[[:space:]_-]*error|NÃO HÁ AST' "$out" "$err"; then
        SYNTAX_ERRORS=$((SYNTAX_ERRORS + 1))
    elif [[ "$status" -ne 0 ]]; then
        RUNTIME_ERRORS=$((RUNTIME_ERRORS + 1))
    fi

    actual="$TMP_DIR/$INDEX.actual"; cat "$out" > "$actual"
    if [[ -s "$err" ]]; then cat "$err" >> "$actual"; fi
    normalize "$expected" > "$TMP_DIR/$INDEX.expected.norm"
    normalize "$actual" > "$TMP_DIR/$INDEX.actual.norm"
    if [[ "$status" -le 1 ]] && cmp -s "$TMP_DIR/$INDEX.expected.norm" "$TMP_DIR/$INDEX.actual.norm"; then
        PASS=$((PASS + 1)); printf '[%02d/%02d] %-36s OK\n' "$INDEX" "$TOTAL" "$name"
    else
        FAIL=$((FAIL + 1)); printf '[%02d/%02d] %-36s FALHOU (saída diferente)\n' "$INDEX" "$TOTAL" "$name"
        if [[ "$KEEP_TMP" == 1 ]]; then diff -u "$TMP_DIR/$INDEX.expected.norm" "$TMP_DIR/$INDEX.actual.norm" | sed 's/^/    /' || true; fi
    fi
done

printf '\nResumo\n-------\n'
printf 'Total de casos:       %d\nAprovados:            %d\nReprovados:           %d\nErros sintáticos:     %d\nErros de execução:    %d\nEsperados ausentes:   %d\n' \
    "$TOTAL" "$PASS" "$FAIL" "$SYNTAX_ERRORS" "$RUNTIME_ERRORS" "$MISSING_EXPECTED"

if [[ "$FAIL" -eq 0 && "$TOTAL" -eq 50 ]]; then
    echo 'Resultado final: TODOS OS 50 CASOS PASSARAM'; exit 0
elif [[ "$FAIL" -eq 0 ]]; then
    echo "Resultado final: todos os casos encontrados passaram, mas foram encontrados $TOTAL em vez de 50"; exit 0
else
    echo 'Resultado final: há casos que precisam ser corrigidos'; exit 1
fi

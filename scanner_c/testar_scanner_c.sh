#!/usr/bin/env bash
# Testa um analisador léxico escrito em C com arquivos .c e .minic.
#
# Uso:
#   ./testar_scanner_c.sh [scanner.c] [pasta-de-testes]
#
# O scanner deve aceitar:
#   ./scanner --tokens arquivo.c
#   ./scanner --tokens arquivo.minic
#
# A saída padrão deve conter um objeto JSON por linha, compatível com os
# arquivos <entrada>.expected.jsonl. Diagnósticos esperados podem ser
# armazenados em <entrada>.errors.jsonl.


set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SCANNER_SOURCE="${1:-$SCRIPT_DIR/scanner.c}"
TESTS_DIR="${2:-$SCRIPT_DIR/tests}"
KEEP_TMP="${KEEP_TMP:-0}"
TOTAL=0
PASS=0
FAIL=0
WARN=0

fail() { echo "ERRO: $1" >&2; exit 1; }

COMPILER="$(command -v "${CC:-gcc}")" || fail 'GCC não encontrado no PATH.'
# No Git Bash, as DLLs do GCC precisam ter prioridade sobre as DLLs do Git.
case "$OSTYPE" in
    msys*|cygwin*) export PATH="$(dirname -- "$COMPILER"):$PATH" ;;
esac
[[ -f "$SCANNER_SOURCE" ]] || fail "código do scanner não encontrado: $SCANNER_SOURCE"
[[ -d "$TESTS_DIR" ]] || fail "diretório de testes não encontrado: $TESTS_DIR"
SCANNER_SOURCE="$(cd -- "$(dirname -- "$SCANNER_SOURCE")" && pwd)/$(basename -- "$SCANNER_SOURCE")"
TESTS_DIR="$(cd -- "$TESTS_DIR" && pwd)"

# No Windows, Python pode estar disponível como python ou pelo launcher py.
PYTHON=()
for candidate in python3 python py; do
    python_command=("$candidate")
    [[ "$candidate" != py ]] || python_command+=(-3)
    if "${python_command[@]}" -c 'import sys; sys.exit(sys.version_info < (3, 6))' >/dev/null 2>&1; then
        PYTHON=("${python_command[@]}")
        break
    fi
done
(( ${#PYTHON[@]} > 0 )) || fail 'Python 3 não encontrado (python3, python ou py -3).'

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/scanner-c-XXXXXX")"
cleanup() {
    if [[ "$KEEP_TMP" == 1 ]]; then
        printf 'Arquivos temporários: %s\n' "$TMP_DIR"
    else
        rm -rf -- "$TMP_DIR"
    fi
}
trap cleanup EXIT
SCANNER_BINARY="$TMP_DIR/scanner.exe"

compare_jsonl() {
    local actual="$1" expected_tokens="$2"
    "${PYTHON[@]}" -X utf8 - "$actual" "$expected_tokens" <<'PY'
import json
import sys
from pathlib import Path

actual_path, expected_tokens_path = sys.argv[1:]

def read_jsonl(path):
    values = []
    if not Path(path).exists():
        return values
    for number, line in enumerate(Path(path).read_text(encoding='utf-8').splitlines(), 1):
        if not line.strip():
            continue
        try:
            values.append(json.loads(line))
        except json.JSONDecodeError as exc:
            print(f'FALHA: JSON inválido em {path}:{number}: {exc}')
            raise SystemExit(1)
    return values

def first_difference(got, want):
    for i in range(max(len(got), len(want))):
        a = got[i] if i < len(got) else '<ausente>'
        b = want[i] if i < len(want) else '<a mais>'
        if a != b:
            print(f'  primeira diferença na posição {i + 1}:')
            print(f'    esperado: {json.dumps(b, ensure_ascii=False)}')
            print(f'    obtido:   {json.dumps(a, ensure_ascii=False)}')
            return

produced = read_jsonl(actual_path)
expected_tokens = read_jsonl(expected_tokens_path)
produced_tokens = [x for x in produced if 'token' in x]
ok = True

if produced_tokens != expected_tokens:
    print(f'FALHA: sequência de tokens diferente.')
    first_difference(produced_tokens, expected_tokens)
    ok = False
if ok:
    print(f'OK: {len(produced_tokens)} token(s).')
raise SystemExit(0 if ok else 1)
PY
}

run_case() {
    local input="$1" expected="$2" label="${1#./}"
    TOTAL=$((TOTAL + 1))
    printf '\n================================================================\nCaso: %s\n' "$label"
    printf 'Comando: %s --tokens %s\n' "$SCANNER_BINARY" "$input"
    printf 'Resultado esperado: %s\n' "$expected"

    local status=0
    (cd -- "$TMP_DIR" && "$SCANNER_BINARY" --tokens "$input") > "$TMP_DIR/output.jsonl" || status=$?

    if compare_jsonl "$TMP_DIR/output.jsonl" "$expected"; then
        echo 'RESULTADO: OK'
        PASS=$((PASS + 1))
    else
        echo 'RESULTADO: FALHOU — tokens diferentes do esperado'
        FAIL=$((FAIL + 1))
    fi
    if (( status != 0 )); then
        WARN=$((WARN + 1))
        echo "Aviso: o scanner terminou com código $status."
    fi
}

printf '%s\n' '== Compilando o analisador léxico =='
"$COMPILER" -Wall -Wextra -std=c11 "$SCANNER_SOURCE" -o "$SCANNER_BINARY" || fail 'a compilação falhou.'
printf 'Executável gerado: %s\n' "$SCANNER_BINARY"

mapfile -d '' -t expected_files < <(find "$TESTS_DIR" -type f -name '*.expected.jsonl' -print0 | sort -z)
(( ${#expected_files[@]} > 0 )) || { echo "ERRO: nenhum resultado esperado para .c ou .minic foi encontrado." >&2; exit 2; }

for expected in "${expected_files[@]}"; do
    input="${expected%.expected.jsonl}"
    if [[ -f "$input" || -f "$input.minic" ]]; then
        if [[ -f "$input" ]]; then
            run_case "$input" "$expected"
        fi
        if [[ -f "$input.minic" ]]; then
            run_case "$input.minic" "$expected"
        fi
    else
        echo "AVISO: entrada correspondente não encontrada para $expected" >&2
        WARN=$((WARN + 1))
    fi
done

printf '\n================================================================\n'
printf 'Resumo: %d OK, %d falharam, %d avisos, %d casos verificados.\n' "$PASS" "$FAIL" "$WARN" "$TOTAL"
(( TOTAL > 0 && FAIL == 0 && WARN == 0 ))

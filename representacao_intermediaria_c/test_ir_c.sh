#!/usr/bin/env bash
# Uso: ./test_ir_c.sh representacao_intermediaria.c ./tests
set -euo pipefail
if [[ $# -ne 2 || ! -f "$1" || ! -d "$2" ]]; then
    echo "Uso: $0 representacao_intermediaria.c ./tests" >&2
    exit 2
fi
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -z "${PYTHON_BIN:-}" ]]; then
    for candidate in python3 python; do
        if "$candidate" -c 'import sys; assert sys.version_info >= (3, 10)' >/dev/null 2>&1; then
            PYTHON_BIN="$candidate"
            break
        fi
    done
fi
: "${PYTHON_BIN:?Defina PYTHON_BIN com o interpretador Python 3.10 ou superior}"
CC="${CC:-gcc}"

COMPILER="$(command -v "$CC")"

case "$OSTYPE" in
    msys*|cygwin*)
        export PATH="$(dirname -- "$COMPILER"):$PATH"
        ;;
esac

CFLAGS="${CFLAGS:--std=c11 -Wall -Wextra -O2}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf -- "$TMP_DIR"' EXIT
EXECUTABLE="$TMP_DIR/minic-ir.exe"
# A divisão de CFLAGS é intencional para aceitar várias opções.
"$CC" $CFLAGS "$1" -o "$EXECUTABLE"
export PYTHONDONTWRITEBYTECODE=1
"$PYTHON_BIN" "$SCRIPT_DIR/../validar_ir.py" --compiler "$EXECUTABLE" --suite "$2"

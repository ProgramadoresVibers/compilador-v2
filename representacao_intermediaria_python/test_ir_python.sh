#!/usr/bin/env bash
# Uso: ./test_ir_python.sh representacao_intermediaria.py ./tests
set -euo pipefail
if [[ $# -ne 2 || ! -f "$1" || ! -d "$2" ]]; then
    echo "Uso: $0 representacao_intermediaria.py ./tests" >&2
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
export PYTHONDONTWRITEBYTECODE=1
"$PYTHON_BIN" "$SCRIPT_DIR/../validar_ir.py" --python --compiler "$1" --suite "$2"

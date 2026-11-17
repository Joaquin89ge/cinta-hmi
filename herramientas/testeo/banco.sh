#!/usr/bin/env bash
# idf.py de la prueba de banco, en test/. Mismos argumentos que idf.py.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../contenedor.sh
source "${DIR}/../contenedor.sh"

if [[ ! -f "${CINTA_RAIZ}/test/CMakeLists.txt" ]]; then
    echo "No está la prueba de banco en test/." >&2
    exit 1
fi

contenedor_correr -w /trabajo/test -- idf.py "$@"

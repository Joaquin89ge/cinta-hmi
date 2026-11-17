#!/usr/bin/env bash
# Pasa los argumentos a idf.py dentro del contenedor, en el directorio esp32/.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../contenedor.sh
source "${DIR}/../contenedor.sh"

if [[ ! -f "${CINTA_RAIZ}/esp32/CMakeLists.txt" ]]; then
    echo "Todavía no hay firmware en esp32/. Cuando esté el CMakeLists.txt, este script corre idf.py." >&2
    exit 1
fi

contenedor_correr -w /trabajo/esp32 -- idf.py "$@"

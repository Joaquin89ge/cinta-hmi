#!/usr/bin/env bash
# Compila la HMI para esta PC con el preset pc. El binario queda en rpi/build/pc/.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=contenedor.sh
source "${DIR}/contenedor.sh"

if [[ ! -f "${CINTA_RAIZ}/rpi/CMakeLists.txt" ]]; then
    echo "Todavía no está la HMI en rpi/. Cuando exista el CMakeLists.txt, este script la compila para la PC." >&2
    exit 1
fi

contenedor_correr -w /trabajo/rpi -- \
    bash -c 'cmake --preset pc && cmake --build --preset pc'

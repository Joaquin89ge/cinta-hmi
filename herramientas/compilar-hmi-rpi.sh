#!/usr/bin/env bash
# Cruza la HMI para la Raspberry con el preset rpi. El binario estático queda en rpi/build/rpi/.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=contenedor.sh
source "${DIR}/contenedor.sh"

if [[ ! -f "${CINTA_RAIZ}/rpi/CMakeLists.txt" ]]; then
    echo "Todavía no está la HMI en rpi/. Cuando exista el CMakeLists.txt, este script la cruza para la Raspberry." >&2
    exit 1
fi

contenedor_correr -w /trabajo/rpi -- \
    bash -c 'cmake --preset rpi && cmake --build --preset rpi'

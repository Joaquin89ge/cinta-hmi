#!/usr/bin/env bash
# Abre la HMI de la PC. xWindow y el binario corren en el mismo contenedor
# para compartir la memoria del framebuffer, y la ventana sale en este escritorio.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=contenedor.sh
source "${DIR}/contenedor.sh"

bin="${CINTA_RAIZ}/rpi/build/pc/cinta_hmi"
if [[ ! -x "${bin}" ]]; then
    echo "No está ${bin}. Primero corré herramientas/compilar-hmi-pc.sh." >&2
    exit 1
fi

contenedor_correr -w /trabajo/rpi -- \
    bash -c './build/pc/cinta_hmi "$@"' bash "$@"

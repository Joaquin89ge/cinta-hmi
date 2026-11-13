#!/usr/bin/env bash
# Abre la HMI de la PC. xWindow y el binario corren en el mismo contenedor
# para compartir la memoria del framebuffer, y la ventana sale en este escritorio.
# El main reintenta shmget; este script no espera esa memoria.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=contenedor.sh
source "${DIR}/contenedor.sh"

bin="${CINTA_RAIZ}/rpi/build/pc/cinta_hmi"
ventana="${CINTA_RAIZ}/copiaTotalMateria/material/Tablero/xWindow"
if [[ ! -x "${bin}" ]]; then
    echo "No está ${bin}. Primero corré herramientas/compilar-hmi-pc.sh." >&2
    exit 1
fi
if [[ ! -x "${ventana}" ]]; then
    echo "No está ${ventana}." >&2
    exit 1
fi

# El FIFO se crea en el /tmp del host y se monta adentro. Así un echo
# desde otra terminal de esta máquina llega a la HMI.
for fifo in /tmp/fifo_nav /tmp/fifo_datos /tmp/fifo_cmd; do
    if [[ ! -p "${fifo}" ]]; then
        mkfifo "${fifo}"
    fi
done

contenedor_correr \
    -v /tmp/fifo_nav:/tmp/fifo_nav \
    -v /tmp/fifo_datos:/tmp/fifo_datos \
    -v /tmp/fifo_cmd:/tmp/fifo_cmd \
    -w /trabajo/copiaTotalMateria/material/Tablero -- \
    bash -c '
        set -euo pipefail
        for fifo in /tmp/fifo_nav /tmp/fifo_datos /tmp/fifo_cmd; do
            if [[ ! -p "${fifo}" ]]; then
                mkfifo "${fifo}"
            fi
        done
        ./xWindow 512 512 >/tmp/xwindow-hmi.log 2>&1 &
        xw=$!
        trap '\''kill "${xw}" 2>/dev/null || true; wait "${xw}" 2>/dev/null || true'\'' EXIT
        /trabajo/rpi/build/pc/cinta_hmi shared-fb
    '

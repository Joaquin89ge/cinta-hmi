#!/usr/bin/env bash
# Abre la HMI de la PC. xWindow y el binario corren en el mismo contenedor
# para compartir la memoria del framebuffer, y la ventana sale en este escritorio.
# El main reintenta shmget; este script no espera esa memoria.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../contenedor.sh
source "${DIR}/../contenedor.sh"

bin="${CINTA_RAIZ}/rpi/build/pc/cinta_hmi"
ventana="${CINTA_RAIZ}/copiaTotalMateria/material/Tablero/xWindow"
if [[ ! -x "${bin}" ]]; then
    echo "No está ${bin}. Primero corré herramientas/testeo/compilar-hmi-pc.sh." >&2
    exit 1
fi
if [[ ! -x "${ventana}" ]]; then
    echo "No está ${ventana}." >&2
    exit 1
fi

# La X de xWindow no cerraba el proceso, y cada apertura nueva se sumaba.
# Una sola HMI puede leer los FIFO: la anterior se termina acá.
if pgrep -x cinta_hmi >/dev/null || pgrep -x xWindow >/dev/null; then
    echo "Había una HMI de antes en segundo plano. La cierro." >&2
    pkill -x cinta_hmi 2>/dev/null || true
    pkill -x xWindow 2>/dev/null || true
    sleep 0.3
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
        # El binario de la cátedra ignora la X. La biblioteca hace que ese clic salga.
        so=/trabajo/rpi/build/pc/xwindow-cierre.so
        src=/trabajo/herramientas/testeo/xwindow-cierre.c
        if [[ ! -e "${so}" || "${src}" -nt "${so}" ]]; then
            mkdir -p /trabajo/rpi/build/pc
            gcc -shared -fPIC -o "${so}" "${src}" -lX11
        fi
        LD_PRELOAD="${so}" ./xWindow 512 512 >/tmp/xwindow-hmi.log 2>&1 &
        xw=$!
        /trabajo/rpi/build/pc/cinta_hmi shared-fb &
        hmi=$!
        # Si se cierra la ventana, o si la HMI termina, caen los dos.
        cerrar() {
            kill "${xw}" "${hmi}" 2>/dev/null || true
            wait "${xw}" "${hmi}" 2>/dev/null || true
        }
        trap cerrar EXIT INT TERM
        wait -n "${xw}" "${hmi}" || true
    '

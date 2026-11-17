#!/usr/bin/env bash
# En la Raspberry, con la HMI y el puente ya compilados. No compila.
# Crea los tres FIFO, abre el puente en el UART de la Pi y la HMI en el monitor.
# Ctrl+C cierra los dos.
#
# Si el repo está en la SD, usa rpi/build/rpi/cinta_hmi y puente/build/rpi/puente.
# Si están en otro lado:
#   correr-en-pi.sh /ruta/cinta_hmi /ruta/puente
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RAIZ="$(cd "${DIR}/../.." && pwd)"

hmi="${RAIZ}/rpi/build/rpi/cinta_hmi"
puente="${RAIZ}/puente/build/rpi/puente"
if [[ $# -ge 1 ]]; then
    hmi="$1"
fi
if [[ $# -ge 2 ]]; then
    puente="$2"
fi

serie="/dev/ttyS0"
pantalla="/dev/fb0"

if [[ ! -x "${hmi}" ]]; then
    echo "No está la HMI: ${hmi}" >&2
    exit 1
fi
if [[ ! -x "${puente}" ]]; then
    echo "No está el puente: ${puente}" >&2
    exit 1
fi

echo "FIFO de navegación, datos y órdenes"
for fifo in /tmp/fifo_nav /tmp/fifo_datos /tmp/fifo_cmd; do
    if [[ ! -p "${fifo}" ]]; then
        mkfifo "${fifo}"
    fi
done

echo "Puente en ${serie}"
"${puente}" "${serie}" &
pid_puente=$!
sleep 0.3
if ! kill -0 "${pid_puente}" 2>/dev/null; then
    echo "El puente no quedó abierto. Mirá ${serie}." >&2
    exit 1
fi

# Al salir de la HMI, o con Ctrl+C, cierro el puente.
cerrar() {
    kill "${pid_puente}" 2>/dev/null || true
    wait "${pid_puente}" 2>/dev/null || true
}
trap cerrar EXIT INT TERM

echo "HMI en el monitor"
"${hmi}" "${pantalla}"

#!/usr/bin/env bash
# Comprueba imagen, cross-compiler, puerto de la ESP32 y la ventana del ejemplo.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=contenedor.sh
source "${DIR}/contenedor.sh"

echo "== imagen y ESP-IDF =="
contenedor_correr -- idf.py --version

echo "== cross aarch64 estático =="
contenedor_correr -- bash -c '
    set -euo pipefail
    echo "int main(){return 0;}" | /usr/bin/aarch64-linux-gnu-g++ -x c++ -static -o /tmp/hola_rpi -
    if ldd /tmp/hola_rpi >/tmp/ldd.out 2>&1; then
        echo "El binario de la Raspberry no quedó estático:" >&2
        cat /tmp/ldd.out >&2
        exit 1
    fi
    /usr/bin/aarch64-linux-gnu-readelf -h /tmp/hola_rpi | grep -E "Class|Machine"
'

echo "== ESP32 en ${CINTA_PUERTO} =="
if [[ ! -e "${CINTA_PUERTO}" ]]; then
    echo "No aparece la ESP32 en ${CINTA_PUERTO}." >&2
    exit 1
fi
contenedor_correr -- esptool.py -p "${CINTA_PUERTO}" chip-id

echo "== ventana del ejemplo Tablero =="
"${DIR}/probar-tablero.sh" 5

echo "Entorno listo."

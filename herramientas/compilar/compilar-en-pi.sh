#!/usr/bin/env bash
# Compila la HMI y el puente en la Raspberry, con el g++ de esa máquina.
# No usa Docker ni CMake: el EOS trae g++ y no trae cmake.
# Salida: rpi/build/en-pi/cinta_hmi y rpi/build/en-pi/puente
set -euo pipefail
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
salida="${RAIZ}/rpi/build/en-pi"
mkdir -p "${salida}"

g++ -std=c++17 -O2 -Wall -Wextra -static -pthread \
    -I "${RAIZ}/rpi/terceros" \
    -I "${RAIZ}/esp32/main/domain" \
    -o "${salida}/cinta_hmi" \
    "${RAIZ}/rpi/main.cpp" \
    "${RAIZ}/rpi/pantalla.cpp" \
    "${RAIZ}/rpi/fifos.cpp" \
    "${RAIZ}/rpi/terceros/Consolas_24B.cpp" \
    "${RAIZ}/esp32/main/domain/protocol.cpp"

g++ -std=c++17 -O2 -Wall -Wextra -static \
    -I "${RAIZ}/esp32/main/domain" \
    -o "${salida}/puente" \
    "${RAIZ}/puente/main.cpp"

echo "Listo: ${salida}/cinta_hmi y ${salida}/puente"
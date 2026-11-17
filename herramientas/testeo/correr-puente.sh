#!/usr/bin/env bash
# Compila el puente y lo abre contra el USB de la ESP32.
# Corre en esta máquina, no en el contenedor: así ve /dev/ttyACM0 y los FIFO de /tmp.
# En la Raspberry no uses este script: herramientas/correr/correr-en-pi.sh abre /dev/ttyS0.
# No tener herramientas/testeo/monitor.sh abierto: el puerto lo usa un solo programa.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RAIZ="$(cd "${DIR}/../.." && pwd)"
puerto="${1:-${CINTA_PUERTO:-/dev/ttyACM0}}"

cmake -S "${RAIZ}/puente" -B "${RAIZ}/puente/build"
cmake --build "${RAIZ}/puente/build"
exec "${RAIZ}/puente/build/puente" "${puerto}"

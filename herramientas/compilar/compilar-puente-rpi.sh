#!/usr/bin/env bash
# Cruza el puente para la Raspberry. El binario estático queda en puente/build/rpi/.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../contenedor.sh
source "${DIR}/../contenedor.sh"

contenedor_correr -w /trabajo/puente -- \
    bash -c 'cmake --preset rpi && cmake --build --preset rpi'

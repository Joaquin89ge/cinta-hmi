#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../contenedor.sh
source "${DIR}/../contenedor.sh"

if [[ ! -e "${CINTA_PUERTO}" ]]; then
    echo "No aparece la ESP32 en ${CINTA_PUERTO}." >&2
    exit 1
fi

exec "${DIR}/../placa/idf.sh" -p "${CINTA_PUERTO}" monitor

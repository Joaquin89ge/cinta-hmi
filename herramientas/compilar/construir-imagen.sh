#!/usr/bin/env bash
# Construye la imagen cinta-hmi-dev:v6.1 con el uid de quien la corre.
set -euo pipefail
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

docker build \
    --build-arg "HOST_UID=$(id -u)" \
    --build-arg "HOST_GID=$(id -g)" \
    -t cinta-hmi-dev:v6.1 \
    "${RAIZ}/docker"

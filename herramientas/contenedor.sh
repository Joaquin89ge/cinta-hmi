#!/usr/bin/env bash
# Corre un comando dentro de la imagen cinta-hmi-dev.
# Uso desde otro script: source contenedor.sh && contenedor_correr -- comando args

CINTA_RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CINTA_IMAGEN="${CINTA_IMAGEN:-cinta-hmi-dev:v6.1}"
CINTA_PUERTO="${CINTA_PUERTO:-/dev/ttyACM0}"

contenedor_correr() {
    local extra=()
    while [[ $# -gt 0 && "$1" != "--" ]]; do
        extra+=("$1")
        shift
    done
    if [[ "${1:-}" == "--" ]]; then
        shift
    fi
    if [[ $# -eq 0 ]]; then
        echo "contenedor_correr: falta el comando después de --" >&2
        return 1
    fi

    local dialout_gid
    dialout_gid="$(getent group dialout | cut -d: -f3)"

    local run=(
        docker run --rm
        -e HOME=/tmp
        -e DISPLAY="${DISPLAY:-:0}"
        -v /tmp/.X11-unix:/tmp/.X11-unix
        -v "${CINTA_RAIZ}:/trabajo"
        -w /trabajo
        -u "$(id -u):$(id -g)"
        --group-add "${dialout_gid}"
    )

    if [[ -n "${XAUTHORITY:-}" && -f "${XAUTHORITY}" ]]; then
        run+=(-e "XAUTHORITY=${XAUTHORITY}" -v "${XAUTHORITY}:${XAUTHORITY}:ro")
    fi

    if [[ -e "${CINTA_PUERTO}" ]]; then
        run+=(--device "${CINTA_PUERTO}:${CINTA_PUERTO}")
    fi

    if [[ ${#extra[@]} -gt 0 ]]; then
        run+=("${extra[@]}")
    fi

    run+=("${CINTA_IMAGEN}" "$@")
    "${run[@]}"
}

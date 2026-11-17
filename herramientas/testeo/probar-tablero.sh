#!/usr/bin/env bash
# Compila el ejemplo Tablero de la cátedra y abre xWindow unos segundos.
# El fuente está en copiaTotalMateria/, que no se sube al repo.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../contenedor.sh
source "${DIR}/../contenedor.sh"

segundos="${1:-15}"
tablero="${CINTA_RAIZ}/copiaTotalMateria/material/Tablero"
if [[ ! -f "${tablero}/main.cpp" || ! -x "${tablero}/xWindow" ]]; then
    echo "No está el ejemplo de la cátedra en copiaTotalMateria/material/Tablero." >&2
    exit 1
fi

contenedor_correr -w /trabajo/copiaTotalMateria/material/Tablero -- \
    bash -c '
        set -euo pipefail
        segundos="$1"
        /usr/bin/g++ -std=c++17 -O2 -pthread -o /tmp/programa main.cpp mvc.cpp Consolas_24B.cpp
        rm -f /tmp/fifo
        mkfifo /tmp/fifo
        ./xWindow 680 512 >/tmp/xwindow.log 2>&1 &
        xw=$!
        sleep 1
        if ! kill -0 "$xw"; then
            echo "xWindow no llegó a abrir la ventana:" >&2
            cat /tmp/xwindow.log >&2
            exit 1
        fi
        /tmp/programa shared-fb >/tmp/programa.log 2>&1 &
        pr=$!
        sleep 2
        if ! kill -0 "$xw" || ! kill -0 "$pr"; then
            echo "La ventana se cerró:" >&2
            cat /tmp/xwindow.log /tmp/programa.log >&2
            exit 1
        fi
        echo "Ventana del Tablero abierta (${segundos}s)."
        sleep "$segundos"
        kill "$xw" "$pr" 2>/dev/null || true
        wait || true
    ' bash "${segundos}"

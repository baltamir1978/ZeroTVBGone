#!/usr/bin/env bash
# Compila ZeroTVBGone dentro de Docker, para no instalar en la máquina ni CMake ni el
# compilador cruzado (sólo hace falta Docker).
#
#   Tools/compilar.sh pruebas     compila el simulador y pasa las pruebas
#   Tools/compilar.sh deb         compila para el Cardputer Zero y deja el .deb en dist/
#   Tools/compilar.sh todo        las dos cosas (por defecto)
#   Tools/compilar.sh simulador   compila y abre el simulador en este escritorio (X11)
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGEN=zerotvbgone-build
docker build -q -t "$IMAGEN" Tools >/dev/null

# Con el usuario de fuera, para que build/ y dist/ no queden a nombre de root.
en_docker() {
    docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -v "$PWD":/src -w /src "$@"
}

pruebas() {
    en_docker "$IMAGEN" bash -c '
        cmake --preset linux-x86-64 >/dev/null &&
        cmake --build --preset linux-x86-64-rel &&
        ctest --test-dir build/linux-x86-64 -C Release --output-on-failure'
}

deb() {
    en_docker "$IMAGEN" cmake --workflow --preset cp0-cross-package
    ls -l dist/*.deb
}

simulador() {
    pruebas
    # En el simulador no hay LED: hace la pasada entera sin emitir nada.
    en_docker -e DISPLAY="$DISPLAY" -e SDL_RENDER_DRIVER=software \
        -v /tmp/.X11-unix:/tmp/.X11-unix \
        "$IMAGEN" build/linux-x86-64/Release/zerotvbgone
}

case "${1:-todo}" in
    pruebas) pruebas ;;
    deb) deb ;;
    simulador) simulador ;;
    todo) pruebas && deb ;;
    *) echo "uso: $0 [pruebas|deb|todo|simulador]" >&2; exit 2 ;;
esac

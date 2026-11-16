#!/usr/bin/env bash
# Compila ZeroTVBGone dentro de Docker, para no instalar en la máquina ni CMake ni el
# compilador cruzado (sólo hace falta Docker). Sin Docker usa los de la máquina, si están.
#
#   Tools/compilar.sh pruebas     compila el simulador y pasa las pruebas
#   Tools/compilar.sh deb         compila para el Cardputer Zero y deja el .deb en dist/
#   Tools/compilar.sh todo        las dos cosas (por defecto)
#   Tools/compilar.sh simulador   compila y abre el simulador en este escritorio (X11)
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGEN=zerotvbgone-build

# Sin Docker pero con las herramientas instaladas (las de Tools/Dockerfile) se compila
# directamente en la máquina: es lo mismo, sin el contenedor.
if command -v docker >/dev/null 2>&1; then
    docker build -q -t "$IMAGEN" Tools >/dev/null
    CON_DOCKER=1
elif command -v cmake >/dev/null 2>&1 && command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
    CON_DOCKER=0
else
    echo "Hace falta Docker o, si no, los paquetes de Tools/Dockerfile (cmake, compilador cruzado...)." >&2
    exit 1
fi

# Con el usuario de fuera, para que build/ y dist/ no queden a nombre de root.
# Los argumentos son los de `docker run`: opciones (-e, -v), la imagen y la orden. Sin
# Docker se queda con las variables de entorno y la orden.
en_docker() {
    if [ "$CON_DOCKER" = 1 ]; then
        docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -v "$PWD":/src -w /src "$@"
        return
    fi
    local entorno=()
    while [ "$1" != "$IMAGEN" ]; do
        case "$1" in
            -e) entorno+=("$2") ;;
        esac
        shift 2
    done
    shift
    env "${entorno[@]}" "$@"
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

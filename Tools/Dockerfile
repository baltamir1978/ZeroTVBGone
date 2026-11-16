# Entorno de compilación de ZeroTVBGone: simulador de escritorio (SDL2) y compilación
# cruzada a aarch64 para el Cardputer Zero. Lo usa Tools/compilar.sh.
# xvfb, xdotool e imagemagick son para probar el simulador sin pantalla.
FROM debian:trixie-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential crossbuild-essential-arm64 cmake ninja-build pkg-config git ca-certificates \
    curl wget file dpkg-dev fakeroot python3 \
    libsdl2-dev libfmt-dev libpng-dev libjpeg-dev libfreetype-dev zlib1g-dev \
    xvfb xdotool imagemagick x11-apps \
    && rm -rf /var/lib/apt/lists/*

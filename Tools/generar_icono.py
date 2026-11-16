#!/usr/bin/env python3
"""Dibuja el icono de la app (una tele con el símbolo de apagado) en los tamaños que piden
el lanzador (assets/images) y la tienda (store/icon.png). Necesita Pillow.

    Tools/generar_icono.py
"""

import os

from PIL import Image, ImageDraw

LADO = 1024  # se dibuja grande y se reduce: así los bordes salen suaves


def icono():
    imagen = Image.new("RGBA", (LADO, LADO), (0, 0, 0, 0))

    # Fondo: degradado de arriba abajo recortado en un cuadrado de esquinas redondas.
    fondo = Image.new("RGBA", (LADO, LADO))
    trazo = ImageDraw.Draw(fondo)
    arriba, abajo = (38, 44, 66), (12, 13, 22)
    for y in range(LADO):
        t = y / (LADO - 1)
        trazo.line([(0, y), (LADO, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(arriba, abajo)) + (255,))
    mascara = Image.new("L", (LADO, LADO), 0)
    ImageDraw.Draw(mascara).rounded_rectangle([40, 40, LADO - 40, LADO - 40], radius=200, fill=255)
    imagen.paste(fondo, (0, 0), mascara)

    trazo = ImageDraw.Draw(imagen)
    claro = (230, 232, 240, 255)
    pantalla = (16, 18, 28, 255)
    rojo = (232, 72, 85, 255)

    # Antenas, tele y patas.
    trazo.line([(512, 300), (380, 150)], fill=claro, width=30)
    trazo.line([(512, 300), (644, 150)], fill=claro, width=30)
    trazo.rounded_rectangle([170, 280, 854, 780], radius=80, fill=claro)
    trazo.rounded_rectangle([220, 330, 804, 730], radius=45, fill=pantalla)
    trazo.rounded_rectangle([300, 780, 360, 850], radius=14, fill=claro)
    trazo.rounded_rectangle([664, 780, 724, 850], radius=14, fill=claro)

    # Símbolo de apagado: un arco abierto por arriba y la raya.
    cx, cy, r = 512, 540, 120
    trazo.arc([cx - r, cy - r, cx + r, cy + r], start=-55, end=235, fill=rojo, width=40)
    trazo.rounded_rectangle([cx - 20, cy - r - 40, cx + 20, cy + 10], radius=20, fill=rojo)
    return imagen


def main():
    raiz = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    imagen = icono()
    for ruta, lado in (
        ("assets/images/zerotvbgone.png", 100),
        ("assets/images/zerotvbgone_100.png", 100),
        ("assets/images/zerotvbgone_80.png", 80),
        ("store/icon.png", 256),
    ):
        imagen.resize((lado, lado), Image.LANCZOS).save(os.path.join(raiz, ruta))


if __name__ == "__main__":
    main()

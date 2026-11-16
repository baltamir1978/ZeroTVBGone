#!/usr/bin/env python3
"""Genera src/model/tvbgone_codes.cpp a partir del WORLDcodes.c del TV-B-Gone original.

    git clone https://github.com/adafruit/TV-B-Gone-kit
    Tools/generar_codigos.py TV-B-Gone-kit/firmware/WORLDcodes.c > src/model/tvbgone_codes.cpp

El original está pensado para un ATtiny85 con 8 KB: guarda cada código como una tabla de
parejas encendido/apagado y una ristra de índices empaquetados de 2 a 4 bits. Aquí sobra
memoria, así que los índices se desempaquetan ya (un byte cada uno) y el programa no tiene
que repetir el lector de bits; las tablas de tiempos se dejan como están, en decenas de
microsegundos. La frecuencia se guarda en hercios, tal como la escribe el original, y no el
valor del temporizador del AVR, que la redondeaba.

Las directivas #ifdef del original sólo sirven para dejar fuera códigos cuando no caben los
dos continentes en el chip: aquí se pasan por alto y entran todos.
"""

import re
import sys


def sin_comentarios(texto):
    texto = re.sub(r"/\*.*?\*/", "", texto, flags=re.S)
    return re.sub(r"//[^\n]*", "", texto)


def numeros(texto):
    return [int(n, 0) for n in re.findall(r"0[xX][0-9a-fA-F]+|\d+", texto)]


def leer(ruta):
    with open(ruta, encoding="latin-1") as fichero:
        fuente = sin_comentarios(fichero.read())

    tiempos = {}
    for nombre, cuerpo in re.findall(
        r"const\s+uint16_t\s+(\w+)\s*\[\]\s*PROGMEM\s*=\s*\{(.*?)\}\s*;", fuente, flags=re.S
    ):
        valores = numeros(cuerpo)
        if not valores or len(valores) % 2:
            sys.exit(f"{nombre}: la tabla de tiempos no son parejas")
        tiempos[nombre] = valores

    codigos = {}
    patron = re.compile(
        r"const\s+struct\s+IrCode\s+(\w+)\s+PROGMEM\s*=\s*\{\s*"
        r"(freq_to_timerval\(\s*\d+\s*\)|0)\s*,\s*"  # portadora
        r"(\d+)\s*,\s*"  # parejas
        r"(\d+)\s*,\s*"  # bits por índice
        r"(\w+)\s*,\s*"  # tabla de tiempos
        r"\{(.*?)\}\s*\}\s*;",
        flags=re.S,
    )
    for nombre, portadora, parejas, bits, tabla, cuerpo in patron.findall(fuente):
        parejas, bits = int(parejas), int(bits)
        octetos = numeros(cuerpo)
        if tabla not in tiempos:
            sys.exit(f"{nombre}: no existe la tabla {tabla}")
        if len(octetos) * 8 < parejas * bits:
            sys.exit(f"{nombre}: faltan octetos para {parejas} parejas de {bits} bits")
        # Igual que read_bits() del original: de bit más alto a más bajo, octeto a octeto.
        ristra = "".join(f"{octeto:08b}" for octeto in octetos)
        indices = [int(ristra[i * bits : (i + 1) * bits], 2) for i in range(parejas)]
        if max(indices) >= len(tiempos[tabla]) // 2:
            sys.exit(f"{nombre}: índice fuera de la tabla {tabla}")
        hercios = numeros(portadora)[0]
        codigos[nombre] = (hercios, tabla, indices)

    listas = {}
    for nombre, cuerpo in re.findall(
        r"const\s+struct\s+IrCode\s*\*\s*const\s+(\w+)\s*\[\]\s*PROGMEM\s*=\s*\{(.*?)\}\s*;",
        fuente,
        flags=re.S,
    ):
        listas[nombre] = re.findall(r"&\s*(\w+)", cuerpo)
        for codigo in listas[nombre]:
            if codigo not in codigos:
                sys.exit(f"{nombre}: no existe {codigo}")
    return tiempos, codigos, listas


def filas(valores, por_fila, formato="{}"):
    lineas = []
    for i in range(0, len(valores), por_fila):
        lineas.append("    " + ", ".join(formato.format(v) for v in valores[i : i + por_fila]) + ",")
    return "\n".join(lineas)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    tiempos, codigos, listas = leer(sys.argv[1])
    regiones = (("kEurope", "EUpowerCodes"), ("kNorthAmerica", "NApowerCodes"))
    usados = []
    for _, lista in regiones:
        for codigo in listas[lista]:
            if codigo not in usados:
                usados.append(codigo)
    tablas = []
    for codigo in usados:
        if codigos[codigo][1] not in tablas:
            tablas.append(codigos[codigo][1])

    out = []
    out.append(
        """/*
 * GENERADO por Tools/generar_codigos.py: no editar a mano.
 *
 * Códigos de apagado de TV-B-Gone, de WORLDcodes.c del firmware 1.2 del kit de Adafruit
 * (https://github.com/adafruit/TV-B-Gone-kit), (c) Mitch Altman y Limor Fried,
 * con licencia Creative Commons Attribution-ShareAlike (ver LICENSE).
 *
 * SPDX-License-Identifier: CC-BY-SA-3.0
 */

#include "tvbgone.h"

namespace tvbgone {
namespace {

// Parejas encendido/apagado, en decenas de microsegundos."""
    )
    for tabla in tablas:
        out.append(f"const uint16_t {tabla}[] = {{")
        out.append(filas(tiempos[tabla], 2))
        out.append("};")
    out.append("\n// Qué pareja de la tabla toca en cada paso del código.")
    for codigo in usados:
        out.append(f"const uint8_t {codigo}_indices[] = {{")
        out.append(filas(codigos[codigo][2], 24))
        out.append("};")
    for nombre, lista in regiones:
        out.append(f"\nconst Code {nombre}[] = {{")
        for codigo in listas[lista]:
            hercios, tabla, indices = codigos[codigo]
            out.append(
                f"    {{{hercios}, {len(indices)}, {len(tiempos[tabla]) // 2}, {tabla}, {codigo}_indices}},"
            )
        out.append("};")
    out.append(
        """
} // namespace

CodeList codes(Region region) {
    if (region == Region::NorthAmerica) {
        return {kNorthAmerica, sizeof(kNorthAmerica) / sizeof(kNorthAmerica[0])};
    }
    return {kEurope, sizeof(kEurope) / sizeof(kEurope[0])};
}

} // namespace tvbgone"""
    )
    print("\n".join(out))
    print(
        f"{len(listas['EUpowerCodes'])} códigos de Europa, {len(listas['NApowerCodes'])} de "
        f"Norteamérica, {len(usados)} distintos",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()

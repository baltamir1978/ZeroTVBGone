/*
 * SPDX-License-Identifier: MIT
 *
 * Lógica de TV-B-Gone sin LVGL ni hardware: la lista de códigos de apagado, cómo se
 * convierte cada uno en los tiempos que pide el emisor de infrarrojos y qué hace cada
 * tecla. Va aparte para poder probarla en el PC (tests/tvbgone_test.cpp).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tvbgone {

/** Las dos listas del original. Cada una va de los televisores más vendidos a los menos. */
enum class Region {
    Europe,       // también Oceanía y Oriente Medio (donde el vídeo era PAL)
    NorthAmerica, // también el resto de América y Asia
};

/**
 * Un código de apagado tal como lo guarda el original: una tabla corta de parejas
 * encendido/apagado y la lista de qué pareja toca en cada paso.
 */
struct Code {
    uint32_t carrier_hz; // 0: sin portadora, el LED se queda encendido todo el pulso
    uint16_t pair_count;
    uint8_t time_pairs;     // parejas que tiene `times`
    const uint16_t* times;  // encendido, apagado, encendido…, en decenas de microsegundos
    const uint8_t* indices; // `pair_count` índices de pareja
};

struct CodeList {
    const Code* data;
    size_t size;
};

CodeList codes(Region region);

/** Nombre para la pantalla y nombre corto para el fichero de configuración. */
const char* region_name(Region region);
const char* region_id(Region region);
bool parse_region(std::string_view id, Region& region);
Region other_region(Region region);

/** Pausa entre un código y el siguiente, como en el original. */
constexpr uint32_t kGapBetweenCodesMs = 205;

/**
 * Tiempos del código en microsegundos, alternando pulso y silencio, que es como los pide
 * LIRC: empieza y acaba en pulso (el silencio final sobra, detrás va la pausa entre
 * códigos) y no hay ninguno a cero (los del original se funden con sus vecinos).
 */
std::vector<uint32_t> expand(const Code& code);

/**
 * Trozo de un código que se manda al núcleo de una vez, y el silencio que le sigue.
 * `durations` tiene siempre un número impar de tiempos.
 */
struct Burst {
    std::vector<uint32_t> durations;
    uint32_t gap_after_us{0};
};

// Lo que acepta el núcleo en cada escritura a /dev/lircN (drivers/media/rc/lirc_dev.c):
// como mucho medio segundo en total y un número limitado de tiempos. Se deja margen.
constexpr uint32_t kMaxBurstUs = 450'000;
constexpr size_t kMaxBurstDurations = 255;
// Un silencio así de largo separa dos tramas del mismo código: se corta por ahí.
constexpr uint32_t kFrameGapUs = 10'000;

/**
 * Parte los tiempos de `expand` en ráfagas que el núcleo acepte. Muchos códigos repiten la
 * trama dos o tres veces con decenas de milisegundos entre medias y enteros no caben en
 * medio segundo: se cortan por esos silencios, que los espera la app y no el núcleo (ahí
 * un milisegundo de más no importa; dentro de una trama sí).
 */
std::vector<Burst> split_bursts(const std::vector<uint32_t>& durations);

/** Si el núcleo aceptaría la ráfaga tal cual. */
bool burst_is_valid(const Burst& burst);

/** Lo que el usuario quiere hacer, ya separado de la tecla concreta. */
enum class Action {
    None,
    Toggle,     // empezar, o parar si ya está emitiendo
    NextRegion, // sólo hay dos: cualquier flecha cambia a la otra
    ToggleTheme,
};

/**
 * Traduce una tecla (código de LVGL o carácter) a una acción.
 *
 * En el Cardputer Zero las flechas están serigrafiadas en F, Z, X y C y sólo salen como
 * flechas con Fn: aquí esas letras valen igual sin Fn, que es como se usa con una mano.
 */
Action action_for_key(uint32_t key);

} // namespace tvbgone

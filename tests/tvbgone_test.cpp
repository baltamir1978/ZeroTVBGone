/*
 * SPDX-License-Identifier: MIT
 *
 * Pruebas de src/model/tvbgone.*: sin LVGL ni hardware, se pasan en el PC con ctest.
 */

#include "tvbgone.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("FALLA %s:%d: %s\n", __FILE__, __LINE__, #condition);   \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

using tvbgone::Burst;
using tvbgone::Code;
using tvbgone::Region;

void test_lists() {
    const auto europe = tvbgone::codes(Region::Europe);
    const auto america = tvbgone::codes(Region::NorthAmerica);
    // Los que trae WORLDcodes.c del firmware 1.2: si el generador se deja alguno, se nota.
    CHECK(europe.size == 138);
    CHECK(america.size == 137);

    // El primero de Norteamérica es el apagado de Sony (SIRC de 12 bits, dos veces):
    // cabecera de 2,4 ms y luego 1,2 ms para un uno y 0,6 ms para un cero.
    const auto sony = tvbgone::expand(america.data[0]);
    CHECK(america.data[0].carrier_hz == 38400);
    CHECK(sony.size() == 51);
    CHECK(sony.size() > 4 && sony[0] == 2400 && sony[1] == 600 && sony[2] == 1200 && sony[3] == 600);
    CHECK(sony.size() > 25 && sony[25] == 27000); // el silencio entre las dos tramas
}

void test_every_code_is_sendable() {
    size_t unmodulated = 0;
    for (const auto region : {Region::Europe, Region::NorthAmerica}) {
        const auto list = tvbgone::codes(region);
        for (size_t i = 0; i < list.size; ++i) {
            const Code& code = list.data[i];
            if (code.carrier_hz == 0) {
                ++unmodulated;
            }
            else {
                // Lo que admite el emisor: de 20 a 500 kHz; los mandos van de 30 a 60.
                CHECK(code.carrier_hz >= 25000 && code.carrier_hz <= 500000);
            }
            for (size_t k = 0; k < code.pair_count; ++k) {
                CHECK(code.indices[k] < code.time_pairs);
            }

            const auto durations = tvbgone::expand(code);
            CHECK(!durations.empty());
            CHECK(durations.size() % 2 == 1);
            uint64_t expected = 0;
            for (const auto us : durations) {
                CHECK(us > 0);
                expected += us;
            }

            // Partido en ráfagas no se pierde ni un microsegundo, y todas caben.
            const auto bursts = tvbgone::split_bursts(durations);
            CHECK(!bursts.empty());
            uint64_t sum = 0;
            for (const Burst& burst : bursts) {
                if (!tvbgone::burst_is_valid(burst)) {
                    std::printf("código %zu de %s: ráfaga inválida\n", i, tvbgone::region_id(region));
                    ++failures;
                }
                sum += burst.gap_after_us;
                for (const auto us : burst.durations) {
                    sum += us;
                }
            }
            CHECK(sum == expected);
            CHECK(bursts.back().gap_after_us == 0);
        }
    }
    CHECK(unmodulated > 0); // los hay: el emisor tiene que saber mandarlos (o saltarlos)
}

void test_expand_merges_zeros() {
    // Parejas en decenas de µs: 0 = (10,20) 1 = (30,0) 2 = (0,40) 3 = (50,60)
    const uint16_t times[] = {10, 20, 30, 0, 0, 40, 50, 60};
    {
        // Un apagado a cero junta su pulso con el siguiente; un encendido a cero, los silencios.
        const uint8_t indices[] = {0, 1, 0, 2, 3};
        const Code code{38000, 5, 4, times, indices};
        const std::vector<uint32_t> expected = {100, 200, 400, 600, 500};
        // 100 200 | 300 +100 → 400, 200 | (0) +400 → 600 | 500 (600 sobra al final)
        CHECK(tvbgone::expand(code) == expected);
    }
    {
        // Empezar por un silencio no cuenta, y acabar en pulso no pierde nada.
        const uint8_t indices[] = {2, 0, 1};
        const Code code{38000, 3, 4, times, indices};
        const std::vector<uint32_t> expected = {100, 200, 300};
        CHECK(tvbgone::expand(code) == expected);
    }
    {
        const Code code{38000, 0, 4, times, nullptr};
        CHECK(tvbgone::expand(code).empty());
        CHECK(tvbgone::split_bursts({}).empty());
    }
}

void test_split() {
    {
        // Sin silencios largos: una sola ráfaga.
        const std::vector<uint32_t> durations = {500, 500, 500, 9999, 500};
        const auto bursts = tvbgone::split_bursts(durations);
        CHECK(bursts.size() == 1 && bursts[0].durations == durations && bursts[0].gap_after_us == 0);
    }
    {
        // Se corta por los silencios entre tramas, que pasan a ser la espera.
        const auto bursts = tvbgone::split_bursts({500, 500, 500, 40000, 600, 600, 600, 10000, 700});
        CHECK(bursts.size() == 3);
        if (bursts.size() == 3) {
            CHECK(bursts[0].durations == (std::vector<uint32_t>{500, 500, 500}));
            CHECK(bursts[0].gap_after_us == 40000);
            CHECK(bursts[1].durations == (std::vector<uint32_t>{600, 600, 600}));
            CHECK(bursts[1].gap_after_us == 10000);
            CHECK(bursts[2].durations == (std::vector<uint32_t>{700}));
            CHECK(bursts[2].gap_after_us == 0);
        }
    }
    {
        // Una trama sin silencios largos que no cabe en medio segundo se corta por el mayor.
        std::vector<uint32_t> durations;
        for (int i = 0; i < 61; ++i) {
            durations.push_back(i == 31 ? 9500 : 9000);
        }
        const auto bursts = tvbgone::split_bursts(durations);
        CHECK(bursts.size() == 2);
        if (bursts.size() == 2) {
            CHECK(bursts[0].durations.size() == 31 && bursts[0].gap_after_us == 9500);
            CHECK(bursts[1].durations.size() == 29 && bursts[1].gap_after_us == 0);
            CHECK(tvbgone::burst_is_valid(bursts[0]) && tvbgone::burst_is_valid(bursts[1]));
        }
    }
    {
        // Y una con demasiados tiempos, también.
        std::vector<uint32_t> durations(601, 100);
        for (const auto& burst : tvbgone::split_bursts(durations)) {
            CHECK(tvbgone::burst_is_valid(burst));
        }
    }
    {
        // Si llega acabada en silencio, ese silencio no se manda.
        const auto bursts = tvbgone::split_bursts({500, 500, 500, 500});
        CHECK(bursts.size() == 1 && bursts[0].durations.size() == 3);
    }
    CHECK(!tvbgone::burst_is_valid({{}, 0}));
    CHECK(!tvbgone::burst_is_valid({{100, 100}, 0}));
    CHECK(!tvbgone::burst_is_valid({{100, 0, 100}, 0}));
    CHECK(!tvbgone::burst_is_valid({{400000, 100, 400000}, 0}));
    CHECK(tvbgone::burst_is_valid({{100}, 0}));
}

void test_regions() {
    Region region = Region::Europe;
    CHECK(tvbgone::parse_region("na", region) && region == Region::NorthAmerica);
    CHECK(tvbgone::parse_region("EU", region) && region == Region::Europe);
    CHECK(!tvbgone::parse_region("asia", region) && region == Region::Europe);
    CHECK(!tvbgone::parse_region("", region));
    for (const auto r : {Region::Europe, Region::NorthAmerica}) {
        Region parsed = tvbgone::other_region(r);
        CHECK(parsed != r);
        CHECK(tvbgone::parse_region(tvbgone::region_id(r), parsed) && parsed == r);
        CHECK(tvbgone::other_region(tvbgone::other_region(r)) == r);
    }
}

void test_keys() {
    using tvbgone::Action;
    // Códigos de LVGL: flechas 17-20, Enter 10.
    CHECK(tvbgone::action_for_key(10) == Action::Toggle);
    CHECK(tvbgone::action_for_key(' ') == Action::Toggle);
    for (const uint32_t key : {17U, 18U, 19U, 20U}) {
        CHECK(tvbgone::action_for_key(key) == Action::NextRegion);
    }
    for (const char key : {'f', 'x', 'z', 'c', 'F', 'X', 'Z', 'C'}) {
        CHECK(tvbgone::action_for_key(static_cast<uint32_t>(key)) == Action::NextRegion);
    }
    CHECK(tvbgone::action_for_key('t') == Action::ToggleTheme);
    CHECK(tvbgone::action_for_key(27) == Action::None); // ESC lo lleva la app
    CHECK(tvbgone::action_for_key('q') == Action::None);
}

} // namespace

int main() {
    test_lists();
    test_every_code_is_sendable();
    test_expand_merges_zeros();
    test_split();
    test_regions();
    test_keys();
    if (failures != 0) {
        std::printf("tvbgone_test: %d fallos\n", failures);
        return 1;
    }
    std::printf("tvbgone_test: todo bien\n");
    return 0;
}

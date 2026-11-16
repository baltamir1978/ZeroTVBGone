/*
 * SPDX-License-Identifier: MIT
 */

#include "tvbgone.h"

#include <algorithm>
#include <cstdint>

namespace tvbgone {
namespace {

// Códigos de tecla de LVGL (LV_KEY_*). Se repiten aquí para que este fichero no dependa
// de LVGL y se pueda probar solo; base_viewmodel.cpp comprueba al compilar que coinciden.
constexpr uint32_t kKeyUp = 17;
constexpr uint32_t kKeyDown = 18;
constexpr uint32_t kKeyRight = 19;
constexpr uint32_t kKeyLeft = 20;
constexpr uint32_t kKeyEnter = 10;

uint64_t total_us(const std::vector<uint32_t>& durations, size_t first, size_t last) {
    uint64_t sum = 0;
    for (size_t i = first; i < last; ++i) {
        sum += durations[i];
    }
    return sum;
}

/**
 * Añade a `out` el tramo [first, last) de `durations` (de pulso a pulso), partiéndolo por
 * su silencio más largo mientras no quepa en una escritura.
 */
void append_fitting(const std::vector<uint32_t>& durations,
                    size_t first,
                    size_t last,
                    uint32_t gap_after_us,
                    std::vector<Burst>& out) {
    const bool fits = last - first <= kMaxBurstDurations &&
                      total_us(durations, first, last) <= kMaxBurstUs;
    if (fits || last - first < 3) {
        // Un pulso suelto que no cabe no tiene por dónde cortarse: se deja como está y
        // burst_is_valid lo delata (no hay ninguno en la lista; lo comprueban las pruebas).
        out.push_back({{durations.begin() + static_cast<std::ptrdiff_t>(first),
                        durations.begin() + static_cast<std::ptrdiff_t>(last)},
                       gap_after_us});
        return;
    }
    size_t longest = first + 1;
    for (size_t i = first + 1; i < last; i += 2) {
        if (durations[i] > durations[longest]) {
            longest = i;
        }
    }
    append_fitting(durations, first, longest, durations[longest], out);
    append_fitting(durations, longest + 1, last, gap_after_us, out);
}

} // namespace

const char* region_name(Region region) {
    return region == Region::NorthAmerica ? "Americas / Asia" : "Europe";
}

const char* region_id(Region region) {
    return region == Region::NorthAmerica ? "na" : "eu";
}

bool parse_region(std::string_view id, Region& region) {
    if (id == "eu" || id == "EU") {
        region = Region::Europe;
        return true;
    }
    if (id == "na" || id == "NA") {
        region = Region::NorthAmerica;
        return true;
    }
    return false;
}

Region other_region(Region region) {
    return region == Region::Europe ? Region::NorthAmerica : Region::Europe;
}

std::vector<uint32_t> expand(const Code& code) {
    std::vector<uint32_t> out;
    out.reserve(static_cast<size_t>(code.pair_count) * 2);
    for (size_t i = 0; i < code.pair_count; ++i) {
        const size_t pair = code.indices[i];
        if (pair >= code.time_pairs) {
            continue; // el generador ya lo impide; por si la tabla se toca a mano
        }
        for (size_t half = 0; half < 2; ++half) {
            const uint32_t us = static_cast<uint32_t>(code.times[pair * 2 + half]) * 10U;
            if (us == 0) {
                continue;
            }
            const bool is_space = half == 1;
            const bool expects_space = out.size() % 2 == 1;
            if (is_space == expects_space) {
                out.push_back(us);
            }
            else if (!out.empty()) {
                out.back() += us; // el vecino valía cero: dos pulsos o dos silencios seguidos
            }
            // Un silencio antes del primer pulso no es nada.
        }
    }
    if (out.size() % 2 == 0 && !out.empty()) {
        out.pop_back();
    }
    return out;
}

std::vector<Burst> split_bursts(const std::vector<uint32_t>& durations) {
    std::vector<Burst> out;
    if (durations.empty()) {
        return out;
    }
    const size_t end = durations.size() - (durations.size() % 2 == 0 ? 1 : 0);
    size_t first = 0;
    for (size_t i = 1; i < end; i += 2) {
        if (durations[i] >= kFrameGapUs) {
            append_fitting(durations, first, i, durations[i], out);
            first = i + 1;
        }
    }
    append_fitting(durations, first, end, 0, out);
    return out;
}

bool burst_is_valid(const Burst& burst) {
    const auto& durations = burst.durations;
    if (durations.empty() || durations.size() % 2 == 0 || durations.size() > kMaxBurstDurations) {
        return false;
    }
    if (std::find(durations.begin(), durations.end(), 0U) != durations.end()) {
        return false;
    }
    return total_us(durations, 0, durations.size()) <= kMaxBurstUs;
}

Action action_for_key(uint32_t key) {
    switch (key) {
        case kKeyEnter:
        case '\r':
        case ' ':
            return Action::Toggle;
        case kKeyUp:
        case kKeyDown:
        case kKeyLeft:
        case kKeyRight:
        case 'f':
        case 'F':
        case 'x':
        case 'X':
        case 'z':
        case 'Z':
        case 'c':
        case 'C':
            return Action::NextRegion;
        case 't':
        case 'T':
            return Action::ToggleTheme;
        default:
            return Action::None;
    }
}

} // namespace tvbgone

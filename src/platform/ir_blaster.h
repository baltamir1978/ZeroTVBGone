/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "tvbgone.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace platform {

/**
 * Emite la lista de códigos de una región por el LED infrarrojo, en un hilo aparte: una
 * pasada dura cerca de un minuto y la pantalla tiene que seguir respondiendo (sobre todo
 * para poder pararla).
 *
 * El LED se maneja con el dispositivo LIRC del núcleo (`/dev/lircN`, el de `gpio-ir-tx` en
 * el Cardputer Zero): se le escriben los tiempos de pulso y silencio y es el núcleo quien
 * genera la portadora y lleva la cuenta de los microsegundos, cosa que desde un proceso
 * normal no saldría fina.
 *
 * `ZEROTVBGONE_LIRC=/dev/lircN` fuerza el dispositivo y `ZEROTVBGONE_LIRC=sim` no emite
 * nada pero tarda lo mismo (es lo que hace el simulador de escritorio si no se dice otra
 * cosa).
 */
class IrBlaster {
public:
    enum class State {
        Idle,    // todavía no se ha emitido nada
        Running,
        Done,    // llegó al final de la lista
        Stopped, // lo paró el usuario
        Failed,  // no hay emisor, o no deja escribir: ver `error`
    };

    struct Status {
        State state{State::Idle};
        size_t sent{0};  // códigos ya emitidos (o saltados) de esta pasada
        size_t total{0};
        size_t skipped{0};      // los que el emisor no ha querido
        uint32_t carrier_hz{0}; // del código que se está emitiendo
        bool simulated{false};
        std::string error;
    };

    IrBlaster() = default;
    ~IrBlaster();

    IrBlaster(const IrBlaster&) = delete;
    IrBlaster& operator=(const IrBlaster&) = delete;

    /** Empieza una pasada desde el primer código (parando la que hubiera). */
    void start(tvbgone::Region region);
    /** Para la pasada; vuelve cuando el LED ya está quieto. */
    void stop();
    bool running() const;
    Status status() const;

private:
    void run(tvbgone::Region region);
    bool wait_or_cancel(uint32_t milliseconds);
    void finish(State state, const std::string& error = {});

    std::thread worker_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    bool cancel_{false}; // protegido por mutex_
    Status status_;      // protegido por mutex_
    std::atomic<bool> running_{false};
};

} // namespace platform

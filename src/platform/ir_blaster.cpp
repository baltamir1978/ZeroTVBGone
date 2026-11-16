/*
 * SPDX-License-Identifier: MIT
 */

#include "ir_blaster.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

#include <fcntl.h>
#include <linux/lirc.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

namespace platform {
namespace {

// La mitad del tiempo encendido, como el original (y como arranca gpio-ir-tx): es lo que
// más alcance da, porque los receptores filtran por la portadora y con el 50 % es cuando
// más energía cae en ella.
constexpr uint32_t kDutyCyclePercent = 50;

class Transmitter {
public:
    ~Transmitter() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    /** Busca y abre el emisor. Si no hay, deja en `error` el motivo, ya para la pantalla. */
    bool open(std::string& error) {
        const char* forced = std::getenv("ZEROTVBGONE_LIRC");
        std::string choice = forced ? forced : "";
#if USE_DESKTOP
        if (choice.empty()) {
            choice = "sim";
        }
#endif
        if (choice == "sim") {
            simulated_ = true;
            return true;
        }

        std::vector<std::string> candidates;
        if (!choice.empty()) {
            candidates.push_back(choice);
        }
        else {
            std::error_code ignored;
            for (const auto& entry : std::filesystem::directory_iterator("/dev", ignored)) {
                const auto name = entry.path().filename().string();
                if (name.rfind("lirc", 0) == 0) {
                    candidates.push_back(entry.path().string());
                }
            }
            std::sort(candidates.begin(), candidates.end());
        }

        // El receptor del aparato también es un /dev/lircN: no vale el número, hay que
        // preguntarle a cada uno si sabe emitir.
        bool denied = false;
        for (const auto& path : candidates) {
            const int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
            if (fd < 0) {
                denied = denied || errno == EACCES || errno == EPERM;
                continue;
            }
            uint32_t features = 0;
            if (::ioctl(fd, LIRC_GET_FEATURES, &features) == 0 && (features & LIRC_CAN_SEND_PULSE)) {
                fd_ = fd;
                features_ = features;
                uint32_t mode = LIRC_MODE_PULSE;
                ::ioctl(fd_, LIRC_SET_SEND_MODE, &mode);
                if (features_ & LIRC_CAN_SET_SEND_DUTY_CYCLE) {
                    uint32_t duty = kDutyCyclePercent;
                    ::ioctl(fd_, LIRC_SET_SEND_DUTY_CYCLE, &duty);
                }
                return true;
            }
            ::close(fd);
        }
        if (denied) {
            error = "No permission to use the IR LED";
        }
        else if (!choice.empty()) {
            error = choice + " is not an IR transmitter";
        }
        else {
            error = "No IR transmitter found";
        }
        return false;
    }

    bool simulated() const {
        return simulated_;
    }

    /**
     * Pone la portadora del código que viene. `false` si el emisor no puede con ella
     * (pasa con los pocos códigos sin portadora en un emisor por PWM): ese se salta.
     */
    bool set_carrier(uint32_t hz) {
        if (simulated_) {
            return true;
        }
        if (!(features_ & LIRC_CAN_SET_SEND_CARRIER)) {
            return hz != 0; // irá con la que tenga, que será la habitual de 38 kHz
        }
        return ::ioctl(fd_, LIRC_SET_SEND_CARRIER, &hz) >= 0;
    }

    /** Manda una ráfaga; vuelve cuando ha acabado de salir. En fallo deja `errno`. */
    bool send(const tvbgone::Burst& burst) {
        if (simulated_) {
            uint64_t us = 0;
            for (const auto duration : burst.durations) {
                us += duration;
            }
            sleep_us(us);
            return true;
        }
        const size_t bytes = burst.durations.size() * sizeof(uint32_t);
        ssize_t written;
        do {
            written = ::write(fd_, burst.durations.data(), bytes);
        } while (written < 0 && errno == EINTR);
        return written == static_cast<ssize_t>(bytes);
    }

    static void sleep_us(uint64_t us) {
        timespec wait{static_cast<time_t>(us / 1'000'000), static_cast<long>(us % 1'000'000) * 1000};
        while (::clock_nanosleep(CLOCK_MONOTONIC, 0, &wait, &wait) == EINTR) {
        }
    }

private:
    int fd_{-1};
    uint32_t features_{0};
    bool simulated_{false};
};

} // namespace

IrBlaster::~IrBlaster() {
    stop();
}

void IrBlaster::start(tvbgone::Region region) {
    stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancel_ = false;
        status_ = Status{};
        status_.state = State::Running;
        status_.total = tvbgone::codes(region).size;
    }
    running_ = true;
    worker_ = std::thread([this, region] { run(region); });
}

void IrBlaster::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancel_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool IrBlaster::running() const {
    return running_;
}

IrBlaster::Status IrBlaster::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

bool IrBlaster::wait_or_cancel(uint32_t milliseconds) {
    std::unique_lock<std::mutex> lock(mutex_);
    wake_.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return cancel_; });
    return cancel_;
}

void IrBlaster::finish(State state, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_.state = state;
    status_.error = error;
    status_.carrier_hz = 0;
    running_ = false;
}

void IrBlaster::run(tvbgone::Region region) {
    Transmitter transmitter;
    std::string error;
    if (!transmitter.open(error)) {
        finish(State::Failed, error);
        return;
    }

    const auto list = tvbgone::codes(region);
    size_t skipped = 0;
    int write_error = 0;
    bool gave_up = false;
    for (size_t i = 0; i < list.size; ++i) {
        const tvbgone::Code& code = list.data[i];
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (cancel_) {
                break;
            }
            status_.sent = i;
            status_.skipped = skipped;
            status_.carrier_hz = code.carrier_hz;
            status_.simulated = transmitter.simulated();
        }

        bool sent = transmitter.set_carrier(code.carrier_hz);
        if (sent) {
            // Una vez empezado, el código se manda entero aunque se pida parar: son décimas
            // de segundo, y medio código no apaga nada.
            for (const auto& burst : tvbgone::split_bursts(tvbgone::expand(code))) {
                if (!transmitter.send(burst)) {
                    write_error = errno;
                    sent = false;
                    break;
                }
                if (burst.gap_after_us > 0) {
                    Transmitter::sleep_us(burst.gap_after_us);
                }
            }
        }
        if (!sent) {
            ++skipped;
            // Si los tres primeros ya fallan al escribir, fallarán todos: mejor decirlo ya
            // que tener la pantalla medio minuto contando códigos que no salen.
            if (write_error != 0 && i == 2 && skipped == 3) {
                gave_up = true;
                break;
            }
        }

        if (i + 1 < list.size && wait_or_cancel(tvbgone::kGapBetweenCodesMs)) {
            break;
        }
    }

    bool cancelled;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancelled = cancel_;
        if (!cancelled && !gave_up) {
            status_.sent = list.size;
        }
        status_.skipped = skipped;
    }
    if (cancelled) {
        finish(State::Stopped);
    }
    else if (gave_up || skipped == list.size) {
        // Ni uno ha salido: el emisor está pero no funciona.
        finish(State::Failed,
               write_error != 0 ? std::string("IR LED error: ") + std::strerror(write_error)
                                : std::string("The IR LED rejected every code"));
    }
    else {
        finish(State::Done);
    }
}

} // namespace platform

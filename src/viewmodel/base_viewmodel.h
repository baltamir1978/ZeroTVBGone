/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "app_page.h"
#include "ir_blaster.h"
#include "subjects.h"
#include "tvbgone.h"

#include "lvgl.h"

namespace viewmodel {

/**
 * Estado de la app tal como lo ve la pantalla: qué región está elegida y cómo va la
 * emisión. Aquí se decide qué hace cada tecla; emitir es cosa de platform::IrBlaster, que
 * va en su hilo, y la pantalla le pregunta cómo va con `status()` (no hay «subject» para
 * eso: el hilo no puede tocar LVGL).
 */
class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* quit_requested_subject();
    /** 0 = Europa, 1 = Norteamérica (el valor de tvbgone::Region). */
    lv_subject_t* region_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    tvbgone::Region region() const;
    void set_region(tvbgone::Region region);

    platform::IrBlaster::Status status() const;

    model::AppPage current_page() const;

    /** Tecla que no es ESC. Devuelve si se ha usado. */
    bool handle_key(uint32_t key);

    /**
     * ESC corto: para la emisión. `false` si no había nada que parar (y entonces la app
     * recuerda que para salir hay que mantenerlo).
     */
    bool back();

    void request_quit();

private:
    platform::IrBlaster blaster_;
    reactive::StringSubject<128> title_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::BoolSubject quit_requested_subject_;
    reactive::IntSubject region_subject_;
};

} // namespace viewmodel

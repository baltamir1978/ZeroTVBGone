/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include <array>
#include <cstddef>

namespace screen {

/**
 * La única pantalla: arriba las dos regiones, en medio qué está pasando (con la cuenta de
 * códigos y una barra mientras emite) y abajo qué tecla toca.
 */
class MainScreen : public BaseScreen {
public:
    MainScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~MainScreen() override;

protected:
    void build_content(lv_obj_t* content) override;

private:
    void refresh();
    static void refresh_observer(lv_observer_t* observer, lv_subject_t* subject);
    static void poll_timer_cb(lv_timer_t* timer);

    std::array<lv_obj_t*, 2> region_tabs_{};
    lv_obj_t* headline_{nullptr};
    lv_obj_t* detail_{nullptr};
    lv_obj_t* bar_{nullptr};
    lv_obj_t* hint_{nullptr};
    lv_timer_t* poll_timer_{nullptr};
};

} // namespace screen

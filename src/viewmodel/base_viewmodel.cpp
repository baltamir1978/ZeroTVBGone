/*
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

namespace viewmodel {
namespace {

// tvbgone.cpp repite estos códigos para no depender de LVGL; si LVGL los cambiara,
// que falle aquí al compilar y no en el Cardputer.
static_assert(LV_KEY_UP == 17 && LV_KEY_DOWN == 18 && LV_KEY_RIGHT == 19 && LV_KEY_LEFT == 20);
static_assert(LV_KEY_ENTER == 10);

} // namespace

BaseViewModel::BaseViewModel()
    : title_subject_("TV-B-Gone"),
      dark_mode_subject_(true),
      current_page_subject_(static_cast<int>(model::AppPage::Main)),
      quit_requested_subject_(false),
      region_subject_(static_cast<int>(tvbgone::Region::Europe)) {}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

lv_subject_t* BaseViewModel::region_subject() {
    return region_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return dark_mode_subject_.value();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    dark_mode_subject_.set(enabled);
}

void BaseViewModel::toggle_dark_mode() {
    dark_mode_subject_.toggle();
}

tvbgone::Region BaseViewModel::region() const {
    return region_subject_.value() == static_cast<int>(tvbgone::Region::NorthAmerica)
               ? tvbgone::Region::NorthAmerica
               : tvbgone::Region::Europe;
}

void BaseViewModel::set_region(tvbgone::Region region) {
    region_subject_.set(static_cast<int>(region));
}

platform::IrBlaster::Status BaseViewModel::status() const {
    return blaster_.status();
}

model::AppPage BaseViewModel::current_page() const {
    return model::AppPage::Main;
}

bool BaseViewModel::handle_key(uint32_t key) {
    switch (tvbgone::action_for_key(key)) {
        case tvbgone::Action::Toggle:
            if (blaster_.running()) {
                blaster_.stop();
            }
            else {
                blaster_.start(region());
            }
            return true;
        case tvbgone::Action::NextRegion:
            // A media pasada no se cambia de lista: la tecla se da por usada y no hace nada.
            if (!blaster_.running()) {
                set_region(tvbgone::other_region(region()));
            }
            return true;
        case tvbgone::Action::ToggleTheme:
            toggle_dark_mode();
            return true;
        default:
            return false;
    }
}

bool BaseViewModel::back() {
    if (!blaster_.running()) {
        return false;
    }
    blaster_.stop();
    return true;
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

} // namespace viewmodel

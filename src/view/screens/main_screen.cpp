/*
 * SPDX-License-Identifier: MIT
 */

#include "main_screen.h"

#include "asset_manager.h"
#include "theme.h"

#include <cstdio>

namespace screen {
namespace {

using State = platform::IrBlaster::State;

constexpr int32_t kMargin = 8;
constexpr int32_t kTabGap = 6;
constexpr int32_t kTabWidth = (view::kScreenWidth - 2 * kMargin - kTabGap) / 2;
constexpr int32_t kTabHeight = 26;

// El hilo que emite no puede tocar LVGL: la pantalla le pregunta cómo va. Diez veces por
// segundo sobra, los códigos salen a unos tres por segundo.
constexpr uint32_t kPollMs = 100;

} // namespace

MainScreen::MainScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

MainScreen::~MainScreen() {
    if (poll_timer_) {
        lv_timer_delete(poll_timer_);
    }
}

void MainScreen::build_content(lv_obj_t* content) {
    const lv_font_t* tab_font = assets().load_standard_font(13);
    const lv_font_t* headline_font = assets().load_standard_font(24);
    const lv_font_t* text_font = assets().load_standard_font(13, app::StandardFontWeight::Regular);
    const lv_font_t* hint_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);

    for (size_t i = 0; i < region_tabs_.size(); ++i) {
        auto* tab = lv_label_create(content);
        lv_label_set_text(tab, tvbgone::region_name(static_cast<tvbgone::Region>(i)));
        lv_obj_set_style_text_font(tab, tab_font ? tab_font : &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(tab, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(tab, kTabWidth, kTabHeight);
        const int32_t line = lv_font_get_line_height(tab_font ? tab_font : &lv_font_montserrat_14);
        lv_obj_set_style_pad_top(tab, (kTabHeight - line) / 2, 0);
        lv_obj_set_pos(tab, kMargin + static_cast<int32_t>(i) * (kTabWidth + kTabGap), 7);
        lv_obj_set_style_radius(tab, 5, 0);
        lv_obj_set_style_border_width(tab, 1, 0);
        region_tabs_[i] = tab;
    }

    headline_ = lv_label_create(content);
    lv_obj_set_style_text_font(headline_, headline_font ? headline_font : &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(headline_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(headline_, view::kScreenWidth - 2 * kMargin);
    lv_label_set_long_mode(headline_, LV_LABEL_LONG_DOT);
    lv_obj_align(headline_, LV_ALIGN_TOP_MID, 0, 42);

    detail_ = lv_label_create(content);
    lv_obj_set_style_text_font(detail_, text_font ? text_font : &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(detail_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(detail_, view::kScreenWidth - 2 * kMargin);
    lv_label_set_long_mode(detail_, LV_LABEL_LONG_DOT);
    // Con alto fijo de una línea: un error largo se corta con puntos en vez de bajar a la barra.
    lv_obj_set_height(detail_, lv_font_get_line_height(text_font ? text_font : &lv_font_montserrat_14));
    lv_obj_align(detail_, LV_ALIGN_TOP_MID, 0, 74);

    bar_ = lv_bar_create(content);
    lv_obj_set_size(bar_, view::kScreenWidth - 2 * kMargin, 8);
    lv_obj_align(bar_, LV_ALIGN_TOP_MID, 0, 99);
    lv_obj_set_style_radius(bar_, 4, 0);
    lv_obj_set_style_radius(bar_, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, LV_PART_INDICATOR);

    hint_ = lv_label_create(content);
    lv_obj_set_style_text_font(hint_, hint_font ? hint_font : &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(hint_, view::kScreenWidth - 2 * kMargin);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -6);

    // Al añadir cada observador LVGL lo llama una vez: eso hace el primer pintado.
    lv_subject_add_observer_obj(view_model().region_subject(), refresh_observer, content, this);
    lv_subject_add_observer_obj(view_model().dark_mode_subject(), refresh_observer, content, this);
    poll_timer_ = lv_timer_create(poll_timer_cb, kPollMs, this);
}

void MainScreen::refresh() {
    if (!hint_) {
        return; // primer aviso de un observador, con la pantalla a medio montar
    }
    const auto colors = view::palette(view_model().is_dark_mode());
    const auto status = view_model().status();
    const bool running = status.state == State::Running;
    const size_t selected = static_cast<size_t>(view_model().region());

    for (size_t i = 0; i < region_tabs_.size(); ++i) {
        const bool is_selected = i == selected;
        auto* tab = region_tabs_[i];
        lv_obj_set_style_bg_color(tab, colors.primary, 0);
        lv_obj_set_style_bg_opa(tab, is_selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(tab, is_selected ? colors.primary : colors.border, 0);
        // Sobre el color de selección el texto va con el color de fondo, que contrasta con
        // él en los dos temas. La otra región se apaga mientras se emite: no se puede cambiar.
        lv_obj_set_style_text_color(
            tab, is_selected ? colors.background : (running ? colors.text_disabled : colors.text), 0);
    }

    char headline[48];
    char detail[96];
    lv_color_t headline_color = colors.text;
    const size_t total = status.total > 0 ? status.total : tvbgone::codes(view_model().region()).size;
    switch (status.state) {
        case State::Running:
            std::snprintf(headline, sizeof(headline), "%zu / %zu", status.sent + 1, total);
            // En el simulador no sale nada por ningún LED: que no parezca que sí.
            if (status.carrier_hz > 0) {
                std::snprintf(detail, sizeof(detail), "%s power-off codes  %u.%u kHz",
                              status.simulated ? "Simulating" : "Sending",
                              status.carrier_hz / 1000, (status.carrier_hz % 1000) / 100);
            }
            else {
                std::snprintf(detail, sizeof(detail), "%s power-off codes",
                              status.simulated ? "Simulating" : "Sending");
            }
            headline_color = colors.primary;
            break;
        case State::Done:
            std::snprintf(headline, sizeof(headline), "Done");
            if (status.skipped > 0) {
                std::snprintf(detail, sizeof(detail), "%zu codes sent, %zu not supported",
                              status.total - status.skipped, status.skipped);
            }
            else {
                std::snprintf(detail, sizeof(detail), "%zu codes sent", status.total);
            }
            headline_color = colors.success;
            break;
        case State::Stopped:
            std::snprintf(headline, sizeof(headline), "Stopped");
            std::snprintf(detail, sizeof(detail), "%zu of %zu codes sent", status.sent, status.total);
            headline_color = colors.warning;
            break;
        case State::Failed:
            std::snprintf(headline, sizeof(headline), "No IR");
            std::snprintf(detail, sizeof(detail), "%s", status.error.c_str());
            headline_color = colors.error;
            break;
        case State::Idle:
        default:
            std::snprintf(headline, sizeof(headline), "Ready");
            std::snprintf(detail, sizeof(detail), "%zu power-off codes. Point at the TV.", total);
            break;
    }
    lv_label_set_text(headline_, headline);
    lv_label_set_text(detail_, detail);
    lv_obj_set_style_text_color(headline_, headline_color, 0);
    lv_obj_set_style_text_color(detail_, colors.text, 0);

    lv_bar_set_range(bar_, 0, static_cast<int32_t>(total));
    lv_bar_set_value(bar_, static_cast<int32_t>(status.sent), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_, colors.border, 0);
    lv_obj_set_style_bg_color(bar_, running ? colors.primary : colors.text_disabled, LV_PART_INDICATOR);
    if (status.state == State::Idle || status.state == State::Failed) {
        lv_obj_add_flag(bar_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_remove_flag(bar_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(hint_, running ? "ENTER / ESC  stop" : "ENTER  start      Z / C  region");
    lv_obj_set_style_text_color(hint_, colors.text_disabled, 0);
}

void MainScreen::refresh_observer(lv_observer_t* observer, lv_subject_t*) {
    auto* screen = static_cast<MainScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh();
    }
}

void MainScreen::poll_timer_cb(lv_timer_t* timer) {
    auto* screen = static_cast<MainScreen*>(lv_timer_get_user_data(timer));
    if (screen) {
        screen->refresh();
    }
}

} // namespace screen

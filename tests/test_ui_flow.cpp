#include <cstddef>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <guayaba/Guayaba.hpp>

#include "app/FakePrepController.hpp"
#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"
#include "TestHarness.hpp"

namespace {

using booteable::FakePrepController;
using booteable::PrepOutcome;
using booteable::PrepStage;
using booteable::SafetyVerdict;
using booteable::ui::AppScene;
using booteable::ui::UiScreen;
using booteable::ui::WizardStep;
using guayaba::Key;
using guayaba::Rect;
using guayaba::Vec2;

// Terminal de prueba: presenta el buffer a memoria para inspeccionar qué celdas
// quedaron sin cubrir. El renderer limpia el buffer en BRIGHT_WHITE, así que un
// fondo blanco residual delata un hueco de layout.
class CapturingTerminal : public guayaba::IOSTerminal {
public:
    std::vector<guayaba::TermChar> buffer;
    Vec2 size{0, 0};

    void init() override {}
    void restore() override {}
    void set_cursor_visible(bool) override {}
    void set_cursor_position(Vec2) override {}
    void present_buffer(const std::vector<guayaba::TermChar>& presented,
                        Vec2 presented_size) override {
        buffer = presented;
        size = presented_size;
    }
    Vec2 get_screen_size(Vec2 fallback) const override { return fallback; }
    void poll_events() override {}
    Vec2 get_mouse_position() const override { return {0, 0}; }
    bool is_left_click_pressed() const override { return false; }
    bool is_right_click_pressed() const override { return false; }
    bool is_left_click_just_pressed() const override { return false; }
    bool is_right_click_just_pressed() const override { return false; }
    std::vector<guayaba::InputEvent> take_input_events() override { return {}; }
    bool was_key_pressed(Key) const override { return false; }
    int get_wheel_delta() const override { return 0; }
    std::string get_line_input(Vec2) override { return {}; }

    bool has_white_cell() const {
        for (const guayaba::TermChar& cell : buffer) {
            if (cell.background ==
                guayaba::Color::from_name(guayaba::Color::BRIGHT_WHITE)) {
                return true;
            }
        }
        return false;
    }
};

std::unique_ptr<guayaba::Node> layout(AppScene& app, int width, int height) {
    auto root = app.build();
    root->update_layout(Rect::from_size(Vec2{width, height}));
    return root;
}

void start_flow(AppScene& app) {
    app.handle_key(Key::ENTER);
    app.set_confirmed(true);
    app.handle_key(Key::ENTER);
}

void test_format() {
    using booteable::ui::format_bytes;
    using booteable::ui::outcome_color;
    using booteable::ui::outcome_label;
    using booteable::ui::progress_step;
    using booteable::ui::wizard_dots;
    using booteable::ui::wizard_failed_label;
    using booteable::ui::wizard_phase_label;
    using booteable::ui::wizard_step_for_stage;
    using booteable::ui::wizard_step_label;

    CHECK_EQ(format_bytes(0), std::string("0 B"));
    CHECK_EQ(format_bytes(1536), std::string("1.5 KiB"));
    CHECK_EQ(format_bytes(64ull * 1024ull * 1024ull), std::string("64.0 MiB"));
    CHECK_EQ(format_bytes(16ull * 1024ull * 1024ull * 1024ull),
             std::string("16.0 GiB"));

    CHECK_EQ(outcome_label(PrepOutcome::Verified), std::string("VERIFICADO"));
    CHECK_EQ(outcome_color(PrepOutcome::Verified), std::string("BRIGHT_GREEN"));
    CHECK_EQ(outcome_color(PrepOutcome::Incomplete),
             std::string("BRIGHT_YELLOW"));
    CHECK_EQ(outcome_color(PrepOutcome::VerificationFailed),
             std::string("BRIGHT_RED"));

    CHECK_EQ(progress_step(PrepStage::Writing, PrepStage::Building),
             std::string("[x] Construyendo la imagen"));
    CHECK_EQ(progress_step(PrepStage::Writing, PrepStage::Writing),
             std::string("[>] Escribiendo la imagen"));

    CHECK_EQ(wizard_step_label(WizardStep::Partition), std::string("Particionar"));
    CHECK_EQ(wizard_dots(WizardStep::Format), std::string("[##>---]"));
    CHECK(wizard_phase_label(WizardStep::Partition).find("FASE 1") !=
          std::string::npos);
    CHECK(wizard_phase_label(WizardStep::Write).find("FASE 2") !=
          std::string::npos);
    CHECK(wizard_step_for_stage(PrepStage::Building) ==
          WizardStep::Partition);
    CHECK(wizard_step_for_stage(PrepStage::Writing) == WizardStep::Write);
    CHECK(wizard_failed_label(PrepStage::Building).find("Particionar") !=
          std::string::npos);
}

void test_narrow_layout() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    CHECK_EQ(app.rail_slot()->get_custom_minimum_size().x, 5);
    CHECK(app.split()->get_orientation() ==
          guayaba::SplitOrientation::HorizontalDivider);
    const Rect devices = app.devices_panel()->get_rect();
    const Rect details = app.details_panel()->get_rect();
    CHECK(devices.bottom() <= details.top());
    CHECK(devices.right() <= 80);
    CHECK(details.right() <= 80);
    CHECK(details.bottom() <= 24);

    CHECK(app.prepare_button()->get_rect().right() <= 80);
    CHECK(app.export_button()->get_rect().right() <= 80);
    CHECK(app.prepare_button()->get_rect().bottom() <= 24);

    CHECK_EQ(app.device_list()->item_count(), std::size_t{4});

    guayaba::ConsoleRenderer renderer(Vec2{80, 24}, nullptr);
    root->draw(renderer, Rect::from_size(Vec2{80, 24}));
}

void test_wide_layout_and_expanded_rail() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = layout(app, 120, 40);
    CHECK(root != nullptr);

    CHECK(app.split()->get_orientation() ==
          guayaba::SplitOrientation::VerticalDivider);
    const Rect devices = app.devices_panel()->get_rect();
    const Rect details = app.details_panel()->get_rect();
    CHECK(devices.right() <= details.left());
    CHECK(details.right() <= 120);
    CHECK(details.bottom() <= 40);
    CHECK(app.prepare_button()->get_rect().right() <= details.right());

    app.handle_key(Key::F9);
    CHECK(app.state().rail_expanded);
    CHECK_EQ(app.rail_slot()->get_custom_minimum_size().x, 18);
    root->update_layout(Rect::from_size(Vec2{120, 40}));
    CHECK_EQ(app.rail_slot()->get_rect().size.x, 18);
    CHECK(app.split()->get_orientation() ==
          guayaba::SplitOrientation::VerticalDivider);

    guayaba::ConsoleRenderer renderer(Vec2{120, 40}, nullptr);
    root->draw(renderer, Rect::from_size(Vec2{120, 40}));
}

void test_no_privileges() {
    FakePrepController controller;
    controller.set_can_write(false, "Sin permisos para escribir /dev/sdb.");
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    CHECK(!app.state().can_write);
    CHECK(app.prepare_button()->is_disabled());
    CHECK(app.status_label()->get_text().find("permisos") != std::string::npos);

    app.handle_key(Key::ENTER);
    CHECK(app.state().screen == UiScreen::Main);
    CHECK(app.status_label()->get_text().find("permisos") != std::string::npos);
}

void test_fake_flow_cancel_during_write() {
    FakePrepController controller;
    controller.set_manual_steps(true);
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    CHECK_EQ(app.state().selected, std::size_t{0});
    app.handle_key(Key::DOWN);
    CHECK_EQ(app.state().selected, std::size_t{1});
    app.handle_key(Key::UP);
    CHECK_EQ(app.state().selected, std::size_t{0});

    app.handle_key(Key::ENTER);
    app.settle_transition();
    CHECK(app.state().screen == UiScreen::Confirm);
    CHECK(!app.warning_checkbox()->is_checked());

    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK(app.confirm_panel()->get_rect().right() <= 80);
    CHECK(app.confirm_panel()->get_rect().bottom() <= 24);
    CHECK(app.confirm_kicker()->get_text().find("CONFIRMAR") !=
          std::string::npos);

    // Sin la casilla, Enter no abre el dispositivo ni inicia el flujo.
    app.handle_key(Key::ENTER);
    CHECK(app.state().screen == UiScreen::Confirm);

    app.set_confirmed(true);
    CHECK(app.warning_checkbox()->is_checked());
    app.handle_key(Key::ENTER);
    CHECK(app.state().screen == UiScreen::Progress);

    controller.advance();
    controller.advance();
    controller.advance();
    app.tick();
    CHECK_EQ(app.state().progress.stage, PrepStage::Writing);

    // Las tarjetas no avanzan solas: el usuario las recorre con Continuar.
    CHECK(app.step_title()->get_text() == std::string("Particionar"));
    app.advance_step(); // Particionar -> Formatear
    app.advance_step(); // Formatear -> Escribir
    CHECK(app.step_title()->get_text() == std::string("Escribir"));
    CHECK_EQ(app.progress_bar()->get_value(), 55.0);

    app.settle_transition();
    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK(app.step_panel()->get_rect().right() <= 80);
    CHECK(app.step_panel()->get_rect().bottom() <= 24);
    CHECK(app.cancel_button()->get_rect().right() <= 80);
    CHECK(app.cancel_button()->get_rect().bottom() <= 24);

    app.handle_key(Key::ESCAPE);
    app.settle_transition();
    CHECK(app.state().screen == UiScreen::Result);
    CHECK(app.state().result.outcome == PrepOutcome::Incomplete);
    CHECK_EQ(booteable::ui::outcome_color(app.state().result.outcome),
             std::string("BRIGHT_YELLOW"));
    CHECK_EQ(app.result_outcome()->get_text(), std::string("INCOMPLETO"));

    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK(app.result_panel()->get_rect().right() <= 80);
    CHECK(app.result_panel()->get_rect().bottom() <= 24);

    app.handle_key(Key::ESCAPE);
    app.settle_transition();
    CHECK(app.state().screen == UiScreen::Main);
}

void test_fake_flow_verified() {
    FakePrepController controller;
    controller.set_manual_steps(true);
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    start_flow(app);
    CHECK(app.state().screen == UiScreen::Progress);

    for (int step = 0; step < 6; ++step) {
        controller.advance();
        app.tick();
    }

    // El resultado ya está listo, pero se muestra al terminar de leer los pasos.
    CHECK(app.state().screen == UiScreen::Progress);
    CHECK(app.result_ready());
    while (app.state().screen == UiScreen::Progress) {
        CHECK(!app.continue_button()->is_disabled());
        app.advance_step();
        app.settle_transition();
    }

    CHECK(app.state().screen == UiScreen::Result);
    CHECK(app.state().result.outcome == PrepOutcome::Verified);
    CHECK_EQ(app.result_outcome()->get_text(), std::string("VERIFICADO"));
    CHECK(app.state().result.verified);
    CHECK_EQ(app.progress_bar()->get_value(), 100.0);
}

void test_hybrid_pause_before_write() {
    FakePrepController controller;
    controller.set_manual_steps(true);
    controller.set_wait_before_write(true);
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    start_flow(app);
    app.settle_transition();
    CHECK(app.state().screen == UiScreen::Progress);
    CHECK(app.state().progress.waiting);
    CHECK(controller.waiting_for_continue());
    CHECK(app.step_title()->get_text() == std::string("Particionar"));
    CHECK(!app.continue_button()->is_disabled());

    app.advance_step(); // Particionar -> Formatear
    app.settle_transition();
    CHECK(app.step_title()->get_text() == std::string("Formatear"));
    CHECK_EQ(app.continue_button()->get_text(),
             std::string("[ Continuar y escribir ]"));
    CHECK(!app.continue_button()->is_disabled());

    app.advance_step(); // libera la pausa y pasa a Escribir
    app.settle_transition();
    CHECK(!controller.waiting_for_continue());
    CHECK(app.step_title()->get_text() == std::string("Escribir"));

    controller.advance(); // Building -> Unmounting
    app.tick();
    CHECK_EQ(app.state().progress.stage, PrepStage::Unmounting);
    CHECK(app.step_title()->get_text() == std::string("Escribir"));
}

void test_rejected_device_does_not_open_dialog() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    app.select(2); // NVMe interno: rechazado por la política.
    CHECK_EQ(app.state().selected, std::size_t{2});
    CHECK(app.state().safety.verdict == SafetyVerdict::Rejected);
    app.handle_key(Key::ENTER);
    CHECK(app.state().screen == UiScreen::Main);
}

void test_split_ratio_persists() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = layout(app, 120, 40);
    CHECK(root != nullptr);

    app.split()->set_split_ratio(0.7f);
    root->update_layout(Rect::from_size(Vec2{120, 40}));
    CHECK(std::fabs(app.split()->get_split_ratio() - 0.7f) < 0.001f);

    root->update_layout(Rect::from_size(Vec2{120, 40}));
    CHECK(std::fabs(app.split()->get_split_ratio() - 0.7f) < 0.001f);
}

void test_confirmation_is_full_screen_and_uses_snapshot() {
    FakePrepController controller;
    controller.set_manual_steps(true);
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    app.handle_key(Key::ENTER);
    CHECK(app.state().screen == UiScreen::Confirm);

    app.settle_transition();
    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK(!app.main_panel()->is_visible());
    CHECK(app.confirm_panel()->is_visible());
    CHECK(app.confirm_panel()->get_rect().right() <= 80);
    CHECK(app.confirm_panel()->get_rect().bottom() <= 24);

    const std::size_t selected = app.state().selected;
    app.select(2); // Debe ignorarse: la confirmación es una pantalla propia.
    CHECK_EQ(app.state().selected, selected);

    const std::string summary = app.warning_checkbox()->get_text();
    CHECK(summary.find("/dev/sdb") != std::string::npos);

    app.handle_key(Key::ESCAPE);
    app.settle_transition();
    CHECK(app.state().screen == UiScreen::Main);
    CHECK(app.main_panel()->is_visible());

    // El flujo usa la instantánea del destino, no una selección posterior.
    start_flow(app);
    CHECK(app.state().screen == UiScreen::Progress);
    CHECK(app.step_title()->get_text() == std::string("Particionar"));
    CHECK(app.step_log()->get_text().find("/dev/sdb") != std::string::npos);
}

void test_disabled_button_keeps_frame() {
    FakePrepController controller;
    controller.set_can_write(false, "Sin permisos para escribir /dev/sdb.");
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    CHECK(app.prepare_button()->is_disabled());
    CHECK(app.prepare_button()
              ->get_stylebox(guayaba::ButtonState::Disabled)
              .has_border());
}

void test_export_goes_through_controller() {
    FakePrepController controller;
    controller.set_can_write(false, "Sin permisos para escribir /dev/sdb.");
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    app.request_export();
    CHECK(controller.has_export());
    CHECK(controller.last_export_geometry() ==
          booteable::ExportGeometry::Selected);
    CHECK_EQ(controller.last_export().device.path, std::string("/dev/sdb"));
    CHECK(app.state().status.find("Imagen exportada") != std::string::npos);
}

void test_export_page_standard_and_selected() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    app.handle_key(Key::F8);
    CHECK(app.state().export_page);
    CHECK(app.state().rail_expanded);
    CHECK(app.export_standard_button()->is_visible());
    CHECK(!app.export_selected_button()->is_disabled());

    app.request_export_standard();
    CHECK(controller.has_export());
    CHECK(controller.last_export_geometry() ==
          booteable::ExportGeometry::Standard);
    CHECK_EQ(controller.last_export().device.size_bytes,
             booteable::kMib * 64ull);
    CHECK_EQ(controller.last_export().device.logical_sector, 512u);
    CHECK(app.state().status.find("64 MiB") != std::string::npos);
    CHECK(!app.state().export_page);

    app.handle_key(Key::F8);
    CHECK(app.state().export_page);
    app.request_export_selected();
    CHECK(controller.last_export_geometry() ==
          booteable::ExportGeometry::Selected);
    CHECK(app.state().status.find("Imagen exportada") != std::string::npos);
}

void test_export_selected_disabled_without_devices() {
    FakePrepController controller({});
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    app.handle_key(Key::F8);
    CHECK(app.state().export_page);
    CHECK(app.export_selected_button()->is_disabled());
    CHECK(!app.export_standard_button()->is_disabled());

    app.request_export_standard();
    CHECK(controller.has_export());
    CHECK(controller.last_export_geometry() ==
          booteable::ExportGeometry::Standard);
}

void test_rail_toggle_updates_stage_width() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    CHECK_EQ(app.rail_slot()->get_rect().size.x, 5);
    CHECK_EQ(app.stage()->get_rect().size.x, 75);

    app.handle_key(Key::F9);
    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK_EQ(app.rail_slot()->get_rect().size.x, 18);
    CHECK_EQ(app.stage()->get_rect().size.x, 62);
    CHECK(app.split()->get_orientation() ==
          guayaba::SplitOrientation::HorizontalDivider);

    app.handle_key(Key::F9);
    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK_EQ(app.rail_slot()->get_rect().size.x, 5);
    CHECK_EQ(app.stage()->get_rect().size.x, 75);
}

void test_expanded_rail_keeps_screens_inside() {
    FakePrepController controller;
    controller.set_manual_steps(true);
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    app.handle_key(Key::F9);
    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK_EQ(app.rail_slot()->get_rect().size.x, 18);
    CHECK_EQ(app.stage()->get_rect().size.x, 62);

    app.handle_key(Key::ENTER);
    app.settle_transition();
    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK(app.confirm_panel()->get_rect().left() >= 18);
    CHECK(app.confirm_panel()->get_rect().right() <= 80);
    CHECK(app.confirm_panel()->get_rect().bottom() <= 24);

    app.set_confirmed(true);
    app.handle_key(Key::ENTER);
    app.settle_transition();
    root->update_layout(Rect::from_size(Vec2{80, 24}));
    CHECK(app.step_panel()->get_rect().bottom() <= 24);
    CHECK(app.cancel_button()->get_rect().left() >= 18);
    CHECK(app.cancel_button()->get_rect().right() <= 80);
    CHECK(app.cancel_button()->get_rect().bottom() <= 24);
}

void test_time_based_partition_and_format() {
    FakePrepController controller;
    controller.set_manual_steps(true);
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    start_flow(app);
    CHECK(app.step_title()->get_text() == std::string("Particionar"));

    app.animate(0.3);
    CHECK(app.step_title()->get_text() == std::string("Particionar"));
    CHECK(app.continue_button()->is_disabled());

    app.animate(0.4);
    // El temporizador marca el paso como listo, pero no cambia la tarjeta.
    CHECK(app.step_title()->get_text() == std::string("Particionar"));
    CHECK(!app.continue_button()->is_disabled());

    app.advance_step();
    CHECK(app.step_title()->get_text() == std::string("Formatear"));
    CHECK(app.continue_button()->is_disabled());

    // Una etapa real pisa el temporizador de inmediato.
    controller.advance(); // Building -> Unmounting
    app.tick();
    CHECK(!app.continue_button()->is_disabled());
    app.advance_step();
    CHECK(app.step_title()->get_text() == std::string("Escribir"));
}

void test_building_failure_marks_image_phase() {
    FakePrepController controller;
    controller.fail_next(PrepOutcome::BuildError, PrepStage::Building,
                         "falló la GPT simulada");
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    start_flow(app);
    app.settle_transition();
    CHECK(app.state().screen == UiScreen::Result);
    CHECK(app.state().result.outcome == PrepOutcome::BuildError);
    CHECK(app.result_kicker()->get_text().find("Particionar") !=
          std::string::npos);
}

void test_transition_settles_without_overlap() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    app.handle_key(Key::ENTER);
    CHECK(app.state().screen == UiScreen::Confirm);
    CHECK(app.main_panel()->is_visible());
    CHECK(app.confirm_panel()->is_visible());

    app.settle_transition();
    CHECK(!app.main_panel()->is_visible());
    CHECK(app.confirm_panel()->is_visible());
    CHECK_EQ(app.confirm_panel()->get_anchor_left(), 0.0f);
    CHECK_EQ(app.confirm_panel()->get_anchor_right(), 1.0f);
}

void test_f10_notice_survives_tick() {
    FakePrepController controller;
    controller.set_manual_steps(true);
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    start_flow(app);
    CHECK(app.state().screen == UiScreen::Progress);

    app.handle_key(Key::F10);
    CHECK(app.state().screen == UiScreen::Progress);
    CHECK(app.step_status()->get_text().find("cancelá") != std::string::npos);

    app.tick();
    CHECK(app.step_status()->get_text().find("cancelá") != std::string::npos);
}

void test_no_white_background_leaks() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = app.build();
    root->update_layout(Rect::from_size(Vec2{120, 40}));
    app.settle_transition();

    guayaba::SceneManager& manager = guayaba::SceneManager::get_instance();
    manager.change_scene(std::move(root));
    CapturingTerminal terminal;
    guayaba::ConsoleRenderer renderer(Vec2{120, 40}, &terminal);

    manager.run_frame(renderer, {});
    CHECK_EQ(terminal.buffer.size(), std::size_t{120 * 40});
    CHECK(!terminal.has_white_cell());

    // La pantalla de progreso es la que más huecos deja entre paneles, labels y
    // márgenes; el fondo único debe cubrirla igual.
    app.handle_key(Key::ENTER);
    app.set_confirmed(true);
    app.handle_key(Key::ENTER);
    app.settle_transition();
    manager.run_frame(renderer, {});
    CHECK(!terminal.has_white_cell());

    manager.take_scene();
    guayaba::FocusManager::get_instance().clear();
}

void test_modal_blocks_list_clicks() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = app.build();
    root->update_layout(Rect::from_size(Vec2{80, 24}));
    app.settle_transition();

    guayaba::SceneManager& manager = guayaba::SceneManager::get_instance();
    manager.change_scene(std::move(root));
    guayaba::ConsoleRenderer renderer(Vec2{80, 24}, nullptr);
    manager.run_frame(renderer, {});

    const Rect list_rect = app.device_list()->get_rect();
    CHECK(list_rect.size.x > 0);
    CHECK(list_rect.size.y > 0);
    const Vec2 click{list_rect.pos.x + 1, list_rect.pos.y + 1};

    app.handle_key(Key::ENTER);
    app.settle_transition();
    CHECK(app.state().screen == UiScreen::Confirm);
    const std::size_t selected = app.state().selected;

    guayaba::InputEvent press;
    press.type = guayaba::InputEvent::Type::MOUSE_BUTTON;
    press.mouse_button = guayaba::MouseButton::LEFT;
    press.pressed = true;
    press.mouse_pos = click;

    manager.run_frame(renderer, {press});
    CHECK_EQ(app.state().selected, selected);

    manager.take_scene();
    guayaba::FocusManager::get_instance().clear();
}

} // namespace

void run_ui_flow_tests() {
    test_format();
    test_narrow_layout();
    test_wide_layout_and_expanded_rail();
    test_no_privileges();
    test_fake_flow_cancel_during_write();
    test_fake_flow_verified();
    test_hybrid_pause_before_write();
    test_rejected_device_does_not_open_dialog();
    test_split_ratio_persists();
    test_confirmation_is_full_screen_and_uses_snapshot();
    test_disabled_button_keeps_frame();
    test_export_goes_through_controller();
    test_export_page_standard_and_selected();
    test_export_selected_disabled_without_devices();
    test_rail_toggle_updates_stage_width();
    test_expanded_rail_keeps_screens_inside();
    test_time_based_partition_and_format();
    test_building_failure_marks_image_phase();
    test_transition_settles_without_overlap();
    test_f10_notice_survives_tick();
    test_no_white_background_leaks();
    test_modal_blocks_list_clicks();
}

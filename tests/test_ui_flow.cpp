#include <cstddef>
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
using guayaba::Key;
using guayaba::Rect;
using guayaba::Vec2;

std::unique_ptr<guayaba::Node> layout(AppScene& app, int width, int height) {
    auto root = app.build();
    root->update_layout(Rect::from_size(Vec2{width, height}));
    return root;
}

void test_format() {
    using booteable::ui::format_bytes;
    using booteable::ui::outcome_color;
    using booteable::ui::outcome_label;
    using booteable::ui::progress_step;

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
    CHECK_EQ(progress_step(PrepStage::Writing, PrepStage::Verifying),
             std::string("[ ] Verificando la relectura"));
}

void test_narrow_layout() {
    FakePrepController controller;
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    CHECK(app.split()->get_orientation() ==
          guayaba::SplitOrientation::HorizontalDivider);
    const Rect devices = app.devices_panel()->get_rect();
    const Rect details = app.details_panel()->get_rect();
    CHECK(devices.bottom() <= details.top());
    CHECK(devices.right() <= 80);
    CHECK(details.right() <= 80);
    CHECK(details.bottom() <= 24);

    const Rect prepare = app.prepare_button()->get_rect();
    const Rect export_button = app.export_button()->get_rect();
    CHECK(prepare.right() <= 80);
    CHECK(export_button.right() <= 80);
    CHECK(prepare.bottom() <= 24);

    CHECK_EQ(app.device_list()->item_count(), std::size_t{4});

    guayaba::ConsoleRenderer renderer(Vec2{80, 24}, nullptr);
    root->draw(renderer, Rect::from_size(Vec2{80, 24}));
}

void test_wide_layout() {
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
    app.handle_key(Key::ENTER);
    CHECK(app.state().screen == UiScreen::Main);
    CHECK(app.state().status.find("permisos") != std::string::npos);
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
    CHECK(app.state().screen == UiScreen::Confirm);
    CHECK(!app.warning_checkbox()->is_checked());

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
    CHECK_EQ(app.progress_bar()->get_value(), 55.0);

    app.handle_key(Key::ESCAPE);
    CHECK(app.state().screen == UiScreen::Result);
    CHECK(app.state().result.outcome == PrepOutcome::Incomplete);
    CHECK_EQ(booteable::ui::outcome_color(app.state().result.outcome),
             std::string("BRIGHT_YELLOW"));
    CHECK_EQ(app.result_outcome()->get_text(), std::string("INCOMPLETO"));

    app.handle_key(Key::ESCAPE);
    CHECK(app.state().screen == UiScreen::Main);
}

void test_fake_flow_verified() {
    FakePrepController controller;
    controller.set_manual_steps(true);
    AppScene app(controller);
    auto root = layout(app, 80, 24);
    CHECK(root != nullptr);

    app.handle_key(Key::ENTER);
    app.set_confirmed(true);
    app.handle_key(Key::ENTER);
    CHECK(app.state().screen == UiScreen::Progress);

    for (int step = 0; step < 6; ++step) {
        controller.advance();
        app.tick();
    }

    CHECK(app.state().screen == UiScreen::Result);
    CHECK(app.state().result.outcome == PrepOutcome::Verified);
    CHECK_EQ(app.result_outcome()->get_text(), std::string("VERIFICADO"));
    CHECK_EQ(app.state().result.outcome, PrepOutcome::Verified);
    CHECK(app.state().result.verified);
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

} // namespace

void run_ui_flow_tests() {
    test_format();
    test_narrow_layout();
    test_wide_layout();
    test_no_privileges();
    test_fake_flow_cancel_during_write();
    test_fake_flow_verified();
    test_rejected_device_does_not_open_dialog();
}

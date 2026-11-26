#include "ui/ResultScene.hpp"

#include <string>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::ButtonState;
using guayaba::Control;
using guayaba::HBox;
using guayaba::Label;
using guayaba::NodeRef;
using guayaba::PanelContainer;
using guayaba::StyleBorder;
using guayaba::StyleBox;
using guayaba::VBox;
using guayaba::Vec2;

NodeRef<Label> add_row(const NodeRef<Control>& parent, const std::string& name,
                       const std::string& text, int height = 1) {
    NodeRef<Label> label = parent->add_child<Label>(name);
    label->set_text(text);
    label->set_custom_minimum_size(Vec2{0, height});
    label->set_anchor_right(1.0f);
    label->set_grow_x(guayaba::Grow::Both);
    label->set_clip_children(true);
    return label;
}

void style_button(const NodeRef<Button>& button) {
    StyleBox& normal = button->get_stylebox(ButtonState::Normal);
    normal.set_border_style(StyleBorder::ASCII);
    normal.set_border_color("BRIGHT_BLACK");
    normal.set_border_width(1);
    normal.set_content_margin(1, 0, 1, 0);

    StyleBox& hover = button->get_stylebox(ButtonState::Hover);
    hover.set_border_style(StyleBorder::ASCII);
    hover.set_border_color("BRIGHT_CYAN");
    hover.set_border_width(1);
    hover.set_content_margin(1, 0, 1, 0);
    hover.set_background_color("BRIGHT_BLACK");
}

} // namespace

ResultSceneWidgets build_result_scene(
    guayaba::NodeRef<guayaba::Control> parent, AppScene& app) {
    ResultSceneWidgets widgets;

    widgets.panel = parent->add_child<PanelContainer>("result");
    widgets.panel->set_anchor_left(0.06f);
    widgets.panel->set_anchor_right(0.94f);
    widgets.panel->set_anchor_top(0.05f);
    widgets.panel->set_anchor_bottom(0.95f);
    StyleBox& box = widgets.panel->get_stylebox();
    box.set_background_color("BLACK");
    box.set_border_style(StyleBorder::DOUBLE);
    box.set_border_color("BRIGHT_CYAN");
    box.set_border_width(1);
    box.set_content_margin(1, 0, 1, 0);

    NodeRef<VBox> layout = widgets.panel->add_child<VBox>("result_layout");
    layout->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    layout->set_separation(0);

    widgets.outcome = add_row(layout, "result_outcome", "SIN RESULTADO");
    widgets.outcome->set_text_color("BRIGHT_WHITE");

    widgets.stage = add_row(layout, "result_stage", "Etapa: -");
    widgets.stage->set_text_color("BRIGHT_WHITE");

    widgets.message =
        add_row(layout, "result_message", "", 2);
    widgets.message->set_text_color("BRIGHT_WHITE");

    widgets.details = add_row(layout, "result_details", "");
    widgets.details->set_text_color("BRIGHT_BLACK");

    widgets.hashes = add_row(layout, "result_hashes", "", 2);
    widgets.hashes->set_text_color("BRIGHT_BLACK");

    widgets.commands = add_row(
        layout, "result_commands",
        "Prueba virtual:  ./tools/run-qemu.sh --headless --image build/booteable.img\n"
        "Prueba física:   reiniciar, abrir el menú de arranque y elegir la USB\n"
        "Secure Boot:     debe estar desactivado en el equipo de prueba",
        3);
    widgets.commands->set_text_color("BRIGHT_CYAN");

    NodeRef<HBox> buttons = layout->add_child<HBox>("result_buttons");
    buttons->set_custom_minimum_size(Vec2{0, 3});
    buttons->set_separation(2);

    widgets.back = buttons->add_child<Button>("result_back");
    widgets.back->set_text("[ Volver ]");
    widgets.back->set_custom_minimum_size(Vec2{12, 3});
    style_button(widgets.back);

    widgets.quit = buttons->add_child<Button>("result_quit");
    widgets.quit->set_text("[ Salir ]");
    widgets.quit->set_custom_minimum_size(Vec2{10, 3});
    style_button(widgets.quit);

    widgets.back->connect_signal("clicked", [&app] { app.leave_result(); });
    widgets.quit->connect_signal("clicked", [&app] { app.quit(); });

    return widgets;
}

void refresh_result_scene(ResultSceneWidgets& widgets, const DeviceInfo& device,
                          const PrepResult& result) {
    widgets.outcome->set_text(outcome_label(result.outcome));
    widgets.outcome->set_text_color(outcome_color(result.outcome));

    const PrepStage stage =
        result.outcome == PrepOutcome::Verified ? PrepStage::Done
                                                : result.failed_stage;
    widgets.stage->set_text("Destino: " + device.path + " | Etapa: " +
                            stage_label(stage));
    widgets.message->set_text(result.message.empty() ? "(sin mensaje)"
                                                     : result.message);

    widgets.details->set_text(
        "Bytes escritos: " + format_bytes(result.bytes_written) +
        " | Sincronizado: " + (result.synchronized ? "sí" : "no") +
        " | Verificado: " + (result.verified ? "sí" : "no"));

    widgets.hashes->set_text(
        "Imagen SHA-256: " + short_hash(result.image_sha256) +
        "\nDispositivo SHA-256: " + short_hash(result.device_sha256));
}

} // namespace booteable::ui

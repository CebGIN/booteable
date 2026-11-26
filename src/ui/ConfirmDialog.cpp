#include "ui/ConfirmDialog.hpp"

#include <string>

#include "ui/AppScene.hpp"
#include "ui/UiFormat.hpp"

namespace booteable::ui {
namespace {

using guayaba::Button;
using guayaba::ButtonState;
using guayaba::CheckBox;
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

void style_button(const NodeRef<Button>& button, bool danger) {
    StyleBox& normal = button->get_stylebox(ButtonState::Normal);
    normal.set_border_style(StyleBorder::ASCII);
    normal.set_border_color(danger ? "BRIGHT_RED" : "BRIGHT_BLACK");
    normal.set_border_width(1);
    normal.set_content_margin(1, 0, 1, 0);

    StyleBox& hover = button->get_stylebox(ButtonState::Hover);
    hover.set_border_style(StyleBorder::ASCII);
    hover.set_border_color(danger ? "BRIGHT_RED" : "BRIGHT_CYAN");
    hover.set_border_width(1);
    hover.set_content_margin(1, 0, 1, 0);
    hover.set_background_color("BRIGHT_BLACK");
}

} // namespace

ConfirmDialogWidgets build_confirm_dialog(
    guayaba::NodeRef<guayaba::Control> parent, AppScene& app) {
    ConfirmDialogWidgets widgets;

    widgets.panel = parent->add_child<PanelContainer>("confirm");
    widgets.panel->set_anchor_left(0.08f);
    widgets.panel->set_anchor_right(0.92f);
    widgets.panel->set_anchor_top(0.10f);
    widgets.panel->set_anchor_bottom(0.90f);
    StyleBox& box = widgets.panel->get_stylebox();
    box.set_background_color("BLACK");
    box.set_border_style(StyleBorder::DOUBLE);
    box.set_border_color("BRIGHT_RED");
    box.set_border_width(1);
    box.set_content_margin(1, 0, 1, 0);

    NodeRef<VBox> layout = widgets.panel->add_child<VBox>("confirm_layout");
    layout->set_anchor_preset(guayaba::AnchorPreset::FullRect);
    layout->set_separation(0);

    widgets.title = add_row(layout, "confirm_title", "Confirmar sobrescritura");
    widgets.title->set_text_color("BRIGHT_RED");

    widgets.summary =
        add_row(layout, "confirm_summary", "Destino: -", 2);
    widgets.summary->set_text_color("BRIGHT_WHITE");

    widgets.warning = add_row(
        layout, "confirm_warning",
        "Se perderá todo el contenido del dispositivo.", 2);
    widgets.warning->set_text_color("BRIGHT_RED");

    widgets.checkbox = layout->add_child<CheckBox>("confirm_checkbox");
    widgets.checkbox->set_text("Entiendo que se perderá todo el contenido");
    widgets.checkbox->set_custom_minimum_size(Vec2{0, 1});
    widgets.checkbox->set_anchor_right(1.0f);
    widgets.checkbox->set_grow_x(guayaba::Grow::Both);
    widgets.checkbox->set_text_color(
        guayaba::Color::from_name("BRIGHT_YELLOW"));

    NodeRef<HBox> buttons = layout->add_child<HBox>("confirm_buttons");
    buttons->set_custom_minimum_size(Vec2{0, 3});
    buttons->set_separation(2);

    widgets.cancel = buttons->add_child<Button>("confirm_cancel");
    widgets.cancel->set_text("[ Cancelar ]");
    widgets.cancel->set_custom_minimum_size(Vec2{14, 3});
    style_button(widgets.cancel, false);

    widgets.overwrite = buttons->add_child<Button>("confirm_overwrite");
    widgets.overwrite->set_text("[ Sobrescribir ]");
    widgets.overwrite->set_custom_minimum_size(Vec2{18, 3});
    style_button(widgets.overwrite, true);

    widgets.status = add_row(layout, "confirm_status", "");
    widgets.status->set_text_color("BRIGHT_YELLOW");

    widgets.checkbox->connect_signal<bool>(
        "toggled", [&app](bool checked) { app.set_confirmed(checked); });
    widgets.cancel->connect_signal("clicked",
                                   [&app] { app.dismiss_confirmation(); });
    widgets.overwrite->connect_signal(
        "clicked", [&app] { app.accept_confirmation(); });

    return widgets;
}

void refresh_confirm_dialog(ConfirmDialogWidgets& widgets,
                            const DeviceInfo& device, bool confirmed,
                            const std::string& status) {
    widgets.summary->set_text(
        "Destino: " + device.path + " | " +
        (device.model.empty() ? "USB" : device.model) + " | " +
        format_bytes(device.size_bytes));
    widgets.warning->set_text(
        "Se perderá todo el contenido de " + device.path +
        ". Esta acción no se puede deshacer.");
    widgets.checkbox->set_text("Entiendo que se perderá todo el contenido de " +
                               device.path);
    widgets.checkbox->set_checked(confirmed);
    widgets.status->set_text(status);
}

} // namespace booteable::ui
